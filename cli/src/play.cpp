// NETWAR conductor console — the playable M2 terminal client.
//
// All game rules live in netwar::Match (sim/). This file only renders the
// match, turns key presses into Match commands, and runs the tick clock.
// The clock thread never touches the match: it posts a closure that the UI
// thread runs, so the simulation stays single-threaded and deterministic.
// Player-facing text lives in lang.hpp; `l` switches language mid-match.

#include "lang.hpp"

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
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace ftxui;
using netwar::Front;
using netwar::Match;
using netwar::Signal;
using netwar::Tier;
using console_text::Lang;

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

// Terminal columns taken by a UTF-8 string: one per code point, which holds
// for the Latin and Cyrillic text the console prints.
std::size_t columns(std::string_view s) {
    return static_cast<std::size_t>(
        std::count_if(s.begin(), s.end(), [](char ch) { return (ch & 0xC0) != 0x80; }));
}

std::string pad(std::string s, std::size_t width) {
    if (const std::size_t w = columns(s); w < width) s.append(width - w, ' ');
    return s;
}

// Front panel row labels share one column so the values line up.
constexpr std::size_t kLabelWidth = 7;
Element label(const char* s) { return text(pad(s, kLabelWidth)) | color(kDim); }

std::string front_name(const Lang& l, Front f) { return l.front[idx(f)]; }

const char* tier_name(const Lang& l, Tier t) { return l.tier[static_cast<std::size_t>(t)]; }

Color tier_color(Tier t) {
    switch (t) {
    case Tier::Directed: return kGreen;
    case Tier::SemiAutonomous: return kAmber;
    case Tier::Blackout: return kRed;
    }
    return kWhite;
}

const char* grade_name(const Lang& l, netwar::LineGrade g) {
    return l.grade[static_cast<std::size_t>(g)];
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
    const Lang* lang;
    std::uint64_t seed{};
    Match match;
    Front selected = Front::West;
    std::array<std::deque<Sample>, 2> history;
    std::deque<LogLine> log;
    bool briefing = true;

    Console(const Lang& l, std::uint64_t s)
        : lang(&l), seed(s), match(netwar::MatchRules{.seed = s}) {
        note(lang->console_online, kCyan);
    }

    void note(std::string text, Color tint) {
        log.push_front({match.tick(), std::move(text), tint});
        while (log.size() > kLogLines) log.pop_back();
    }

    void restart(std::uint64_t s) {
        *this = Console(*lang, s);
        briefing = false;
    }

    // --- Player verbs ---

    void upgrade() {
        const Lang& l = *lang;
        const std::string name = front_name(l, selected);
        const Signal cost = match.upgrade_cost(selected);
        if (cost == 0) {
            note(l.already_fiber(name), kDim);
        } else if (match.upgrade_line(selected)) {
            note(l.upgraded(name, grade_name(l, match.grade(selected)), num(cost, 0)), kCyan);
        } else {
            note(l.no_matter(name, num(cost, 0)), kDim);
        }
    }

    void nudge_allocation(Signal delta) {
        match.set_allocation(selected, match.allocation(selected) + delta);
    }

    void give_priority() {
        if (match.priority() == selected) return;
        match.set_priority(selected);
        note(lang->took_priority(front_name(*lang, selected)), kCyan);
    }

    // --- Clock ---

    void advance() {
        if (match.outcome() != netwar::Outcome::InProgress) return;
        const Lang& l = *lang;

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

            const std::string name = front_name(l, f);
            if (match.flaring(f) && !flaring_before[idx(f)]) {
                note(l.contact(name, num(match.flare_peak(f))), kAmber);
            }
            const Tier now = match.tier(f);
            if (now != tier_before[idx(f)]) {
                const bool worse = now < tier_before[idx(f)];
                note((worse ? l.browns_out : l.recovers)(name, tier_name(l, now)), tier_color(now));
            }
        }

        if (match.storm_forecast() && !forecast_before) {
            note(l.storm_forecast(std::to_string(match.storm_start() - match.tick())), kCyan);
        }
        if (match.storm_active() && !storm_before) note(l.storm_hits, kRed);
        if (!match.storm_active() && storm_before) note(l.storm_passed, kCyan);

        const auto period = match.rules().escalation_period;
        if (match.tick() % period == 0) {
            note(l.escalates, kRed);
        }

        switch (match.outcome()) {
        case netwar::Outcome::Victory: note(l.won_log, kGreen); break;
        case netwar::Outcome::Defeat: note(l.lost_log, kRed); break;
        case netwar::Outcome::InProgress: break;
        }
    }
};

// --- Rendering ------------------------------------------------------------------

Element scope_row(const Console& c, Front f) {
    const Lang& l = *c.lang;
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
        hbox({label(l.wave), hbox(std::move(intensity))}),
        hbox({label(l.level), hbox(std::move(relay))}),
        hbox({label(""), text(pad(l.past, kHistory)) | color(kDim),
              text(std::string(" ") + l.intel) | color(kDim)}),
    });
}

Element front_panel(const Console& c, Front f) {
    const Lang& l = *c.lang;
    const Match& m = c.match;
    const bool selected = c.selected == f;
    const Tier tier = m.tier(f);

    // Line
    Element upgrade = text(std::string("  ") + l.maxed) | color(kDim);
    if (const Signal cost = m.upgrade_cost(f); cost > 0) {
        const bool affordable = m.matter() >= cost;
        const bool copper = m.grade(f) == netwar::LineGrade::Copper;
        upgrade = text(std::string("  [u] -> ") +
                       grade_name(l, copper ? netwar::LineGrade::Coax : netwar::LineGrade::Fiber) +
                       " " + (copper ? "4" : "8") + l.per_tick + " : " + num(cost, 0) +
                       l.matter_unit) |
                  color(affordable ? kCyan : kDim);
    }
    Element line_row = hbox({label(l.line), text(pad(grade_name(l, m.grade(f)), 7)) | bold,
                             text(num(m.line_throughput(f), 0) + l.per_tick), upgrade});

    // Router
    std::string pips;
    const auto alloc = m.allocation(f) / netwar::kSignalScale;
    const auto router_max = m.router_max() / netwar::kSignalScale;
    for (Signal i = 0; i < router_max; ++i) pips += i < alloc ? "■" : "□";
    const bool priority = m.priority() == f;
    Element router_row =
        hbox({label(l.router), text(pips) | color(kCyan),
              text(" " + num(m.allocation(f), 0) + l.per_tick),
              priority ? text(std::string("  ") + l.priority_on) | color(kAmber) | bold
                       : text(std::string("  ") + l.priority_hint) | color(kDim)});

    // Relay
    Element relay_row = hbox({
        label(l.relay),
        meter(static_cast<float>(m.relay(f)) / static_cast<float>(m.relay_capacity()), 16,
              tier_color(tier)),
        text(" " + pad(num(m.relay(f)), 5)),
        text(tier_name(l, tier)) | color(tier_color(tier)) | bold,
    });

    // Enemy
    Element enemy_row;
    if (m.flaring(f)) {
        enemy_row = hbox({label(l.enemy),
                          text(std::string(l.flare) + " " + num(m.intensity(f)) + l.per_tick) |
                              color(kAmber) | bold,
                          text(std::string("  ") + l.peak + " " + num(m.flare_peak(f))) |
                              color(kAmber)});
    } else {
        enemy_row = hbox({label(l.enemy), text(l.quiet(std::to_string(m.ticks_to_flare(f)))),
                          text(std::string("  ") + l.next_peak + " " + num(m.next_flare_peak(f))) |
                              color(kAmber)});
    }

    // Front line
    const auto hold = m.hold(f);
    const Color hold_color = hold >= 60'000 ? kGreen : hold >= 30'000 ? kAmber : kRed;
    Element hold_row = hbox({
        label(l.front_row),
        text(std::string(l.collapse) + " ") | color(kRed) | dim,
        meter(static_cast<float>(hold) / static_cast<float>(netwar::kHoldMax), 20, hold_color),
        text(std::string(" ") + l.breakthrough) | color(kGreen) | dim,
        text("  " + std::to_string(hold / 1000) + "%") | bold | color(hold_color),
    });

    Element body = vbox({line_row, router_row, relay_row, separatorLight(), enemy_row,
                         scope_row(c, f), separatorLight(), hold_row});

    Element title = text(std::string(" ") + l.front_title[idx(f)] + " " + (selected ? "◀ " : "")) |
                    bold | color(selected ? kWhite : kDim);
    return window(title, body, selected ? DOUBLE : ROUNDED) | color(selected ? kAmber : kDim) | flex;
}

Element header(const Console& c, int tick_ms, bool paused) {
    const Lang& l = *c.lang;
    const Match& m = c.match;
    char speed[32];
    std::snprintf(speed, sizeof speed, "%.2g %s", tick_ms / 1000.0, l.sec_per_tick);

    Element state = paused ? text(l.paused) | inverted | color(kAmber)
                           : text(l.live) | inverted | color(kGreen);
    if (m.outcome() == netwar::Outcome::Victory) state = text(l.victory) | inverted | color(kGreen);
    if (m.outcome() == netwar::Outcome::Defeat) state = text(l.defeat) | inverted | color(kRed);

    return hbox({
        text(" NETWAR ") | bold | inverted | color(kAmber),
        text(l.title) | color(kAmber),
        text("  v" NETWAR_VERSION) | color(kDim),
        filler(),
        text("T+" + std::to_string(m.tick())) | bold,
        text(std::string("   ") + l.seed + " " + std::to_string(c.seed)) | color(kDim),
        text("   " + std::string(speed) + "  ") | color(kCyan),
        state,
    });
}

Element hub_panel(const Console& c) {
    const Lang& l = *c.lang;
    const Match& m = c.match;
    const auto& g = m.engine().graph();
    const Signal gen = g.find(netwar::ids::kCommandHub)->generation;
    const Signal upkeep = g.find(netwar::ids::kUpkeepDrain)->consumption;
    const float fill = static_cast<float>(m.hub_buffer()) / static_cast<float>(m.hub_capacity());

    Element weather = text(l.spectrum_clear) | color(kDim);
    if (m.storm_active()) {
        weather = text(l.storm_raging(std::to_string(m.storm_end() - m.tick()))) | bold | color(kRed);
    } else if (m.storm_forecast()) {
        weather =
            text(l.storm_coming(std::to_string(m.storm_start() - m.tick()))) | bold | color(kAmber);
    }

    return hbox({
        text(l.hub) | bold | color(kAmber),
        text("+" + num(gen, 0) + l.per_tick + "  " + l.upkeep + " -" + num(upkeep, 0) + l.per_tick +
             "  " + l.buffer + " "),
        meter(fill, 16, fill > 0.95F ? kRed : kCyan),
        text(" " + num(m.hub_buffer(), 0) + "/" + num(m.hub_capacity(), 0)),
        text(std::string("  ") + l.heat + " " + num(m.engine().wasted_heat(), 0)) | color(kDim),
        filler(),
        text(std::string(l.matter) + " ") | bold | color(kAmber),
        text(num(m.matter(), 0)) | bold,
        text(" (+" + num(m.rules().matter_income, 0) + l.per_tick + ")   ") | color(kDim),
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

Element keys_bar(const Lang& l) {
    auto key = [](const std::string& k, const std::string& what) {
        return hbox({text(k) | inverted | color(kAmber), text(" " + what + "  ") | color(kDim)});
    };
    return hflow({key("←→", l.key_front), key("↑↓", l.key_router), key("u", l.key_upgrade),
                  key("p", l.key_priority), key("space", l.key_pause), key("+-", l.key_speed),
                  key("n", l.key_step), key("h", l.key_help), key("l", l.key_lang),
                  key("r", l.key_restart), key("q", l.key_quit)});
}

Element briefing_box(const Lang& l) {
    auto para = [](const std::string& s, Color c = kWhite) { return paragraph(s) | color(c); };
    auto tier_row = [&](Tier t, Color c) {
        const auto i = static_cast<std::size_t>(t);
        return hbox({text("    " + pad(l.brief_tier_range[i], 12)),
                     text(pad(tier_name(l, t), 16)) | color(c) | bold,
                     text(l.brief_tier_effect[i])});
    };
    return window(text(l.briefing_title) | bold | color(kAmber),
                  vbox({
                      para(l.brief_role, kAmber),
                      text(""),
                      para(l.brief_hub),
                      para(l.brief_flares),
                      text(l.brief_tiers),
                      tier_row(Tier::Directed, kGreen),
                      tier_row(Tier::SemiAutonomous, kAmber),
                      tier_row(Tier::Blackout, kRed),
                      para(l.brief_win),
                      para(l.brief_lines),
                      para(l.brief_intel),
                      para(l.brief_escalation),
                      text(""),
                      para(l.brief_start, kCyan),
                  }) | size(WIDTH, LESS_THAN, 86)) |
           color(kAmber) | clear_under | center;
}

Element outcome_box(const Console& c) {
    const Lang& l = *c.lang;
    const Match& m = c.match;
    const bool won = m.outcome() == netwar::Outcome::Victory;
    Front decisive = Front::West;
    for (Front f : kFronts) {
        if ((won && m.hold(f) == netwar::kHoldMax) || (!won && m.hold(f) == 0)) decisive = f;
    }
    const int front = static_cast<int>(idx(decisive));
    const std::string headline = won ? l.breakthrough_on(front) : l.collapsed(front);
    const Color tint = won ? kGreen : kRed;

    return window(text(won ? l.victory : l.defeat) | bold | color(tint),
                  vbox({
                      text(headline) | bold | color(tint) | hcenter,
                      text(""),
                      text(l.held(std::to_string(m.tick()), num(m.engine().wasted_heat(), 0))) |
                          color(kWhite) | hcenter,
                      text(""),
                      text(l.outcome_keys) | color(kCyan) | hcenter,
                  }) | size(WIDTH, GREATER_THAN, 50)) |
           color(tint) | clear_under | center;
}

// Start-up picker. Each language is listed in its own name, and the frame is
// bilingual, because the player has not chosen yet.
Element language_box(std::size_t choice) {
    Elements rows;
    for (std::size_t i = 0; i < console_text::kLanguages.size(); ++i) {
        const bool on = i == choice;
        const std::string row = std::string(on ? " ▶ " : "   ") + std::to_string(i + 1) + "  " +
                                console_text::kLanguages[i]->name + "  ";
        rows.push_back(on ? text(row) | bold | inverted | color(kAmber) : text(row) | color(kWhite));
    }
    return window(text(" LANGUAGE / МОВА ") | bold | color(kAmber),
                  vbox({text(""), vbox(std::move(rows)) | hcenter, text(""),
                        text("↑↓  Enter") | color(kCyan) | hcenter})) |
           size(WIDTH, GREATER_THAN, 30) | color(kAmber) | clear_under | center;
}

// A key press, whether the player's layout is Latin or Ukrainian (ЙЦУКЕН):
// `cyrillic` is the letter printed on the same physical key.
bool pressed(const Event& e, char latin, const char* cyrillic) {
    return e == Event::Character(latin) || e == Event::Character(cyrillic);
}

} // namespace

int main(int argc, char** argv) {
    std::uint64_t seed = std::random_device{}() % 100000;
    const Lang* lang = console_text::lang_from_env();
    bool picking = true; // ask at start-up unless --lang already answered
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        std::string_view code;
        if (arg == "--lang" && i + 1 < argc) {
            code = argv[++i];
        } else if (arg.starts_with("--lang=")) {
            code = arg.substr(7);
        } else {
            seed = std::strtoull(argv[i], nullptr, 10);
            continue;
        }
        lang = console_text::find_lang(code);
        if (lang == nullptr) {
            std::fprintf(stderr, "netwar: unknown language '%.*s' (available: en, uk)\n",
                         static_cast<int>(code.size()), code.data());
            return 2;
        }
        picking = false;
    }
    const auto& langs = console_text::kLanguages;
    std::size_t choice = static_cast<std::size_t>(
        std::find(langs.begin(), langs.end(), lang) - langs.begin());

    Console console(*lang, seed);
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
            window(text(console.lang->signal_log) | color(kDim), log_panel(console)) | color(kDim),
            keys_bar(*console.lang),
        });
        if (picking) return dbox({main, language_box(choice)});
        if (console.briefing) return dbox({main, briefing_box(*console.lang)});
        if (console.match.outcome() != netwar::Outcome::InProgress) {
            return dbox({main, outcome_box(console)});
        }
        return main;
    });

    ui |= CatchEvent([&](const Event& e) {
        if (pressed(e, 'q', "й") || e == Event::Escape) {
            quit = true;
            screen.Exit();
            return true;
        }
        if (picking) {
            const std::size_t count = langs.size();
            // Digits pick a language outright; Enter or SPACE confirm the cursor.
            const bool digit = e.is_character() && e.character()[0] >= '1' &&
                               static_cast<std::size_t>(e.character()[0] - '1') < count;
            if (digit) choice = static_cast<std::size_t>(e.character()[0] - '1');
            if (e == Event::ArrowUp || e == Event::ArrowLeft || pressed(e, 'w', "ц")) {
                choice = (choice + count - 1) % count;
            } else if (e == Event::ArrowDown || e == Event::ArrowRight || e == Event::Tab ||
                       pressed(e, 's', "і")) {
                choice = (choice + 1) % count;
            } else if (digit || e == Event::Return || e == Event::Character(' ')) {
                // Rebuilt rather than relabelled, so the opening log line is
                // already in the chosen language.
                console = Console(*langs[choice], seed);
                picking = false;
            }
            return true; // nothing else reaches the match until a language is chosen
        }
        if (e == Event::Character(' ')) {
            console.briefing = false;
            paused = !paused;
            return true;
        }
        if (pressed(e, 'h', "р")) {
            console.briefing = !console.briefing;
            if (console.briefing) paused = true;
            return true;
        }
        if (pressed(e, 'l', "д")) {
            console.lang = console.lang == &console_text::kUkrainian ? &console_text::kEnglish
                                                                     : &console_text::kUkrainian;
            return true;
        }
        if (pressed(e, 'r', "к")) {
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
        if (pressed(e, 'n', "т")) {
            if (paused) console.advance();
            return true;
        }
        if (e == Event::ArrowLeft || pressed(e, 'a', "ф")) {
            console.selected = Front::West;
            return true;
        }
        if (e == Event::ArrowRight || pressed(e, 'd', "в")) {
            console.selected = Front::East;
            return true;
        }
        if (e == Event::Tab) {
            console.selected = other(console.selected);
            return true;
        }
        if (e == Event::ArrowUp || pressed(e, 'w', "ц")) {
            console.nudge_allocation(netwar::units(1));
            return true;
        }
        if (e == Event::ArrowDown || pressed(e, 's', "і")) {
            console.nudge_allocation(-netwar::units(1));
            return true;
        }
        if (pressed(e, 'u', "г")) {
            console.upgrade();
            return true;
        }
        if (pressed(e, 'p', "з")) {
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
