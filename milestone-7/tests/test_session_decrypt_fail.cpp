// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
// An established session whose encrypted frames stop decrypting (a TLS stream that lost a record) ends by itself
// after kMaxUndecryptedRun failures in a row, so the phone can reconnect. Until v1.9.45 it stayed up, frozen, for as
// long as the phone kept sending. Built with stub_cryptor.hpp (see tests/Makefile): "encrypted" frames are plain text
// and the stub's decrypt() fails on demand.
#include "aa_session.hpp"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
using namespace nbt;

static const int kRun = 32;   // kMaxUndecryptedRun in aa_session.cpp

static volatile int g_ended = 0;
static void onEnded(void*) { __sync_fetch_and_add(&g_ended, 1); }
static int ended() { return __sync_fetch_and_add(&g_ended, 0); }

static void readN(int fd, uint8_t* out, size_t n) {
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, out + got, n - got);
        assert(r > 0);
        got += (size_t)r;
    }
}

// An encrypted control frame nobody handles: with the stub it decrypts (to itself) unless the stub is set to fail.
static void sendFrames(int phone, int n) {
    const uint8_t frame[6] = { 0, 0x0B, 0, 2, 0x7F, 0x7F };
    for (int i = 0; i < n; ++i) assert(write(phone, frame, sizeof(frame)) == (ssize_t)sizeof(frame));
}

// Starts a session on a socketpair and reads the VersionRequest it sends. Returns the phone's end.
static int startSession(AASession& s) {
    int sv[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
    assert(s.start(sv[0]));
    uint8_t version[12];
    readN(sv[1], version, sizeof(version));
    return sv[1];
}

// Past the VersionRequest and with one frame decrypted, and the session thread has seen it.
static int establish(AASession& s) {
    int phone = startSession(s);
    sendFrames(phone, 1);
    for (int w = 0; w < 400 && s.outcome() != AASession::kEstablished; ++w) usleep(5000);
    assert(s.outcome() == AASession::kEstablished);
    return phone;
}

static void waitEnded(AASession& s, int before) {
    for (int w = 0; w < 400 && (ended() <= before || s.isActive()); ++w) usleep(5000);
    assert(ended() > before);
    assert(!s.isActive());
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    AASession s;
    s.setDisconnectCallback(&onEnded, NULL);

    // Established, then the stream stops decrypting: one short of the run keeps the session up, the next frame ends
    // it. It was an established session, so the outcome is not "stale" (no USB reset for this).
    {
        int phone = establish(s);
        stubDecryptFail() = true;
        sendFrames(phone, kRun - 1);
        usleep(300000);
        assert(s.isActive());
        int before = ended();
        sendFrames(phone, 1);
        waitEnded(s, before);
        assert(s.outcome() == AASession::kEstablished);
        stubDecryptFail() = false;
        s.stop();
        close(phone);
    }
    printf("run ends the session: ok\n");

    // A frame that decrypts in between starts the count again.
    {
        int phone = establish(s);
        stubDecryptFail() = true;
        sendFrames(phone, kRun - 12);
        usleep(200000);
        stubDecryptFail() = false;
        sendFrames(phone, 1);
        usleep(300000);                                  // let the session thread take it before failing again
        stubDecryptFail() = true;
        sendFrames(phone, kRun - 1);
        usleep(300000);
        assert(s.isActive());                            // 20 + 31 failures, but not 32 in a row
        int before = ended();
        sendFrames(phone, 1);
        waitEnded(s, before);
        stubDecryptFail() = false;
        s.stop();
        close(phone);
    }
    printf("success resets the run: ok\n");

    // The rule for a session that never decrypted anything is unchanged: 16 such frames end it as stale.
    {
        int phone = startSession(s);
        stubDecryptFail() = true;
        int before = ended();
        sendFrames(phone, 16);
        waitEnded(s, before);
        assert(s.outcome() == AASession::kStale);
        stubDecryptFail() = false;
        s.stop();
        close(phone);
    }
    printf("stale rule unchanged: ok\n");

    // The next session starts clean: a run from the previous one does not carry over.
    {
        int phone = establish(s);
        stubDecryptFail() = true;
        sendFrames(phone, kRun - 1);
        usleep(300000);
        assert(s.isActive());
        stubDecryptFail() = false;
        s.stop();
        close(phone);
        phone = establish(s);
        stubDecryptFail() = true;
        sendFrames(phone, kRun - 1);
        usleep(300000);
        assert(s.isActive());
        stubDecryptFail() = false;
        s.stop();
        close(phone);
    }
    printf("fresh per session: ok\n");

    printf("test_session_decrypt_fail OK\n");
    return 0;
}
