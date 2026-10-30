#include "test_harness.hpp"

#include "dungeon/display/buffer.hpp"

using dungeon::display::FrameBuffer;

DUNGEON_TEST(display, default_framebuffer_has_a_usable_size)
{
    FrameBuffer buffer;

    CHECK(buffer.width() > 0);
    CHECK(buffer.height() > 0);

    buffer.draw(0, 0, 'x');
    buffer.draw(buffer.width() - 1, buffer.height() - 1, 'x');
    buffer.draw(buffer.width(), buffer.height(), 'x');
}

DUNGEON_TEST(display, explicit_framebuffer_uses_the_requested_size)
{
    FrameBuffer buffer(5, 3);

    CHECK_EQ(buffer.width(), 5U);
    CHECK_EQ(buffer.height(), 3U);

    buffer.draw(4, 2, 'x');
    buffer.draw(5, 2, 'x');
}
