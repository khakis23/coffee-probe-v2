#include <array>
#include <cstdint>
#include <span>
#include <doctest/doctest.h>

#include "core/button.hpp"

using core::ButtonEvent;
using core::Debouncer;

// Every expected value below is computed by hand in the comment next to it.

namespace {

struct Step {
    bool raw_pressed{false};
    uint32_t now_ms{0};
};

struct EventRecord {
    uint32_t time_ms{0};
    ButtonEvent event{ButtonEvent::Short};
};

template <std::size_t MaxEvents = 16>
struct EventLog {
    std::array<EventRecord, MaxEvents> events{};
    std::size_t count{0};

    void record(uint32_t time_ms, ButtonEvent ev) {
        if (count < MaxEvents) {
            events[count++] = {time_ms, ev};
        }
    }
};

template <std::size_t MaxEvents = 16>
EventLog<MaxEvents> run_sequence(Debouncer& debouncer, std::span<const Step> sequence) {
    EventLog<MaxEvents> log{};
    for (const auto& sample : sequence) {
        const auto ev = debouncer.update(sample.raw_pressed, sample.now_ms);
        if (ev.has_value()) {
            log.record(sample.now_ms, *ev);
        }
    }
    return log;
}

}  // namespace

TEST_CASE("Clean short press debounces edges and emits Short on release acceptance") {
    Debouncer debouncer{};
    std::array<Step, 26> steps{};
    std::size_t idx = 0;

    // Press begins at t = 0 ms, holds through t = 190 ms in 10 ms steps.
    for (uint32_t t = 0; t < 200; t += 10) {
        steps[idx++] = {.raw_pressed = true, .now_ms = t};
    }
    // Release begins at t = 200 ms, holds through t = 250 ms in 10 ms steps.
    for (uint32_t t = 200; t <= 250; t += 10) {
        steps[idx++] = {.raw_pressed = false, .now_ms = t};
    }

    const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

    // Press accepted at 0 + 20 = 20 ms. Release accepted at 200 + 20 = 220 ms.
    // Elapsed hold duration: 220 - 20 = 200 ms (< 600 ms kShortMaxMs).
    // Exactly one Short event at t = 220 ms, no event on any other call.
    CHECK(log.count == 1);
    CHECK(log.events[0].time_ms == 220);
    CHECK(log.events[0].event == ButtonEvent::Short);
    CHECK_FALSE(debouncer.pressed());
}

TEST_CASE("Contact bounce during press and release transitions emits exactly one Short") {
    Debouncer debouncer{};
    std::array<Step, 261> steps{};
    std::size_t idx = 0;

    // Contact bounce at press: raw toggles every 2 ms for 18 ms (t = 0 to 17 ms, 1 ms polling).
    for (uint32_t t = 0; t < 18; ++t) {
        const bool level = ((t / 2) % 2 == 0);
        steps[idx++] = {.raw_pressed = level, .now_ms = t};
    }
    // Solid press held from t = 18 ms through t = 199 ms.
    for (uint32_t t = 18; t < 200; ++t) {
        steps[idx++] = {.raw_pressed = true, .now_ms = t};
    }
    // Contact bounce at release: raw toggles every 2 ms for 18 ms (t = 200 to 217 ms, 1 ms polling).
    for (uint32_t t = 200; t < 218; ++t) {
        const bool level = !(((t - 200) / 2) % 2 == 0);
        steps[idx++] = {.raw_pressed = level, .now_ms = t};
    }
    // Solid release held from t = 218 ms through t = 260 ms.
    for (uint32_t t = 218; t <= 260; ++t) {
        steps[idx++] = {.raw_pressed = false, .now_ms = t};
    }

    const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

    // Rapid 2 ms toggles never reach the 20 ms debounce threshold and drop-backs cancel.
    // Press accepted after solid press holds: 16 + 20 = 36 ms (press_ms = 36).
    // Release accepted after solid release holds: 216 + 20 = 236 ms.
    // Elapsed hold duration: 236 - 36 = 200 ms (< 600 ms kShortMaxMs).
    // Exactly one Short event produced across the entire sequence.
    CHECK(log.count == 1);
    CHECK(log.events[0].time_ms == 236);
    CHECK(log.events[0].event == ButtonEvent::Short);
    CHECK_FALSE(debouncer.pressed());
}

TEST_CASE("Long press emits Long once after threshold and suppresses Short on release") {
    Debouncer debouncer{};
    std::array<Step, 211> steps{};
    std::size_t idx = 0;

    // Held for 2000 ms in 10 ms steps (t = 0 to 2000 ms).
    for (uint32_t t = 0; t <= 2000; t += 10) {
        steps[idx++] = {.raw_pressed = true, .now_ms = t};
    }
    // Released and held released for 100 ms (t = 2010 to 2100 ms).
    for (uint32_t t = 2010; t <= 2100; t += 10) {
        steps[idx++] = {.raw_pressed = false, .now_ms = t};
    }

    const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

    // Press accepted at 0 + 20 = 20 ms (press_ms = 20).
    // Long threshold is 800 ms: first call with now - press_ms >= 800 is at 20 + 800 = 820 ms.
    // Call at 810 ms: 810 - 20 = 790 < 800 (no event).
    // Subsequent calls while held return no further event (Long fires only once).
    // Release accepted at 2010 + 20 = 2030 ms: Long already fired, so Short is suppressed.
    CHECK(log.count == 1);
    CHECK(log.events[0].time_ms == 820);
    CHECK(log.events[0].event == ButtonEvent::Long);
    CHECK_FALSE(debouncer.pressed());
}

TEST_CASE("Dead zone hold between short max and long threshold emits no event") {
    Debouncer debouncer{};
    std::array<Step, 76> steps{};
    std::size_t idx = 0;

    // Press begins at t = 0 ms, accepted at t = 20 ms (press_ms = 20).
    for (uint32_t t = 0; t < 700; t += 10) {
        steps[idx++] = {.raw_pressed = true, .now_ms = t};
    }
    // Release begins at t = 700 ms, accepted at 700 + 20 = 720 ms (press_ms + 700).
    for (uint32_t t = 700; t <= 750; t += 10) {
        steps[idx++] = {.raw_pressed = false, .now_ms = t};
    }

    const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

    // Press accepted at 0 + 20 = 20 ms. Release accepted at 700 + 20 = 720 ms.
    // Release accepted time - press_ms = 720 - 20 = 700 ms.
    // Not Short: 700 >= 600 ms (kShortMaxMs).
    // Not Long: maximum time before release acceptance was 710 ms; 710 - 20 = 690 < 800 ms.
    // Exactly zero events emitted throughout.
    CHECK(log.count == 0);
    CHECK_FALSE(debouncer.pressed());
}

TEST_CASE("Exact boundaries for short release and long hold thresholds") {
    // Release accepted at press + 599 ms emits Short.
    {
        Debouncer debouncer{};
        // Press accepted at t = 20 ms (press_ms = 20). Release begins at t = 599 ms.
        // Release accepted at 599 + 20 = 619 ms: 619 - 20 = 599 < 600 ms -> Short.
        const Step sequence[] = {
            {.raw_pressed = true, .now_ms = 0},
            {.raw_pressed = true, .now_ms = 20},
            {.raw_pressed = false, .now_ms = 599},
            {.raw_pressed = false, .now_ms = 619},
        };
        const auto log = run_sequence(debouncer, sequence);
        CHECK(log.count == 1);
        CHECK(log.events[0].time_ms == 619);
        CHECK(log.events[0].event == ButtonEvent::Short);
    }

    // Release accepted at press + 600 ms emits nothing.
    {
        Debouncer debouncer{};
        // Press accepted at t = 20 ms (press_ms = 20). Release begins at t = 600 ms.
        // Release accepted at 600 + 20 = 620 ms: 620 - 20 = 600 >= 600 ms -> no event.
        const Step sequence[] = {
            {.raw_pressed = true, .now_ms = 0},
            {.raw_pressed = true, .now_ms = 20},
            {.raw_pressed = false, .now_ms = 600},
            {.raw_pressed = false, .now_ms = 620},
        };
        const auto log = run_sequence(debouncer, sequence);
        CHECK(log.count == 0);
    }

    // Long threshold boundary: press + 799 ms emits nothing; press + 800 ms emits Long.
    {
        Debouncer debouncer{};
        // Press accepted at t = 20 ms (press_ms = 20).
        CHECK(debouncer.update(true, 0) == std::nullopt);
        CHECK(debouncer.update(true, 20) == std::nullopt);
        CHECK(debouncer.pressed());

        // At press + 799 ms (t = 819 ms): 819 - 20 = 799 < 800 ms -> no event.
        CHECK(debouncer.update(true, 819) == std::nullopt);

        // At press + 800 ms (t = 820 ms): 820 - 20 = 800 >= 800 ms -> Long event.
        const auto ev = debouncer.update(true, 820);
        REQUIRE(ev.has_value());
        CHECK(*ev == ButtonEvent::Long);
    }
}

TEST_CASE("Glitch shorter than debounce threshold produces no event and level remains released") {
    Debouncer debouncer{};
    std::array<Step, 41> steps{};
    std::size_t idx = 0;

    // Released at t = 0 ms.
    steps[idx++] = {.raw_pressed = false, .now_ms = 0};

    // Raw pressed glitch lasting 15 ms (t = 1 to 15 ms, 1 ms polling).
    for (uint32_t t = 1; t <= 15; ++t) {
        steps[idx++] = {.raw_pressed = true, .now_ms = t};
    }

    // Raw returned to released level from t = 16 to 40 ms.
    for (uint32_t t = 16; t <= 40; ++t) {
        steps[idx++] = {.raw_pressed = false, .now_ms = t};
    }

    const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

    // Glitch duration 15 - 1 = 14 ms (< 20 ms kDebounceMs).
    // Sample at t = 16 ms matches debounced level (false), cancelling candidate debounce.
    // Debounced state stays released throughout and no event is generated.
    CHECK(log.count == 0);
    CHECK_FALSE(debouncer.pressed());
}

TEST_CASE("Timer rollover across 32-bit integer limit preserves identical events") {
    const uint32_t t0 = UINT32_MAX - 30;

    // Short press identical to clean short press test, offset by t0.
    {
        Debouncer debouncer{};
        std::array<Step, 26> steps{};
        std::size_t idx = 0;

        for (uint32_t offset = 0; offset < 200; offset += 10) {
            steps[idx++] = {.raw_pressed = true, .now_ms = t0 + offset};
        }
        for (uint32_t offset = 200; offset <= 250; offset += 10) {
            steps[idx++] = {.raw_pressed = false, .now_ms = t0 + offset};
        }

        const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

        // Press accepted at t0 + 20 ms. Release accepted at t0 + 220 ms.
        // Unsigned math: (t0 + 220) - (t0 + 20) = 200 ms (< 600 ms).
        // Emits exactly one Short event at t0 + 220 ms (identical to clean short press test).
        CHECK(log.count == 1);
        CHECK(log.events[0].time_ms == t0 + 220);
        CHECK(log.events[0].event == ButtonEvent::Short);
        CHECK_FALSE(debouncer.pressed());
    }

    // Long press identical to long press test, offset by t0.
    {
        Debouncer debouncer{};
        std::array<Step, 211> steps{};
        std::size_t idx = 0;

        for (uint32_t offset = 0; offset <= 2000; offset += 10) {
            steps[idx++] = {.raw_pressed = true, .now_ms = t0 + offset};
        }
        for (uint32_t offset = 2010; offset <= 2100; offset += 10) {
            steps[idx++] = {.raw_pressed = false, .now_ms = t0 + offset};
        }

        const auto log = run_sequence(debouncer, std::span{steps.data(), idx});

        // Press accepted at t0 + 20 ms.
        // Long threshold: (t0 + 820) - (t0 + 20) = 800 ms (>= 800 ms kLongMs).
        // Emits exactly one Long event at t0 + 820 ms, and release produces no Short.
        CHECK(log.count == 1);
        CHECK(log.events[0].time_ms == t0 + 820);
        CHECK(log.events[0].event == ButtonEvent::Long);
        CHECK_FALSE(debouncer.pressed());
    }
}
