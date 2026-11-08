// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
#ifndef NBT_SPP_REREG_HPP
#define NBT_SPP_REREG_HPP

// When to register the Android Auto Bluetooth service again while no phone opens the Bluetooth setup link.
//
// MINI F60 report (v1.8.35, 2026-10-29): after a cold start, and after the head unit went to sleep and the phone
// closed the link, the phone stayed connected over Bluetooth (A2DP music kept playing) but never opened the Android
// Auto link, so /dev/btspp0 never appeared. Restart from the iDrive menu fixed it within a second; the only thing
// Restart changes on the Bluetooth side is SppUuidDeregister + SppUuidRegister while the phone is connected. The
// daemon now does that by itself: every NBT_SPP_REREG_S (default 5) seconds for the first 2 minutes without a link,
// then every 30 s, so a car with no Android phone around (iPhone drive) is not churned. 0 turns it off.
//
// v1.9.46: the slow phase ends too. Until then the 30 s cycle ran for as long as the daemon had no link: with
// "Start automatically" and no Android phone that was ~2,900 deregister/register pairs a day sent to the car's
// Bluetooth stack, for no one. Now an idle period re-registers for at most NBT_SPP_REREG_MAX_S seconds (default 720
// = the 2 min fast phase plus 10 min at 30 s; 0 = no limit, the old behaviour) and then stops until the next idle
// period begins (a link or session ends, the access point comes back). The service stays registered throughout: the
// re-register is only a kick for a phone that is connected but not opening the link, and a phone arriving later
// finds the service as it is.

#include <stdint.h>
#include <stdlib.h>

namespace nbt {

static const int kSppReregFastPhaseSec = 120;   // retry at the short interval for this long after a link went away
static const int kSppReregSlowSec = 30;         // then this often
static const int kSppReregMaxSecDefault = 720;  // ... and for this long in total, then stop until the next idle period

// Seconds to wait before re-registration number `attempt` (0-based) of one idle period.
inline int sppReregIntervalSec(int attempt, int base) {
    if (base <= 0) return 0;
    if (base >= kSppReregSlowSec) return base;
    int fastTries = kSppReregFastPhaseSec / base;
    return attempt < fastTries ? base : kSppReregSlowSec;
}

// NBT_SPP_REREG_S: short interval in seconds (default 5, 0 = off, max 3600).
inline int sppReregBaseSec() {
    const char* e = getenv("NBT_SPP_REREG_S");
    if (!e || !*e) return 5;
    int v = atoi(e);
    if (v <= 0) return 0;
    return v > 3600 ? 3600 : v;
}

// NBT_SPP_REREG_MAX_S: how long one idle period keeps re-registering, in seconds (default 720, 0 = no limit,
// max 86400).
inline int sppReregMaxSec() {
    const char* e = getenv("NBT_SPP_REREG_MAX_S");
    if (!e || !*e) return kSppReregMaxSecDefault;
    int v = atoi(e);
    if (v <= 0) return 0;
    return v > 86400 ? 86400 : v;
}

// Idle-period timer: reset() when waiting for a link starts (start-up, end of a link/session, access point back),
// due() returns true once per interval and moves on to the next, longer one, until the period's budget is spent.
class SppReregTimer {
public:
    SppReregTimer() : base_(0), max_(kSppReregMaxSecDefault), attempt_(0), since_(0), start_(0), gave_up_(false) {}
    void configure(int baseSec, int maxSec = kSppReregMaxSecDefault) {
        base_ = baseSec > 0 ? baseSec : 0;
        max_ = maxSec > 0 ? maxSec : 0;
    }
    bool enabled() const { return base_ > 0; }
    void reset(uint32_t nowSec) { attempt_ = 0; since_ = nowSec; start_ = nowSec; gave_up_ = false; }
    bool due(uint32_t nowSec) {
        if (!base_ || gave_up_) return false;
        if ((uint32_t)(nowSec - since_) < (uint32_t)sppReregIntervalSec(attempt_, base_)) return false;
        if (max_ > 0 && (uint32_t)(nowSec - start_) >= (uint32_t)max_) { gave_up_ = true; return false; }
        since_ = nowSec;
        ++attempt_;
        return true;
    }
    int attempts() const { return attempt_; }
    int nextIntervalSec() const { return sppReregIntervalSec(attempt_, base_); }
    int maxSec() const { return max_; }
    // True once this idle period's budget is spent: due() stays false until reset(). Set on the due() call that
    // would otherwise have fired, so a caller can log it once right there.
    bool gaveUp() const { return gave_up_; }

private:
    int base_;
    int max_;
    int attempt_;
    uint32_t since_;    // the last re-registration (or the reset)
    uint32_t start_;    // the reset: when this idle period began
    bool gave_up_;
};

} // namespace nbt

#endif // NBT_SPP_REREG_HPP
