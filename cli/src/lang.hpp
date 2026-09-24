// Player-facing text of the conductor console, one table per language.
//
// Messages that carry numbers or names are functions rather than format
// strings, so each language is free to order and inflect its own sentence.
// Keys stay Latin in every language; play.cpp also accepts the keys that sit
// in the same place on a Ukrainian (ЙЦУКЕН) layout.

#pragma once

#include <array>
#include <cstdlib>
#include <string>
#include <string_view>

namespace console_text {

using Msg = std::string;
using Arg = const std::string&;

struct Lang {
    const char* code;
    const char* name; // in the language itself, for the start-up picker

    // Indexed by netwar::Front, netwar::Tier and netwar::LineGrade.
    std::array<const char*, 2> front;       // log tag: "WEST"
    std::array<const char*, 2> front_title; // panel title: "WEST FRONT"
    std::array<const char*, 3> tier;
    std::array<const char*, 3> grade;

    // Header
    const char* title;
    const char* seed;
    const char* sec_per_tick;
    const char* paused;
    const char* live;
    const char* victory;
    const char* defeat;

    // Hub bar
    const char* hub;
    const char* per_tick; // "/t"
    const char* ticks;    // "t", as in "5t"
    const char* upkeep;
    const char* buffer;
    const char* heat;
    const char* matter;
    const char* spectrum_clear;
    Msg (*storm_raging)(Arg ticks_left);
    Msg (*storm_coming)(Arg ticks_until);

    // Front panel. Row labels are padded to a common width by the renderer.
    const char* line;
    const char* router;
    const char* relay;
    const char* enemy;
    const char* front_row;
    const char* wave;
    const char* level;
    const char* past;
    const char* intel;
    const char* maxed;
    const char* matter_unit; // cost suffix: "30m"
    const char* priority_on;
    const char* priority_hint;
    const char* flare;
    const char* peak;
    const char* next_peak;
    Msg (*quiet)(Arg ticks);
    const char* collapse;
    const char* breakthrough;

    // Signal log
    const char* signal_log;
    const char* console_online;
    Msg (*already_fiber)(Arg front);
    Msg (*upgraded)(Arg front, Arg grade, Arg cost);
    Msg (*no_matter)(Arg front, Arg cost);
    Msg (*took_priority)(Arg front);
    Msg (*contact)(Arg front, Arg peak);
    Msg (*browns_out)(Arg front, Arg tier);
    Msg (*recovers)(Arg front, Arg tier);
    Msg (*storm_forecast)(Arg ticks);
    const char* storm_hits;
    const char* storm_passed;
    const char* escalates;
    const char* won_log;
    const char* lost_log;

    // Key bar
    const char* key_front;
    const char* key_router;
    const char* key_upgrade;
    const char* key_priority;
    const char* key_pause;
    const char* key_speed;
    const char* key_step;
    const char* key_help;
    const char* key_lang;
    const char* key_restart;
    const char* key_quit;

    // Briefing
    const char* briefing_title;
    const char* brief_role;
    const char* brief_hub;
    const char* brief_flares;
    const char* brief_tiers;
    std::array<const char*, 3> brief_tier_range; // Blackout, Semi, Directed
    std::array<const char*, 3> brief_tier_effect;
    const char* brief_win;
    const char* brief_lines;
    const char* brief_intel;
    const char* brief_escalation;
    const char* brief_start;

    // Outcome
    Msg (*breakthrough_on)(int front);
    Msg (*collapsed)(int front);
    Msg (*held)(Arg ticks, Arg heat);
    const char* outcome_keys;
};

// "1 тік", "2 тіки", "5 тіків": Ukrainian nouns agree with the number.
inline Msg uk_ticks(Arg n) {
    const long v = std::strtol(n.c_str(), nullptr, 10);
    const long tens = v % 100;
    const long ones = v % 10;
    if (tens >= 11 && tens <= 14) return n + " тіків";
    if (ones == 1) return n + " тік";
    if (ones >= 2 && ones <= 4) return n + " тіки";
    return n + " тіків";
}

inline const Lang kEnglish = {
    .code = "en",
    .name = "English",
    .front = {"WEST", "EAST"},
    .front_title = {"WEST FRONT", "EAST FRONT"},
    .tier = {"BLACKOUT", "SEMI-AUTONOMOUS", "DIRECTED"},
    .grade = {"COPPER", "COAX", "FIBER"},

    .title = " // CONDUCTOR CONSOLE",
    .seed = "seed",
    .sec_per_tick = "s/tick",
    .paused = " ❚❚ PAUSED ",
    .live = " ▶ LIVE ",
    .victory = " ✔ VICTORY ",
    .defeat = " ✖ DEFEAT ",

    .hub = " HUB ",
    .per_tick = "/t",
    .ticks = "t",
    .upkeep = "upkeep",
    .buffer = "BUFFER",
    .heat = "heat",
    .matter = "MATTER",
    .spectrum_clear = "SPECTRUM clear",
    .storm_raging = [](Arg t) { return "⚡ STORM: relay decay x3, " + t + "t left"; },
    .storm_coming = [](Arg t) { return "⚠ STORM in " + t + "t"; },

    .line = "LINE",
    .router = "ROUTER",
    .relay = "RELAY",
    .enemy = "ENEMY",
    .front_row = "FRONT",
    .wave = "WAVE",
    .level = "LEVEL",
    .past = "past",
    .intel = "intel",
    .maxed = "maxed",
    .matter_unit = "m",
    .priority_on = "★ PRIORITY",
    .priority_hint = "[p] priority",
    .flare = "FLARE",
    .peak = "peak",
    .next_peak = "next peak",
    .quiet = [](Arg t) { return "quiet, contact in " + t + "t"; },
    .collapse = "collapse",
    .breakthrough = "breakthrough",

    .signal_log = " SIGNAL LOG ",
    .console_online = "Console online. Two fronts, one hub. Press SPACE to go live.",
    .already_fiber = [](Arg f) { return f + " line is already fiber."; },
    .upgraded = [](Arg f, Arg g, Arg cost) {
        return f + " line upgraded to " + g + " (-" + cost + " matter).";
    },
    .no_matter = [](Arg f, Arg cost) {
        return "Not enough matter: " + cost + " needed for the " + f + " line.";
    },
    .took_priority = [](Arg f) { return f + " router takes priority on the hub buffer."; },
    .contact = [](Arg f, Arg peak) {
        return f + ": enemy contact, flare peaking at " + peak + "/t.";
    },
    .browns_out = [](Arg f, Arg t) { return f + " relay browns out -> " + t + "."; },
    .recovers = [](Arg f, Arg t) { return f + " relay recovers -> " + t + "."; },
    .storm_forecast = [](Arg t) {
        return "Spectrum storm forecast in " + t + " ticks: relay decay x3. Fill the relays.";
    },
    .storm_hits = "Spectrum storm hits. Relays are leaking.",
    .storm_passed = "Storm has passed.",
    .escalates = "Enemy escalates: flares grow stronger.",
    .won_log = "BREAKTHROUGH. The line holds.",
    .lost_log = "A FRONT HAS COLLAPSED.",

    .key_front = "front",
    .key_router = "router",
    .key_upgrade = "upgrade",
    .key_priority = "priority",
    .key_pause = "pause",
    .key_speed = "speed",
    .key_step = "step",
    .key_help = "help",
    .key_lang = "українська",
    .key_restart = "restart",
    .key_quit = "quit",

    .briefing_title = " CONDUCTOR BRIEFING ",
    .brief_role = "You command a base, not an army. Nothing you do touches a soldier - "
                  "your only weapon is command bandwidth.",
    .brief_hub = "- The HUB makes 12 signal per tick; 3 go to upkeep. Routers push the rest "
                 "down the LINES into each front's RELAY.",
    .brief_flares = "- The enemy FLARES the fronts in turn. Combat drains the relay.",
    .brief_tiers = "- A relay's level sets its front's command tier:",
    .brief_tier_range = {"below 5", "5 to 15", "above 15"},
    .brief_tier_effect = {"it falls back fast", "it slowly gives ground", "the front pushes forward"},
    .brief_win = "- Push either front to 100% for a BREAKTHROUGH. Let either reach 0% and you lose.",
    .brief_lines = "- Copper lines carry 1/t. Spend MATTER on coax (4/t) and fiber (8/t). Once "
                   "your lines outgrow the hub, you must choose who gets fed: router allocation "
                   "and priority.",
    .brief_intel = "- The scope shows enemy intel for the next 20 ticks (dimmed). Fill a relay "
                   "BEFORE its flare. Spectrum storms triple relay decay.",
    .brief_escalation = "- The enemy escalates every 100 ticks. Standing still loses.",
    .brief_start = "Press SPACE to go live. h reopens this briefing, l switches language.",

    .breakthrough_on = [](int f) {
        return std::string("BREAKTHROUGH ON THE ") + (f == 0 ? "WEST" : "EAST") + " FRONT";
    },
    .collapsed = [](int f) {
        return std::string("THE ") + (f == 0 ? "WEST" : "EAST") + " FRONT HAS COLLAPSED";
    },
    .held = [](Arg ticks, Arg heat) {
        return "held for " + ticks + " ticks, " + heat + " signal vented as heat";
    },
    .outcome_keys = "r  new match      q  quit",
};

inline const Lang kUkrainian = {
    .code = "uk",
    .name = "Українська",
    .front = {"ЗАХІД", "СХІД"},
    .front_title = {"ЗАХІДНИЙ ФРОНТ", "СХІДНИЙ ФРОНТ"},
    .tier = {"БЛЕКАУТ", "НАПІВАВТОНОМНИЙ", "КЕРОВАНИЙ"},
    .grade = {"МІДЬ", "КОАКС", "ОПТИКА"},

    .title = " // ПУЛЬТ ДИРИГЕНТА",
    .seed = "сід",
    .sec_per_tick = "с/тік",
    .paused = " ❚❚ ПАУЗА ",
    .live = " ▶ В ЕФІРІ ",
    .victory = " ✔ ПЕРЕМОГА ",
    .defeat = " ✖ ПОРАЗКА ",

    .hub = " ХАБ ",
    .per_tick = "/т",
    .ticks = "т",
    .upkeep = "утримання",
    .buffer = "БУФЕР",
    .heat = "тепло",
    .matter = "МАТЕРІЯ",
    .spectrum_clear = "ЕФІР чистий",
    .storm_raging = [](Arg t) { return "⚡ ШТОРМ: розпад реле x3, ще " + t + "т"; },
    .storm_coming = [](Arg t) { return "⚠ ШТОРМ за " + t + "т"; },

    .line = "ЛІНІЯ",
    .router = "РОУТЕР",
    .relay = "РЕЛЕ",
    .enemy = "ВОРОГ",
    .front_row = "ФРОНТ",
    .wave = "ХВИЛЯ",
    .level = "РІВЕНЬ",
    .past = "минуле",
    .intel = "розвідка",
    .maxed = "максимум",
    .matter_unit = "м",
    .priority_on = "★ ПРІОРИТЕТ",
    .priority_hint = "[p] пріоритет",
    .flare = "СПАЛАХ",
    .peak = "пік",
    .next_peak = "наступний пік",
    .quiet = [](Arg t) { return "тиша, контакт за " + t + "т"; },
    .collapse = "обвал",
    .breakthrough = "прорив",

    .signal_log = " ЖУРНАЛ СИГНАЛІВ ",
    .console_online = "Пульт на зв'язку. Два фронти, один хаб. SPACE - вийти в ефір.",
    .already_fiber = [](Arg f) { return f + ": лінія вже оптоволоконна."; },
    .upgraded = [](Arg f, Arg g, Arg cost) {
        return f + ": лінію модернізовано до " + g + " (-" + cost + " матерії).";
    },
    .no_matter = [](Arg f, Arg cost) {
        return f + ": бракує матерії, на лінію потрібно " + cost + ".";
    },
    .took_priority = [](Arg f) { return f + ": роутер отримує пріоритет на буфер хаба."; },
    .contact = [](Arg f, Arg peak) {
        return f + ": контакт із ворогом, пік спалаху " + peak + "/т.";
    },
    .browns_out = [](Arg f, Arg t) { return f + ": реле просідає -> " + t + "."; },
    .recovers = [](Arg f, Arg t) { return f + ": реле відновлюється -> " + t + "."; },
    .storm_forecast = [](Arg t) {
        return "Спектральний шторм через " + uk_ticks(t) + ": розпад реле x3. Заповніть реле.";
    },
    .storm_hits = "Спектральний шторм! Реле втрачають сигнал.",
    .storm_passed = "Шторм минув.",
    .escalates = "Ворог посилюється: спалахи стають сильнішими.",
    .won_log = "ПРОРИВ. Лінія тримається.",
    .lost_log = "ФРОНТ ОБВАЛИВСЯ.",

    .key_front = "фронт",
    .key_router = "роутер",
    .key_upgrade = "модернізація",
    .key_priority = "пріоритет",
    .key_pause = "пауза",
    .key_speed = "швидкість",
    .key_step = "крок",
    .key_help = "довідка",
    .key_lang = "english",
    .key_restart = "заново",
    .key_quit = "вихід",

    .briefing_title = " БРИФІНГ ДИРИГЕНТА ",
    .brief_role = "Ви командуєте базою, а не армією. Жодна ваша дія не торкається солдата - "
                  "ваша єдина зброя це командна смуга пропускання.",
    .brief_hub = "- ХАБ виробляє 12 сигналу за тік; 3 іде на утримання. Роутери женуть решту "
                 "ЛІНІЯМИ до РЕЛЕ кожного фронту.",
    .brief_flares = "- Ворог по черзі розпалює СПАЛАХИ на фронтах. Бій виснажує реле.",
    .brief_tiers = "- Рівень реле визначає командний режим фронту:",
    .brief_tier_range = {"менше 5", "від 5 до 15", "понад 15"},
    .brief_tier_effect = {"фронт швидко відкочується", "фронт повільно поступається",
                          "фронт наступає"},
    .brief_win = "- Дотисніть будь-який фронт до 100% - це ПРОРИВ. Якщо будь-який впаде до "
                 "0% - поразка.",
    .brief_lines = "- Мідні лінії несуть 1/т. Витрачайте МАТЕРІЮ на коаксіал (4/т) і оптику "
                   "(8/т). Коли лінії переростуть хаб, доведеться обирати, кого годувати: "
                   "розподіл роутерів і пріоритет.",
    .brief_intel = "- Осцилограф показує розвідку ворога на 20 тіків уперед (тьмяно). "
                   "Заповнюйте реле ДО спалаху. Спектральні шторми потроюють розпад реле.",
    .brief_escalation = "- Ворог посилюється кожні 100 тіків. Хто стоїть на місці, той програє.",
    .brief_start = "SPACE - вийти в ефір. h знову відкриває брифінг, l перемикає мову.",

    .breakthrough_on = [](int f) {
        return std::string("ПРОРИВ НА ") + (f == 0 ? "ЗАХІДНОМУ" : "СХІДНОМУ") + " ФРОНТІ";
    },
    .collapsed = [](int f) {
        return std::string(f == 0 ? "ЗАХІДНИЙ" : "СХІДНИЙ") + " ФРОНТ ОБВАЛИВСЯ";
    },
    .held = [](Arg ticks, Arg heat) {
        return "протрималися " + uk_ticks(ticks) + ", " + heat + " сигналу скинуто в тепло";
    },
    .outcome_keys = "r  новий матч      q  вихід",
};

inline constexpr std::array<const Lang*, 2> kLanguages = {&kEnglish, &kUkrainian};

inline const Lang* find_lang(std::string_view code) {
    for (const Lang* lang : kLanguages) {
        if (code == lang->code) return lang;
    }
    return nullptr;
}

// Ukrainian when the locale says so (LANG=uk_UA.UTF-8 and friends), else
// English. POSIX precedence: LC_ALL, then LC_MESSAGES, then LANG.
inline const Lang* lang_from_env() {
    for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = std::getenv(var);
        if (value == nullptr || *value == '\0') continue;
        return std::string_view(value).starts_with("uk") ? &kUkrainian : &kEnglish;
    }
    return &kEnglish;
}

} // namespace console_text
