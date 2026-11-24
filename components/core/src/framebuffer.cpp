#include "core/framebuffer.hpp"

namespace core {

// Interface stub: the real drawing code lands in the next commit.
void Framebuffer::clear() { bytes_.fill(0); }
void Framebuffer::set_pixel(int, int, bool) {}
bool Framebuffer::pixel(int, int) const { return false; }
void Framebuffer::fill_rect(int, int, int, int, bool) {}
void Framebuffer::hline(int, int, int, bool) {}
int Framebuffer::text(int x, int, std::string_view, int) { return x; }
int Framebuffer::text_width(std::string_view, int) { return 0; }

}  // namespace core
