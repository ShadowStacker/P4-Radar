#pragma once

#include <functional>
#include <string>

namespace ui {

enum class InputKind { Text, Password, Number };

/**
 * Full-screen text entry laid out for a round panel: title, text field and an
 * on-screen keyboard. `on_done` receives the text when the user presses OK;
 * cancel simply closes the overlay.
 */
void textInput(const char* title, const std::string& initial, InputKind kind,
               std::function<void(const std::string&)> on_done);

}  // namespace ui
