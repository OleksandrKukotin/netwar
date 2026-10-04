#include "netwar/match.hpp"

#include "netwar/scenario.hpp"
#include "netwar/wave.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace netwar {

namespace {

// Line throughput by grade (GDD 7A): copper 1, coax 4, fiber 8.
Signal throughput_of(LineGrade grade) {
    switch (grade) {
    case LineGrade::Copper: return units(1);
    case LineGrade::Coax: return units(4);
    case LineGrade::Fiber: return units(8);
    }
    return units(1);
}

// Relay -> combat edges are wide open: escalated flares may exceed the
// router maximum, and the relay level alone should limit what combat takes.
constexpr Signal kCombatEdge = units(1000);

// Rejects rules that would divide by zero, wrap an unsigned span, or roll
// negative flare peaks.
const MatchRules& validated(const MatchRules& r) {
    if (r.escalation_period == 0) {
        throw std::invalid_argument("MatchRules: escalation_period must be positive");
    }
    if (r.storm_gap_max < r.storm_gap_min) {
        throw std::invalid_argument("MatchRules: storm_gap_max is below storm_gap_min");
    }
    if (r.flare_jitter_per_mille < 0 || r.flare_jitter_per_mille > 1000) {
        throw std::invalid_argument("MatchRules: flare_jitter_per_mille must be within 0..1000");
    }
    return r;
}

// Match looks nodes up by ID on every tick; checking them once here makes
// those lookups safe to dereference.
void require_node(const Graph& g, NodeId id) {
    if (g.find(id) == nullptr) throw std::logic_error("Match: scenario lacks a required node");
}

std::size_t index_of_line(const Graph& g, std::uint32_t id) {
    for (std::size_t i = 0; i < g.connections.size(); ++i) {
        if (g.connections[i].id == id) return i;
    }
    throw std::logic_error("Match: scenario lacks a forward line");
}

} // namespace

Match::Match(MatchRules rules)
    : rules_(validated(rules)), engine_(make_readme_scenario()), rng_(rules.seed) {
    Graph& g = engine_.graph();
    sides_[static_cast<std::size_t>(Front::West)] = {.relay = ids::kWestRelay,
                                                     .router = ids::kWestRouter,
                                                     .intensity = ids::kWestIntensity,
                                                     .decay = ids::kWestDecay,
                                                     .line = index_of_line(g, ids::kWestLine),
                                                     .phase = 0};
    sides_[static_cast<std::size_t>(Front::East)] = {.relay = ids::kEastRelay,
                                                     .router = ids::kEastRouter,
                                                     .intensity = ids::kEastIntensity,
                                                     .decay = ids::kEastDecay,
                                                     .line = index_of_line(g, ids::kEastLine),
                                                     .phase = kCosineOffset};
    require_node(g, ids::kHubBuffer);
    for (const auto& s : sides_) {
        for (NodeId id : {s.relay, s.router, s.intensity, s.decay}) require_node(g, id);
    }

    router_max_ = g.find(ids::kWestRouter)->allocation;
    matter_ = rules_.starting_matter;

    for (auto& s : sides_) {
        s.hold = rules_.starting_hold;
        // Storms raise decay temporarily; calm weather restores the
        // scenario's own rate rather than a constant.
        s.base_decay = g.find(s.decay)->decay_per_mille;
        // A front already mid-wave at tick 0 is inside the deployment window.
        s.current_peak = rules_.opening_grace > 0 ? 0 : roll_peak();
        s.next_peak = roll_peak();
        g.find(s.relay)->capacity = rules_.relay_capacity;
        for (auto& conn : g.connections) {
            if (conn.from == s.relay && conn.to != s.decay) conn.throughput = kCombatEdge;
        }
    }
    schedule_storm(0);
}

bool Match::upgrade_line(Front front) {
    const Signal cost = upgrade_cost(front);
    if (outcome_ != Outcome::InProgress || cost == 0 || matter_ < cost) return false;

    matter_ -= cost;
    Side& s = side(front);
    s.grade = s.grade == LineGrade::Copper ? LineGrade::Coax : LineGrade::Fiber;
    line(front).throughput = throughput_of(s.grade);
    return true;
}

void Match::set_allocation(Front front, Signal allocation) {
    engine_.graph().find(side(front).router)->allocation =
        std::clamp(allocation, Signal{0}, router_max_);
}

void Match::set_priority(Front front) {
    // Gates pull in node-vector order (Engine::route), so priority is simply
    // which router sits first. Swapping two nodes keeps the order fixed and
    // deterministic.
    auto& nodes = engine_.graph().nodes;
    auto find = [&](NodeId id) {
        return std::find_if(nodes.begin(), nodes.end(), [id](const Node& n) { return n.id == id; });
    };
    auto mine = find(side(front).router);
    auto other = find(side(front == Front::West ? Front::East : Front::West).router);
    if (other < mine) std::iter_swap(mine, other);
}

void Match::step() {
    if (outcome_ != Outcome::InProgress) return;

    Graph& g = engine_.graph();
    const Tick now = engine_.current_tick();

    // A new flare starts when the wave turns positive: promote the forecast
    // peak and draw the one after it. Inside the deployment window the flare
    // stays silent and the forecast is kept for the first real contact.
    for (std::size_t i = 0; i < kFrontCount; ++i) {
        Side& s = sides_[i];
        if (wave_step(static_cast<Front>(i)) == 1) {
            if (now < rules_.opening_grace) {
                s.current_peak = 0;
            } else {
                s.current_peak = s.next_peak;
                s.next_peak = roll_peak();
            }
        }
        g.find(s.intensity)->amplitude = s.current_peak;
    }

    if (now >= storm_end()) schedule_storm(now);
    for (const auto& s : sides_) {
        g.find(s.decay)->decay_per_mille =
            storm_active() ? rules_.storm_decay_per_mille : s.base_decay;
    }

    engine_.tick();
    matter_ += rules_.matter_income;
    update_holds();
}

void Match::update_holds() {
    bool collapsed = false;
    bool broke_through = false;

    for (std::size_t i = 0; i < kFrontCount; ++i) {
        const auto front = static_cast<Front>(i);
        Side& s = sides_[i];

        std::int64_t push = rules_.blackout_push;
        switch (tier(front)) {
        case Tier::Directed: push = rules_.directed_push; break;
        case Tier::SemiAutonomous: push = rules_.semi_push; break;
        case Tier::Blackout: break;
        }
        // Units of intensity (milliunits / 1000) x push, in milli-percent.
        s.hold = std::clamp(s.hold + intensity(front) * push / 1000, Hold{0}, kHoldMax);

        collapsed = collapsed || s.hold == 0;
        broke_through = broke_through || s.hold == kHoldMax;
    }

    // A collapse anywhere outweighs a breakthrough elsewhere on the same tick.
    if (collapsed) {
        outcome_ = Outcome::Defeat;
    } else if (broke_through) {
        outcome_ = Outcome::Victory;
    }
}

Signal Match::hub_buffer() const { return engine_.graph().find(ids::kHubBuffer)->stored; }

Signal Match::hub_capacity() const { return engine_.graph().find(ids::kHubBuffer)->capacity; }

Front Match::priority() const {
    for (const auto& node : engine_.graph().nodes) {
        if (node.id == ids::kWestRouter) return Front::West;
        if (node.id == ids::kEastRouter) return Front::East;
    }
    return Front::West;
}

Signal Match::line_throughput(Front front) const { return line(front).throughput; }

Signal Match::allocation(Front front) const {
    return engine_.graph().find(side(front).router)->allocation;
}

Signal Match::delivered(Front front) const {
    return engine_.graph().find(side(front).router)->routed;
}

Signal Match::upgrade_cost(Front front) const {
    switch (side(front).grade) {
    case LineGrade::Copper: return rules_.coax_cost;
    case LineGrade::Coax: return rules_.fiber_cost;
    case LineGrade::Fiber: return 0;
    }
    return 0;
}

Signal Match::relay(Front front) const { return engine_.graph().find(side(front).relay)->stored; }

Signal Match::intensity(Front front) const {
    return engine_.graph().find(side(front).intensity)->value;
}

Tick Match::ticks_to_flare(Front front) const {
    // Positive half of the wave is steps 1..9; step 0 and the trough are quiet.
    // Start from the flare the next step() samples, or the next one to begin.
    const auto now = static_cast<std::int64_t>(engine_.current_tick());
    const std::int64_t step = wave_step(front);
    const bool in_flare = step >= 1 && step < kWavePeriod / 2;
    std::int64_t flare_start =
        in_flare ? now - (step - 1) : now + (kWavePeriod + 1 - step) % kWavePeriod;

    // Flares that begin inside the deployment window stay silent (step()),
    // so the next contact is the first flare to begin after it.
    const auto grace = static_cast<std::int64_t>(rules_.opening_grace);
    while (grace > 0 && flare_start < grace) flare_start += kWavePeriod;
    return static_cast<Tick>(std::max(flare_start - now, std::int64_t{0}));
}

Signal Match::forecast_intensity(Front front, Tick ahead) const {
    if (ahead == 0) return intensity(front);

    // The tick whose step() will sample this value, and its wave step.
    const auto sampled = static_cast<std::int64_t>(engine_.current_tick() + ahead - 1);
    const std::int64_t step = (sampled + side(front).phase) % kWavePeriod;
    const std::int64_t wave = sine_per_mille(step);
    if (wave <= 0) return 0;

    // A flare that already began runs at the current peak; one that begins
    // on or after this tick will be promoted from the forecast peak.
    const std::int64_t flare_start = sampled - (step - 1);
    Signal peak = side(front).next_peak;
    if (flare_start < static_cast<std::int64_t>(engine_.current_tick())) {
        peak = side(front).current_peak;
    } else if (flare_start < static_cast<std::int64_t>(rules_.opening_grace)) {
        peak = 0;
    }
    return peak * wave / 1000;
}

bool Match::storm_active() const {
    const Tick now = engine_.current_tick();
    return now >= storm_start_ && now < storm_end();
}

bool Match::storm_forecast() const {
    const Tick now = engine_.current_tick();
    return now + rules_.storm_warning >= storm_start_ && now < storm_end();
}

Connection& Match::line(Front front) { return engine_.graph().connections[side(front).line]; }

const Connection& Match::line(Front front) const {
    return engine_.graph().connections[side(front).line];
}

std::int64_t Match::wave_step(Front front) const {
    return (static_cast<std::int64_t>(engine_.current_tick()) + side(front).phase) % kWavePeriod;
}

std::uint64_t Match::next_random() {
    // splitmix64: tiny, fast, and identical on every platform.
    std::uint64_t z = (rng_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

Signal Match::roll_peak() {
    const auto escalation = static_cast<Signal>(engine_.current_tick() / rules_.escalation_period);
    const Signal base =
        std::min(rules_.base_amplitude + escalation * rules_.escalation_step, rules_.max_amplitude);
    flare_base_ = base;

    const std::int64_t span = 2 * rules_.flare_jitter_per_mille + 1;
    const std::int64_t jitter =
        static_cast<std::int64_t>(next_random() % static_cast<std::uint64_t>(span)) -
        rules_.flare_jitter_per_mille;
    return base * (1000 + jitter) / 1000;
}

void Match::schedule_storm(Tick after) {
    const Tick span = rules_.storm_gap_max - rules_.storm_gap_min + 1;
    storm_start_ = after + rules_.storm_gap_min + next_random() % span;
}

} // namespace netwar
