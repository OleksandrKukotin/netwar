// NETWAR conductor console — the playable M2 terminal client.
//
// All game rules live in netwar::Match (sim/). This file only renders the
// match, turns key presses into Match commands, and runs the tick clock.
// The clock thread never touches the match: it posts a closure that the UI
// thread runs, so the simulation stays single-threaded and deterministic.

#include <netwar/match.hpp>
#include <netwar/scenario.hpp>
#include <netwar/tier.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace ftxui;
using netwar::Front;
using netwar::Match;
using netwar::Signal;
using netwar::Tier;

namespace {

// --- Phosphor palette --------------------------------------------------------

const Color kAmber = Color::RGB(255, 176, 0);
const Color kGreen = Color::RGB(80, 250, 120);
const Color kRed = Color::RGB(255, 70, 60);
const Color kDim = Color::RGB(110, 110, 100);
const Color kCyan = Color::RGB(90, 200, 255);
const Color kWhite = Color::RGB(230, 230, 220);

constexpr std::array<int, 7> kTickSpeeds = {250, 500, 750, 1000, 1500, 2000, 3000};
constexpr std::size_t kDefaultSpeed = 3; // 1 s/tick, GDD section 6 target band

constexpr std::size_t kHistory = 24;  // past ticks on the scope
constexpr netwar::Tick kIntel = 20;   // forecast ticks on the scope
constexpr std::size_t kLogLines = 7;

constexpr std::array<Front, 2> kFronts = {Front::West, Front::East};

std::size_t idx(Front f) { return static_cast<std::size_t>(f); }
Front other(Front f) { return f == Front::West ? Front::East : Front::West; }

std::string num(Signal s, int decimals = 1) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, static_cast<double>(s) / netwar::kSignalScale);
    return buf;
}

std::string pad(std::string s, std::size_t width) {
    if (s.size() < width) s.append(width - s.size(), ' ');
    return s;
}

const char* front_name(Front f) { return f == Front::West ? "WEST" : "EAST"; }

const char* tier_name(Tier t) {
    switch (t) {
    case Tier::Directed: return "DIRECTED";
    case Tier::SemiAutonomous: return "SEMI-AUTONOMOUS";
    case Tier::Blackout: return "BLACKOUT";
    }
    return "";
}

Color tier_color(Tier t) {
    switch (t) {
    case Tier::Directed: return kGreen;
    case Tier::SemiAutonomous: return kAmber;
    case Tier::Blackout: return kRed;
    }
    return kWhite;
}

const char* grade_name(netwar::LineGrade g) {
    switch (g) {
    case netwar::LineGrade::Copper: return "COPPER";
    case netwar::LineGrade::Coax: return "COAX";
    case netwar::LineGrade::Fiber: return "FIBER";
    }
    return "";
}

// One column of a sparkline: 0..8 eighths of a cell.
const char* spark(Signal value, Signal max) {
    static const char* const kLevels[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    if (value <= 0 || max <= 0) return kLevels[0];
    const auto level = std::clamp<Signal>((value * 8 + max - 1) / max, 1, 8);
    return kLevels[level];
}

Element meter(float fraction, int width, Color c) {
    return gauge(std::clamp(fraction, 0.0F, 1.0F)) | color(c) | size(WIDTH, EQUAL, width);
}

// --- Game state held by the console -------------------------------------------

struct Sample {
    Signal intensity{};
    Signal relay{};
};

struct LogLine {
    netwar::Tick tick{};
    std::string text;
    Color tint;
};

struct Console {
    std::uint64_t seed{};
    Match match;
    Front selected = Front::West;
    std::array<std::deque<Sample>, 2> history;
    std::deque<LogLine> log;
    bool briefing = true;

    explicit Console(std::uint64_t s) : seed(s), match(netwar::MatchRules{.seed = s}) {
        note("Console online. Two fronts, one hub. Press SPACE to go live.", kCyan);
    }

    void note(std::string text, Color tint) {
        log.push_front({match.tick(), std::move(text), tint});
        while (log.size() > kLogLines) log.pop_back();
    }

    void restart(std::uint64_t s) {
        *this = Console(s);
        briefing = false;
    }

    // --- Player verbs ---

    void upgrade() {
        const Signal cost = match.upgrade_cost(selected);
        if (cost == 0) {
            note(std::string(front_name(selected)) + " line is already fiber.", kDim);
        } else if (match.upgrade_line(selected)) {
            note(std::string(front_name(selected)) + " line upgraded to " +
                     grade_name(match.grade(selected)) + " (-" + num(cost, 0) + " matter).",
                 kCyan);
        } else {
            note("Not enough matter: " + num(cost, 0) + " needed for the " +
                     front_name(selected) + " line.",
                 kDim);
        }
    }

    void nudge_allocation(Signal delta) {
        match.set_allocation(selected, match.allocation(selected) + delta);
    }

    void give_priority() {
        if (match.priority() == selected) return;
        match.set_priority(selected);
        note(std::string(front_name(selected)) + " router takes priority on the hub buffer.", kCyan);
    }

    // --- Clock ---

    void advance() {
        if (match.outcome() != netwar::Outcome::InProgress) return;

        std::array<Tier, 2> tier_before{};
        std::array<bool, 2> flaring_before{};
        for (Front f : kFronts) {
            tier_before[idx(f)] = match.tier(f);
            flaring_before[idx(f)] = match.flaring(f);
        }
        const bool forecast_before = match.storm_forecast();
        const bool storm_before = match.storm_active();

        match.step();

        for (Front f : kFronts) {
            auto& h = history[idx(f)];
            h.push_back({match.intensity(f), match.relay(f)});
            while (h.size() > kHistory) h.pop_front();

            const std::string name = front_name(f);
            if (match.flaring(f) && !flaring_before[idx(f)]) {
                note(name + ": enemy contact, flare peaking at " + num(match.flare_peak(f)) + "/t.",
                     kAmber);
            }
            const Tier now = match.tier(f);
            if (now != tier_before[idx(f)]) {
                const bool worse = now < tier_before[idx(f)];
                note(name + " relay " + (worse ? "browns out -> " : "recovers -> ") + tier_name(now) +
                         ".",
                     tier_color(now));
            }
        }

        if (match.storm_forecast() && !forecast_before) {
            note("Spectrum storm forecast in " + std::to_string(match.storm_start() - match.tick()) +
                     " ticks: relay decay x3. Fill the relays.",
                 kCyan);
        }
        if (match.storm_active() && !storm_before) note("Spectrum storm hits. Relays are leaking.", kRed);
        if (!match.storm_active() && storm_before) note("Storm has passed.", kCyan);

        const auto period = match.rules().escalation_period;
        if (match.tick() % period == 0) {
            note("Enemy escalates: flares grow stronger.", kRed);
        }

        switch (match.outcome()) {
        case netwar::Outcome::Victory: note("BREAKTHROUGH. The line holds.", kGreen); break;
        case netwar::Outcome::Defeat: note("A FRONT HAS COLLAPSED.", kRed); break;
        case netwar::Outcome::InProgress: break;
        }
    }
};

// --- Rendering ------------------------------------------------------------------

Element scope_row(const Console& c, Front f) {
    const Match& m = c.match;
    const Signal max = m.rules().max_amplitude;
    const auto& h = c.history[idx(f)];

    Elements intensity;
    Elements relay;
    for (std::size_t i = h.size(); i < kHistory; ++i) {
        intensity.push_back(text(" "));
        relay.push_back(text(" "));
    }
    for (const Sample& s : h) {
        intensity.push_back(text(spark(s.intensity, max)) | color(kAmber));
        relay.push_back(text(spark(s.relay, m.relay_capacity())) |
                        color(tier_color(netwar::tier_of(s.relay))));
    }
    intensity.push_back(text("│") | color(kWhite));
    relay.push_back(text("│") | color(kWhite));
    for (netwar::Tick ahead = 1; ahead <= kIntel; ++ahead) {
        intensity.push_back(text(spark(m.forecast_intensity(f, ahead), max)) | color(kAmber) | dim);
        relay.push_back(text(" "));
    }

    return vbox({
        hbox({text("WAVE   ") | color(kDim), hbox(std::move(intensity))}),
        hbox({text("LEVEL  ") | color(kDim), hbox(std::move(relay))}),
        hbox({text("       "), text(pad("past", kHistory)) | color(kDim), text(" intel") | color(kDim)}),
    });
}

Element front_panel(const Console& c, Front f) {
    const Match& m = c.match;
    const bool selected = c.selected == f;
    const Tier tier = m.tier(f);

    // Line
    Element upgrade = text("  maxed") | color(kDim);
    if (const Signal cost = m.upgrade_cost(f); cost > 0) {
        const bool affordable = m.matter() >= cost;
        upgrade = text(std::string("  [u] -> ") +
                       (m.grade(f) == netwar::LineGrade::Copper ? "COAX 4/t" : "FIBER 8/t") + " : " +
                       num(cost, 0) + "m") |
                  color(affordable ? kCyan : kDim);
    }
    Element line_row = hbox({text("LINE   ") | color(kDim),
                             text(pad(grade_name(m.grade(f)), 7)) | bold,
                             text(num(m.line_throughput(f), 0) + "/t"), upgrade});

    // Router
    std::string pips;
    const auto alloc = m.allocation(f) / netwar::kSignalScale;
    const auto router_max = m.router_max() / netwar::kSignalScale;
    for (Signal i = 0; i < router_max; ++i) pips += i < alloc ? "■" : "□";
    const bool priority = m.priority() == f;
    Element router_row = hbox({text("ROUTER ") | color(kDim), text(pips) | color(kCyan),
                               text(" " + num(m.allocation(f), 0) + "/t"),
                               priority ? text("  ★ PRIORITY") | color(kAmber) | bold
                                        : text("  [p] priority") | color(kDim)});

    // Relay
    Element relay_row = hbox({
        text("RELAY  ") | color(kDim),
        meter(static_cast<float>(m.relay(f)) / static_cast<float>(m.relay_capacity()), 16,
              tier_color(tier)),
        text(" " + pad(num(m.relay(f)), 5)),
        text(tier_name(tier)) | color(tier_color(tier)) | bold,
    });

    // Enemy
    Element enemy_row;
    if (m.flaring(f)) {
        enemy_row = hbox({text("ENEMY  ") | color(kDim),
                          text("FLARE " + num(m.intensity(f)) + "/t") | color(kAmber) | bold,
                          text("  peak " + num(m.flare_peak(f))) | color(kAmber)});
    } else {
        enemy_row = hbox({text("ENEMY  ") | color(kDim),
                          text("quiet, contact in " + std::to_string(m.ticks_to_flare(f)) + "t"),
                          text("  next peak " + num(m.next_flare_peak(f))) | color(kAmber)});
    }

    // Front line
    const auto hold = m.hold(f);
    const Color hold_color = hold >= 60'000 ? kGreen : hold >= 30'000 ? kAmber : kRed;
    Element hold_row = hbox({
        text("FRONT  ") | color(kDim),
        text("collapse ") | color(kRed) | dim,
        meter(static_cast<float>(hold) / static_cast<float>(netwar::kHoldMax), 20, hold_color),
        text(" breakthrough") | color(kGreen) | dim,
        text("  " + std::to_string(hold / 1000) + "%") | bold | color(hold_color),
    });

    Element body = vbox({line_row, router_row, relay_row, separatorLight(), enemy_row,
                         scope_row(c, f), separatorLight(), hold_row});

    Element title = text(std::string(" ") + front_name(f) + " FRONT " + (selected ? "◀ " : "")) |
                    bold | color(selected ? kWhite : kDim);
    return window(title, body, selected ? DOUBLE : ROUNDED) | color(selected ? kAmber : kDim) | flex;
}

Element header(const Console& c, int tick_ms, bool paused) {
    const Match& m = c.match;
    char speed[32];
    std::snprintf(speed, sizeof speed, "%.2g s/tick", tick_ms / 1000.0);

    Element state = paused ? text(" ❚❚ PAUSED ") | inverted | color(kAmber)
                           : text(" ▶ LIVE ") | inverted | color(kGreen);
    if (m.outcome() == netwar::Outcome::Victory) state = text(" ✔ VICTORY ") | inverted | color(kGreen);
    if (m.outcome() == netwar::Outcome::Defeat) state = text(" ✖ DEFEAT ") | inverted | color(kRed);

    return hbox({
        text(" NETWAR ") | bold | inverted | color(kAmber),
        text(" // CONDUCTOR CONSOLE") | color(kAmber),
        text("  v" NETWAR_VERSION) | color(kDim),
        filler(),
        text("T+" + std::to_string(m.tick())) | bold,
        text("   seed " + std::to_string(c.seed)) | color(kDim),
        text("   " + std::string(speed) + "  ") | color(kCyan),
        state,
    });
}

Element hub_panel(const Console& c) {
    const Match& m = c.match;
    const auto& g = m.engine().graph();
    const Signal gen = g.find(netwar::ids::kCommandHub)->generation;
    const Signal upkeep = g.find(netwar::ids::kUpkeepDrain)->consumption;
    const float fill = static_cast<float>(m.hub_buffer()) / static_cast<float>(m.hub_capacity());

    Element weather = text("SPECTRUM clear") | color(kDim);
    if (m.storm_active()) {
        weather = text("⚡ STORM: relay decay x3, " + std::to_string(m.storm_end() - m.tick()) +
                       "t left") |
                  bold | color(kRed);
    } else if (m.storm_forecast()) {
        weather = text("⚠ STORM in " + std::to_string(m.storm_start() - m.tick()) + "t") | bold |
                  color(kAmber);
    }

    return hbox({
        text(" HUB ") | bold | color(kAmber),
        text("+" + num(gen, 0) + "/t  upkeep -" + num(upkeep, 0) + "/t  BUFFER "),
        meter(fill, 16, fill > 0.95F ? kRed : kCyan),
        text(" " + num(m.hub_buffer(), 0) + "/" + num(m.hub_capacity(), 0)),
        text("  heat " + num(m.engine().wasted_heat(), 0)) | color(kDim),
        filler(),
        text("MATTER ") | bold | color(kAmber),
        text(num(m.matter(), 0)) | bold,
        text(" (+" + num(m.rules().matter_income, 0) + "/t)   ") | color(kDim),
        weather,
        text(" "),
    });
}

Element log_panel(const Console& c) {
    Elements lines;
    for (const LogLine& l : c.log) {
        lines.push_back(hbox({text(pad("T+" + std::to_string(l.tick), 7)) | color(kDim),
                              text(l.text) | color(l.tint)}));
    }
    return vbox(std::move(lines)) | size(HEIGHT, EQUAL, static_cast<int>(kLogLines));
}

Element keys_bar() {
    auto key = [](const std::string& k, const std::string& what) {
        return hbox({text(k) | inverted | color(kAmber), text(" " + what + "  ") | color(kDim)});
    };
    return hflow({key("←→", "front"), key("↑↓", "router"), key("u", "upgrade"), key("p", "priority"),
                  key("space", "pause"), key("+-", "speed"), key("n", "step"), key("h", "help"),
                  key("r", "restart"), key("q", "quit")});
}

Element briefing_box() {
    auto para = [](const std::string& s, Color c = kWhite) { return paragraph(s) | color(c); };
    return window(
               text(" CONDUCTOR BRIEFING ") | bold | color(kAmber),
               vbox({
                   para("You command a base, not an army. Nothing you do touches a soldier - "
                        "your only weapon is command bandwidth.",
                        kAmber),
                   text(""),
                   para("- The HUB makes 12 signal per tick; 3 go to upkeep. Routers push the rest "
                        "down the LINES into each front's RELAY."),
                   para("- The enemy FLARES the fronts in turn. Combat drains the relay."),
                   text("- A relay's level sets its front's command tier:"),
                   hbox({text("    above 15  "), text("DIRECTED       ") | color(kGreen) | bold,
                         text("the front pushes forward")}),
                   hbox({text("    5 to 15   "), text("SEMI-AUTONOMOUS") | color(kAmber) | bold,
                         text(" it slowly gives ground")}),
                   hbox({text("    below 5   "), text("BLACKOUT       ") | color(kRed) | bold,
                         text("it falls back fast")}),
                   para("- Push either front to 100% for a BREAKTHROUGH. Let either reach 0% and "
                        "you lose."),
                   para("- Copper lines carry 1/t. Spend MATTER on coax (4/t) and fiber (8/t). "
                        "Once your lines outgrow the hub, you must choose who gets fed: router "
                        "allocation and priority."),
                   para("- The scope shows enemy intel for the next 20 ticks (dimmed). Fill a "
                        "relay BEFORE its flare. Spectrum storms triple relay decay."),
                   para("- The enemy escalates every 100 ticks. Standing still loses."),
                   text(""),
                   para("Press SPACE to go live. h reopens this briefing.", kCyan),
               }) | size(WIDTH, LESS_THAN, 86)) |
           color(kAmber) | clear_under | center;
}

Element outcome_box(const Console& c) {
    const Match& m = c.match;
    const bool won = m.outcome() == netwar::Outcome::Victory;
    Front decisive = Front::West;
    for (Front f : kFronts) {
        if ((won && m.hold(f) == netwar::kHoldMax) || (!won && m.hold(f) == 0)) decisive = f;
    }
    const std::string headline =
        won ? std::string("BREAKTHROUGH ON THE ") + front_name(decisive) + " FRONT"
            : std::string("THE ") + front_name(decisive) + " FRONT HAS COLLAPSED";
    const Color tint = won ? kGreen : kRed;

    return window(text(won ? " VICTORY " : " DEFEAT ") | bold | color(tint),
                  vbox({
                      text(headline) | bold | color(tint) | hcenter,
                      text(""),
                      text("held for " + std::to_string(m.tick()) + " ticks, " +
                           num(m.engine().wasted_heat(), 0) + " signal vented as heat") |
                          color(kWhite) | hcenter,
                      text(""),
                      text("r  new match      q  quit") | color(kCyan) | hcenter,
                  }) | size(WIDTH, GREATER_THAN, 50)) |
           color(tint) | clear_under | center;
}

} // namespace

int main(int argc, char** argv) {
    std::uint64_t seed = std::random_device{}() % 100000;
    if (argc > 1) seed = std::strtoull(argv[1], nullptr, 10);

    Console console(seed);
    auto screen = ScreenInteractive::Fullscreen();

    std::atomic<std::size_t> speed{kDefaultSpeed};
    std::atomic<bool> paused{true};
    std::atomic<bool> quit{false};

    auto ui = Renderer([&] {
        const int tick_ms = kTickSpeeds[speed];
        Element main = vbox({
            header(console, tick_ms, paused),
            separator() | color(kDim),
            hub_panel(console),
            hbox({front_panel(console, Front::West), front_panel(console, Front::East)}),
            window(text(" SIGNAL LOG ") | color(kDim), log_panel(console)) | color(kDim),
            keys_bar(),
        });
        if (console.briefing) return dbox({main, briefing_box()});
        if (console.match.outcome() != netwar::Outcome::InProgress) {
            return dbox({main, outcome_box(console)});
        }
        return main;
    });

    ui |= CatchEvent([&](const Event& e) {
        if (e == Event::Character('q') || e == Event::Escape) {
            quit = true;
            screen.Exit();
            return true;
        }
        if (e == Event::Character(' ')) {
            console.briefing = false;
            paused = !paused;
            return true;
        }
        if (e == Event::Character('h')) {
            console.briefing = !console.briefing;
            if (console.briefing) paused = true;
            return true;
        }
        if (e == Event::Character('r')) {
            console.restart(std::random_device{}() % 100000);
            paused = true;
            return true;
        }
        if (e == Event::Character('+') || e == Event::Character('=')) {
            if (speed > 0) --speed; // faster = shorter tick
            return true;
        }
        if (e == Event::Character('-') || e == Event::Character('_')) {
            if (speed + 1 < kTickSpeeds.size()) ++speed;
            return true;
        }
        if (e == Event::Character('n')) {
            if (paused) console.advance();
            return true;
        }
        if (e == Event::ArrowLeft || e == Event::Character('a')) {
            console.selected = Front::West;
            return true;
        }
        if (e == Event::ArrowRight || e == Event::Character('d')) {
            console.selected = Front::East;
            return true;
        }
        if (e == Event::Tab) {
            console.selected = other(console.selected);
            return true;
        }
        if (e == Event::ArrowUp || e == Event::Character('w')) {
            console.nudge_allocation(netwar::units(1));
            return true;
        }
        if (e == Event::ArrowDown || e == Event::Character('s')) {
            console.nudge_allocation(-netwar::units(1));
            return true;
        }
        if (e == Event::Character('u')) {
            console.upgrade();
            return true;
        }
        if (e == Event::Character('p')) {
            console.give_priority();
            return true;
        }
        return false;
    });

    // The clock: sleeps in short slices so speed and pause changes apply
    // at once, and hands each tick to the UI thread.
    std::thread clock([&] {
        using namespace std::chrono;
        auto next = steady_clock::now();
        while (!quit) {
            std::this_thread::sleep_for(milliseconds(10));
            const auto now = steady_clock::now();
            if (paused) {
                next = now + milliseconds(kTickSpeeds[speed]);
                continue;
            }
            if (now < next) continue;
            next = now + milliseconds(kTickSpeeds[speed]);
            screen.Post([&] { console.advance(); });
            screen.PostEvent(Event::Custom);
        }
    });

    screen.Loop(ui);
    quit = true;
    clock.join();
    return 0;
}
