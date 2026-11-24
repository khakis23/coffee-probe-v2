#include "core/ui.hpp"

namespace core::ui {

// Interface stub: the real screens land in the next commit.
Screen choose_screen(const Inputs&) { return Screen::Starting; }
Link link_status(const Inputs&) { return Link::JoinWifi; }
void render(const Inputs&, Framebuffer& fb) { fb.clear(); }

}  // namespace core::ui
