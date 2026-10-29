// Firmware updates: look up the latest GitHub release, then download this
// board's update image into the idle app slot (network task).

#include <cstdio>
#include <mutex>
#include <string>

#include "app/app.h"
#include "core/update.h"
#include "platform/platform.h"

namespace app {

namespace {

enum class Job : uint8_t { None, Check, Install };

// Besides the About page button, the poller checks by itself a minute after
// start and then hourly (GitHub allows 60 unauthenticated requests an hour).
constexpr uint32_t kFirstAutoCheckMs = 60 * 1000;
constexpr uint32_t kAutoCheckEveryMs = 60 * 60 * 1000;

std::mutex s_mu;  // guards everything below
UpdateStatus s_status;
Job s_job = Job::None;
std::string s_url;  // update image of the release in s_status.latest
bool s_auto_scheduled = false;
uint32_t s_next_auto_ms = 0;

void fail(const std::string& why, bool install) {
  std::lock_guard<std::mutex> lock(s_mu);
  s_status.state = UpdateState::Failed;
  s_status.install_failed = install;
  std::snprintf(s_status.error, sizeof(s_status.error), "%s", why.c_str());
}

/** `quiet`: a background check, which leaves the status alone unless it learns something. */
void runCheck(bool quiet) {
  std::string body;
  const char* const headers[] = {"Accept", "application/vnd.github+json", nullptr};
  const int code = plat::httpGet(core::update::kLatestReleaseUrl, &body, 15000, headers);
  core::update::Release rel;
  if (code != 200 || !core::update::parseLatestRelease(body.data(), body.size(), plat::firmwareVariant(), &rel)) {
    std::printf("update: %s -> HTTP %d\n", core::update::kLatestReleaseUrl, code);
    if (quiet) return;
    fail(code == 403 || code == 429 ? "GitHub is busy - try again in an hour"
         : code < 0                  ? "Couldn't reach GitHub (no internet?)"
                                     : "Couldn't read the release list (HTTP " + std::to_string(code) + ")",
         false);
    return;
  }
  const bool newer = core::update::compareVersions(rel.tag.c_str(), plat::firmwareVersion()) > 0;
  std::printf("update: latest %s, running %s, image %s\n", rel.tag.c_str(), plat::firmwareVersion(),
              rel.asset_url.empty() ? "none" : rel.asset_url.c_str());
  std::lock_guard<std::mutex> lock(s_mu);
  // The user may have started an install meanwhile; don't undo that.
  if (s_status.state == UpdateState::Installing || s_status.state == UpdateState::Installed) return;
  std::snprintf(s_status.latest, sizeof(s_status.latest), "%s", rel.tag.c_str());
  s_status.install_failed = false;
  s_status.error[0] = '\0';
  if (!newer) {
    s_status.state = UpdateState::UpToDate;
  } else if (rel.asset_url.empty()) {
    s_status.state = UpdateState::NoImage;
  } else {
    s_status.state = UpdateState::Available;
    s_url = rel.asset_url;
    s_status.total = rel.asset_size;
  }
}

void runInstall() {
  std::string url;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    url = s_url;
    s_status.done = 0;
  }
  std::string error;
  const bool ok = plat::installFirmware(url.c_str(), [](size_t done, size_t total) {
    std::lock_guard<std::mutex> lock(s_mu);
    s_status.done = done;
    if (total > 0) s_status.total = total;
  }, &error);
  if (!ok) {
    std::printf("update: install failed: %s\n", error.c_str());
    fail(error, true);
    return;
  }
  std::lock_guard<std::mutex> lock(s_mu);
  s_status.state = UpdateState::Installed;
}

}  // namespace

void checkForUpdate() {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_status.state == UpdateState::Installing || s_status.state == UpdateState::Installed) return;
  s_status.state = UpdateState::Checking;
  s_job = Job::Check;
}

void installUpdate() {
  std::lock_guard<std::mutex> lock(s_mu);
  if (s_status.state != UpdateState::Available) return;
  s_status.state = UpdateState::Installing;
  s_job = Job::Install;
}

UpdateStatus updateStatus() {
  std::lock_guard<std::mutex> lock(s_mu);
  return s_status;
}

bool serviceUpdate() {
  Job job;
  bool quiet = false;
  {
    std::lock_guard<std::mutex> lock(s_mu);
    const uint32_t now = plat::millis();
    if (!s_auto_scheduled) {
      s_auto_scheduled = true;
      s_next_auto_ms = now + kFirstAutoCheckMs;
    }
    job = s_job;
    s_job = Job::None;
    const bool idle = s_status.state != UpdateState::Installing && s_status.state != UpdateState::Installed &&
                      s_status.state != UpdateState::Checking;
    if (job == Job::None && idle && static_cast<int32_t>(now - s_next_auto_ms) >= 0) {
      job = Job::Check;
      quiet = true;
    }
    if (job == Job::Check) s_next_auto_ms = now + kAutoCheckEveryMs;  // a manual check counts too
  }
  if (job == Job::Check) runCheck(quiet);
  else if (job == Job::Install) runInstall();
  return job != Job::None;
}

}  // namespace app
