// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
#ifndef NBT_AA_SURFACE_POLL_HPP
#define NBT_AA_SURFACE_POLL_HPP

// When VideoManager::serviceAaSurface() may ask LayerManager whether Android Auto's surface exists yet.
//
// Each probe is a popen() of LayerManagerControl: a shell plus a process that loads the head unit's graphics stack
// (main.cpp, NBT_CHILD_NO_PRELOAD). Until v1.9.45 the probe ran every other 250 ms tick for as long as the surface was
// missing, so a car where LayerManager never creates it (or prints its answer differently) forked two processes a
// second for the rest of the run. Now: every 500 ms for the first 10 s (the surface normally appears within ~500 ms),
// then every 5 s until 60 s after the window was created, then not again until a new window resets the tick count.
//
// `tick` is the number of serviceAaSurface() calls since the window was created (first call = 1).

namespace nbt {

static const unsigned kAaSurfaceTickMs = 250;
static const unsigned kAaSurfaceFastTicks = 40;      // 10 s: probe every 2nd tick (500 ms) up to and including this
static const unsigned kAaSurfaceSlowEvery = 20;      // then every 20th tick (5 s)
static const unsigned kAaSurfaceGiveUpTicks = 240;   // 60 s: the last probe; after this none until the window is remade

inline bool aaSurfacePollDue(unsigned tick) {
    if (tick == 0) return false;
    if (tick <= kAaSurfaceFastTicks) return (tick % 2) == 0;
    if (tick <= kAaSurfaceGiveUpTicks) return (tick % kAaSurfaceSlowEvery) == 0;
    return false;
}

// True on the first tick after the last probe: the caller logs once that it stopped looking.
inline bool aaSurfacePollJustGaveUp(unsigned tick) { return tick == kAaSurfaceGiveUpTicks + 1; }

} // namespace nbt

#endif // NBT_AA_SURFACE_POLL_HPP
