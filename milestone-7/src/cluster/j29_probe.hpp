// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
// A PARKED test of the J29 KOMBI's direction enum (ManeuverDescription 0x0F, body[33]), header-only, no I/O.
//
// Only 6 (LEFT), 7 (RIGHT) and 10 (destination flag) are car-verified; every roundabout is sent as 14 or 15 and the
// cluster draws one generic roundabout symbol for it. Whether the enum holds an exit-specific roundabout picture is
// not known. With NBT_J29_PROBE=1 the J29 driver shows the values FROM..TO one after the other, S seconds each, in
// place of the phone's direction, and writes the number into the road-name line of the cluster so a photo or a video
// of the cluster says which value it shows. Off unless NBT_J29_PROBE=1. Not for driving: use it with the car parked.
//
//   NBT_J29_PROBE=1        turns it on
//   NBT_J29_PROBE_FROM=1   first value (0..255)
//   NBT_J29_PROBE_TO=47    last value (FROM..255); FROM = TO holds one value
//   NBT_J29_PROBE_S=10     seconds per value (3..120)
#ifndef CLUSTER_J29_PROBE_HPP
#define CLUSTER_J29_PROBE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

namespace nbt {
namespace kombinav {

// What the code knows about a direction value, for the log: the KOMBI enum name where there is one, and whether a car
// has shown it.
inline const char* probeName(int d, bool* verified) {
    if (verified) *verified = (d == 6 || d == 7 || d == 10);
    switch (d) {
        case 1: return "STRAIGHT_ON";
        case 2: return "HALF_LEFT";
        case 3: return "HALF_RIGHT";
        case 4: return "TIGHT_LEFT";
        case 5: return "TIGHT_RIGHT";
        case 6: return "LEFT";
        case 7: return "RIGHT";
        case 8: return "UTURN_LEFT";
        case 9: return "UTURN_RIGHT";
        case 10: return "DESTINATION_FLAG";
        case 14: return "ROUNDABOUT_CW";
        case 15: return "ROUNDABOUT_CCW";
        case 25: return "HIGHWAY_IN_LEFT";
        case 26: return "HIGHWAY_IN_RIGHT";
        case 28: return "HIGHWAY_OUT_LEFT";
        case 29: return "HIGHWAY_OUT_RIGHT";
        case 32: return "LANE_CHANGE_LEFT";
        case 33: return "LANE_CHANGE_RIGHT";
        case 37: return "FORK_LEFT";
        case 38: return "FORK_RIGHT";
        default: return "unknown";
    }
}

class DirectionProbe {
public:
    DirectionProbe() : on_(false), from_(1), to_(47), step_s_(10), cur_(1), next_at_(0) {}

    // The four environment values (any may be NULL). A value that is not a whole number in its range is ignored with
    // a note, and the default stays. Anything but NBT_J29_PROBE=1 leaves the probe off.
    static DirectionProbe fromEnv(const char* on, const char* from, const char* to, const char* secs,
                                  std::string* note) {
        DirectionProbe p;
        if (!on || strcmp(on, "1") != 0) return p;
        p.on_ = true;
        long v;
        std::string n;
        if (from && *from) {
            if (number(from, 0, 255, v)) p.from_ = (int)v;
            else n += " NBT_J29_PROBE_FROM must be 0..255 (kept 1);";
        }
        if (to && *to) {
            if (number(to, 0, 255, v)) p.to_ = (int)v;
            else n += " NBT_J29_PROBE_TO must be 0..255 (kept 47);";
        }
        if (secs && *secs) {
            if (number(secs, 3, 120, v)) p.step_s_ = (int)v;
            else n += " NBT_J29_PROBE_S must be 3..120 (kept 10);";
        }
        if (p.to_ < p.from_) {
            n += " NBT_J29_PROBE_TO is below FROM: holding FROM;";
            p.to_ = p.from_;
        }
        p.cur_ = p.from_;
        if (note) *note = n;
        return p;
    }

    bool on() const { return on_; }
    int value() const { return cur_; }
    int from() const { return from_; }
    int to() const { return to_; }
    int seconds() const { return step_s_; }
    unsigned count() const { return (unsigned)(to_ - from_ + 1); }

    // The next value is due: step_s_ seconds since the current one was drawn.
    bool due(double now) const { return on_ && from_ < to_ && now >= next_at_; }
    // The current value was drawn at `now`.
    void drawn(double now) { next_at_ = now + step_s_; }
    // Moves to the next value; true when it wrapped from TO back to FROM (a new sweep).
    bool advance() {
        if (cur_ >= to_) { cur_ = from_; return true; }
        ++cur_;
        return false;
    }

    // The text shown in the cluster's road-name line while the value is up.
    std::string road() const {
        char b[24];
        snprintf(b, sizeof(b), "PROBE %d", cur_);
        return b;
    }

private:
    static bool number(const char* s, long lo, long hi, long& out) {
        char* end = NULL;
        long v = strtol(s, &end, 10);
        if (end == s || *end || v < lo || v > hi) return false;
        out = v;
        return true;
    }
    bool on_;
    int from_, to_, step_s_, cur_;
    double next_at_;
};

} // namespace kombinav
} // namespace nbt

#endif
