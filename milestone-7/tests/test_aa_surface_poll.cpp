// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
// Host test for src/aa_surface_poll.hpp: how often serviceAaSurface() may fork LayerManagerControl while Android
// Auto's surface is missing, and that it stops.
#include "aa_surface_poll.hpp"
#include <assert.h>
#include <stdio.h>
using namespace nbt;

int main() {
    assert(!aaSurfacePollDue(0));                       // no call yet

    // First 10 s: every other tick (500 ms), as before.
    unsigned fast = 0;
    for (unsigned t = 1; t <= kAaSurfaceFastTicks; ++t) {
        bool due = aaSurfacePollDue(t);
        assert(due == ((t % 2) == 0));
        if (due) ++fast;
    }
    assert(fast == 20);
    assert(aaSurfacePollDue(40));                       // the 10 s warning tick still probes

    // 10 s to 60 s: every 5 s.
    unsigned slow = 0, last = 0;
    for (unsigned t = kAaSurfaceFastTicks + 1; t <= kAaSurfaceGiveUpTicks; ++t) {
        if (aaSurfacePollDue(t)) {
            if (last) assert(t - last == kAaSurfaceSlowEvery);
            last = t;
            ++slow;
        }
    }
    assert(slow == 10);
    assert(last == kAaSurfaceGiveUpTicks);              // the last probe is at 60 s exactly

    // After 60 s: never again, however long the run (a day of ticks here).
    for (unsigned t = kAaSurfaceGiveUpTicks + 1; t <= 24u * 3600u * 1000u / kAaSurfaceTickMs; ++t)
        assert(!aaSurfacePollDue(t));

    // The give-up notice fires on exactly one tick, the one after the last probe.
    unsigned notices = 0;
    for (unsigned t = 0; t <= 100000; ++t) if (aaSurfacePollJustGaveUp(t)) { ++notices; assert(t == kAaSurfaceGiveUpTicks + 1); }
    assert(notices == 1);

    // Total forks per missing surface per window: 30, where the old code did 2 a second for the rest of the run.
    unsigned total = 0;
    for (unsigned t = 1; t <= 100000; ++t) if (aaSurfacePollDue(t)) ++total;
    assert(total == 30);

    printf("test_aa_surface_poll OK\n");
    return 0;
}
