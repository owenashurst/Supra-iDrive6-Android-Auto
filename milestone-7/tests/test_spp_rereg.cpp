// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
// Host test for src/spp_rereg.hpp: the Bluetooth re-registration schedule while no phone opens the link, and the
// v1.9.46 budget that ends it.
#include "spp_rereg.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
using namespace nbt;

// Runs the timer second by second from `from` for `secs` seconds; returns how often it fired and the last fire time.
static int run(SppReregTimer& t, uint32_t from, uint32_t secs, uint32_t* lastFire, int* firstGap, int* lastGap) {
    int fired = 0;
    uint32_t prev = from;
    for (uint32_t s = 0; s <= secs; ++s) {
        uint32_t now = from + s;
        if (t.due(now)) {
            int gap = (int)(now - prev);
            if (fired == 0 && firstGap) *firstGap = gap;
            if (lastGap) *lastGap = gap;
            prev = now;
            ++fired;
            if (lastFire) *lastFire = now;
        }
    }
    return fired;
}

int main() {
    // Default schedule, base 5, budget 720: 5 s steps for 2 min (24 fires), 30 s steps after that, nothing once the
    // period is 720 s old, no matter how long the daemon waits.
    {
        SppReregTimer t;
        t.configure(5, 720);
        assert(t.enabled() && t.maxSec() == 720);
        const uint32_t start = 1000;
        t.reset(start);
        uint32_t fires[64]; int n = 0;
        for (uint32_t now = start; now <= start + 100000; ++now) if (t.due(now)) { assert(n < 64); fires[n++] = now; }
        assert(n == 24 + 19);                            // 1005..1120 by 5, then 1150..1690 by 30
        for (int i = 0; i < 24; ++i) assert(fires[i] == start + 5u * (i + 1));
        for (int i = 24; i < n; ++i) assert(fires[i] == fires[i - 1] + 30);
        assert(fires[n - 1] < start + 720);
        assert(t.gaveUp());
        assert(t.attempts() == n);
        // A new idle period starts the schedule again.
        t.reset(start + 200000);
        assert(!t.gaveUp());
        assert(!t.due(start + 200004));
        assert(t.due(start + 200005));
        assert(t.attempts() == 1);
    }
    printf("budget: ok\n");

    // The budget is checked only when a fire is due: a period shorter than the budget ends with no give-up.
    {
        SppReregTimer t;
        t.configure(5, 720);
        t.reset(0);
        uint32_t last = 0; int fg = 0, lg = 0;
        int fired = run(t, 0, 600, &last, &fg, &lg);
        assert(fired == 24 + 16 && fg == 5 && lg == 30 && last == 600);
        assert(!t.gaveUp());
    }
    printf("short period: ok\n");

    // Budget 0: the old behaviour, every 30 s for as long as it takes.
    {
        SppReregTimer t;
        t.configure(5, 0);
        assert(t.maxSec() == 0);
        t.reset(0);
        uint32_t last = 0; int lg = 0;
        int fired = run(t, 0, 100000, &last, NULL, &lg);
        assert(fired == 24 + (100000 - 120) / 30);
        assert(lg == 30 && !t.gaveUp());
    }
    printf("no limit: ok\n");

    // Base 0: off. Base at or above the slow interval: that interval throughout.
    {
        SppReregTimer t;
        t.configure(0, 720);
        assert(!t.enabled());
        t.reset(0);
        assert(run(t, 0, 10000, NULL, NULL, NULL) == 0 && !t.gaveUp());

        SppReregTimer u;
        u.configure(60, 720);
        u.reset(0);
        int fg = 0, lg = 0;
        int fired = run(u, 0, 10000, NULL, &fg, &lg);
        assert(fired == 11 && fg == 60 && lg == 60);     // 60..660; 720 is the budget
        assert(u.gaveUp());
    }
    printf("off and slow base: ok\n");

    // Seconds counters wrap (uint32 monotonic seconds): a period across the wrap still fires on time and still ends.
    {
        SppReregTimer t;
        t.configure(5, 720);
        const uint32_t start = 0xFFFFFFF0u;
        t.reset(start);
        uint32_t fires[64]; int n = 0;
        for (uint32_t s = 0; s <= 2000; ++s) { uint32_t now = start + s; if (t.due(now)) { assert(n < 64); fires[n++] = now; } }
        assert(n == 24 + 19);
        assert(fires[0] == start + 5 && fires[3] == 4u);  // 0xFFFFFFF0 + 20 wrapped
        assert(t.gaveUp());
    }
    printf("wraparound: ok\n");

    // The environment knobs.
    {
        unsetenv("NBT_SPP_REREG_MAX_S");
        assert(sppReregMaxSec() == 720);
        setenv("NBT_SPP_REREG_MAX_S", "", 1);
        assert(sppReregMaxSec() == 720);
        setenv("NBT_SPP_REREG_MAX_S", "0", 1);
        assert(sppReregMaxSec() == 0);
        setenv("NBT_SPP_REREG_MAX_S", "300", 1);
        assert(sppReregMaxSec() == 300);
        setenv("NBT_SPP_REREG_MAX_S", "999999", 1);
        assert(sppReregMaxSec() == 86400);
        unsetenv("NBT_SPP_REREG_MAX_S");

        unsetenv("NBT_SPP_REREG_S");
        assert(sppReregBaseSec() == 5);
        setenv("NBT_SPP_REREG_S", "0", 1);
        assert(sppReregBaseSec() == 0);
        setenv("NBT_SPP_REREG_S", "10", 1);
        assert(sppReregBaseSec() == 10);
        unsetenv("NBT_SPP_REREG_S");

        // The schedule function on its own.
        assert(sppReregIntervalSec(0, 5) == 5 && sppReregIntervalSec(23, 5) == 5 && sppReregIntervalSec(24, 5) == 30);
        assert(sppReregIntervalSec(0, 0) == 0 && sppReregIntervalSec(0, 45) == 45 && sppReregIntervalSec(1 << 20, 45) == 45);
    }
    printf("env: ok\n");

    printf("test_spp_rereg OK\n");
    return 0;
}
