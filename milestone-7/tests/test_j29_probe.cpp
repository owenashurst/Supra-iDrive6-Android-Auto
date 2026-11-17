// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
// Host test: the parked J29 direction probe (cluster/j29_probe.hpp and its use in cluster/j29_display.cpp).
// No sockets: the display's link is a fake that records what would go to the KOMBI.
#include "cluster/j29_display.hpp"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

using namespace nbt::kombinav;

// ---- the probe's own logic -----------------------------------------------------------------------------------------

static void parsing() {
    std::string note;
    assert(!DirectionProbe::fromEnv(NULL, NULL, NULL, NULL, &note).on());
    assert(!DirectionProbe::fromEnv("0", NULL, NULL, NULL, &note).on());
    assert(!DirectionProbe::fromEnv("yes", NULL, NULL, NULL, &note).on());

    DirectionProbe d = DirectionProbe::fromEnv("1", NULL, NULL, NULL, &note);
    assert(d.on() && d.from() == 1 && d.to() == 47 && d.seconds() == 10 && d.value() == 1 && d.count() == 47);
    assert(note.empty());

    d = DirectionProbe::fromEnv("1", "11", "24", "15", &note);
    assert(d.from() == 11 && d.to() == 24 && d.seconds() == 15 && d.value() == 11 && d.count() == 14);

    d = DirectionProbe::fromEnv("1", "abc", "300", "1", &note);   // all three refused: the defaults stay, with a note
    assert(d.from() == 1 && d.to() == 47 && d.seconds() == 10);
    assert(note.find("FROM") != std::string::npos && note.find("TO") != std::string::npos &&
           note.find("_S") != std::string::npos);

    d = DirectionProbe::fromEnv("1", "30", "20", NULL, &note);    // TO below FROM holds FROM
    assert(d.from() == 30 && d.to() == 30 && d.value() == 30);
    assert(note.find("holding") != std::string::npos);

    d = DirectionProbe::fromEnv("1", "0", "255", "120", &note);   // the ends of every range are accepted
    assert(d.from() == 0 && d.to() == 255 && d.seconds() == 120 && note.empty());
}

static void sequence() {
    std::string note;
    DirectionProbe d = DirectionProbe::fromEnv("1", "5", "7", "3", &note);
    assert(d.value() == 5 && d.road() == "PROBE 5");
    d.drawn(100.0);
    assert(!d.due(102.9) && d.due(103.0));
    assert(!d.advance() && d.value() == 6 && d.road() == "PROBE 6");
    assert(!d.advance() && d.value() == 7);
    assert(d.advance() && d.value() == 5);                          // wrapped: a new sweep

    DirectionProbe hold = DirectionProbe::fromEnv("1", "14", "14", "3", &note);
    hold.drawn(0.0);
    assert(!hold.due(1000.0));                                      // one value held: never moves on
    DirectionProbe off;
    assert(!off.on() && !off.due(1000.0));
}

static void names() {
    bool v = false;
    assert(!strcmp(probeName(14, &v), "ROUNDABOUT_CW") && !v);
    assert(!strcmp(probeName(6, &v), "LEFT") && v);
    assert(!strcmp(probeName(10, &v), "DESTINATION_FLAG") && v);
    assert(!strcmp(probeName(20, &v), "unknown") && !v);
}

// ---- the display ---------------------------------------------------------------------------------------------------

struct Rig {
    std::vector<std::vector<Frame> > sent;
    std::vector<std::string> log;
    KombiClient::Result next;       // what the link answers
    int reject_dir;                 // answer kRejected to a send whose direction is this (-1 = none)
    Rig() : next(KombiClient::kOk), reject_dir(-1) {}
};

static int directionOf(const std::vector<Frame>& f) {
    for (size_t i = 0; i < f.size(); ++i)
        if (f[i].method == 0x000A && modelType(f[i].body) == 15 && modelIndex(f[i].body) == 0 && f[i].body.size() > 33)
            return f[i].body[33];
    return -1;
}

static bool hasText(const std::vector<Frame>& frames, const std::string& s) {   // UTF-16BE, as the road name is
    Bytes pat;
    for (size_t i = 0; i < s.size(); ++i) { pat.push_back(0); pat.push_back((uint8_t)s[i]); }
    for (size_t i = 0; i < frames.size(); ++i) {
        const Bytes& b = frames[i].body;
        for (size_t j = 0; j + pat.size() <= b.size(); ++j)
            if (!memcmp(&b[j], &pat[0], pat.size())) return true;
    }
    return false;
}

static KombiClient::Result sendFn(void* u, const std::vector<Frame>& f, std::string*) {
    Rig* r = static_cast<Rig*>(u);
    r->sent.push_back(f);
    if (r->reject_dir >= 0 && directionOf(f) == r->reject_dir) return KombiClient::kRejected;
    return r->next;
}
static bool cancelFn(void*, std::string*) { return true; }
static void logFn(const char* l, void* u) { static_cast<Rig*>(u)->log.push_back(l); }

static bool logged(const Rig& r, const char* part) {
    for (size_t i = 0; i < r.log.size(); ++i)
        if (r.log[i].find(part) != std::string::npos) return true;
    return false;
}

static NavView roundabout() {
    NavView v;
    v.aa_type = 33;            // a clockwise roundabout: sent as direction 14 without the probe
    v.road = "Test Road";
    v.dist_text = "300";
    v.aa_unit = 1;
    return v;
}

static void offIsUnchanged() {
    Graph g(0xC8, 255);
    Rig rig;
    Link l; l.send = &sendFn; l.cancel = &cancelFn; l.user = &rig;
    J29Display d(g, l);
    d.setLog(&logFn, &rig);
    Settings s;
    d.setSettings(s);
    NavView v = roundabout();
    assert(d.showManeuver(v, 1.0, true, true) == ClusterDisplay::kOk);
    assert(rig.sent.size() == 1 && directionOf(rig.sent[0]) == 14);   // the phone's type, as before
    assert(hasText(rig.sent[0], "Test Road") && !hasText(rig.sent[0], "PROBE"));
    assert(d.keepUp(v, 100.0) == ClusterDisplay::kOk && rig.sent.size() == 1);   // nothing cycles
    assert(!logged(rig, "[J29Probe]"));
    // a rejection still ends the session as it always did
    rig.next = KombiClient::kRejected;
    assert(d.showManeuver(v, 2.0, true, true) == ClusterDisplay::kRejected);
}

static void cyclesSlowlyInPlace() {
    Graph g(0xC8, 255);
    Rig rig;
    Link l; l.send = &sendFn; l.cancel = &cancelFn; l.user = &rig;
    J29Display d(g, l);
    d.setLog(&logFn, &rig);
    Settings s;
    s.inplace = 1;
    d.setSettings(s);
    std::string note;
    d.setProbe(DirectionProbe::fromEnv("1", "5", "7", "3", &note));
    NavView v = roundabout();

    assert(d.showManeuver(v, 10.0, true, true) == ClusterDisplay::kOk);     // the first draw: the whole graph
    assert(rig.sent.size() == 1 && directionOf(rig.sent[0]) == 5);          // the probe's value, not 14
    assert(hasText(rig.sent[0], "PROBE 5") && !hasText(rig.sent[0], "Test Road"));
    assert(logged(rig, "[J29Probe] direction 5 (TIGHT_RIGHT) sent now, phone type 33"));

    assert(d.keepUp(v, 12.9) == ClusterDisplay::kOk && rig.sent.size() == 1);   // 3 s not up yet
    assert(d.keepUp(v, 13.0) == ClusterDisplay::kOk && rig.sent.size() == 2);   // the next value, in place
    assert(directionOf(rig.sent[1]) == 6 && hasText(rig.sent[1], "PROBE 6"));
    assert(rig.sent[1].size() < rig.sent[0].size());                            // the update block, not the route start
    assert(logged(rig, "direction 6 (LEFT, car-verified)"));

    assert(d.keepUp(v, 16.0) == ClusterDisplay::kOk && directionOf(rig.sent[2]) == 7);
    assert(d.keepUp(v, 19.0) == ClusterDisplay::kOk && directionOf(rig.sent[3]) == 5);   // the sweep starts again
    assert(logged(rig, "sweep complete"));

    // a phone maneuver change in the middle does not bring the phone's direction back
    NavView w = v;
    w.aa_type = 7;
    assert(d.showManeuver(w, 20.0, true, false) == ClusterDisplay::kOk);
    assert(directionOf(rig.sent.back()) == 5 && hasText(rig.sent.back(), "PROBE 5"));
}

static void rejectedValueIsSkipped() {
    Graph g(0xC8, 255);
    Rig rig;
    Link l; l.send = &sendFn; l.cancel = &cancelFn; l.user = &rig;
    J29Display d(g, l);
    d.setLog(&logFn, &rig);
    Settings s;
    s.inplace = 1;
    d.setSettings(s);
    std::string note;
    d.setProbe(DirectionProbe::fromEnv("1", "11", "13", "3", &note));
    NavView v = roundabout();

    assert(d.showManeuver(v, 0.0, true, true) == ClusterDisplay::kOk);      // 11 is drawn
    rig.reject_dir = 12;                                                    // the KOMBI refuses 12
    assert(d.keepUp(v, 3.0) == ClusterDisplay::kOk);                        // ... and the session carries on
    assert(logged(rig, "direction 12 REJECTED"));
    rig.reject_dir = -1;
    assert(d.keepUp(v, 6.0) == ClusterDisplay::kOk);
    assert(directionOf(rig.sent.back()) == 13);                             // the next value follows
}

int main() {
    parsing();
    sequence();
    names();
    offIsUnchanged();
    cyclesSlowlyInPlace();
    rejectedValueIsSkipped();
    printf("test_j29_probe: ok\n");
    return 0;
}
