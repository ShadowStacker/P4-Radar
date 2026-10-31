#pragma once

namespace ui {

/**
 * "What's new" after an update: the changelog notes since the version the
 * radar ran before, shown once (call from the UI timer; it waits until the
 * radar is set up and on screen). Before v1.21 no version was remembered:
 * then only this version's notes are shown.
 */
void maybeShowWhatsNew();

/** The notes for this version and the two before it (About → What's new). */
void showWhatsNew();

}  // namespace ui
