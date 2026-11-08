# Fixes in milestone-7

These are the source changes in the `milestone-7` folder.
A QNX build is still required before a head unit can run them. The daemon and the iDrive app
need to be this build together: the menu asks the running daemon with a ping, and an older
plugin still tries to bind the control port itself.

The MINI 8.8 screen profile and the Bluetooth audio route are kept.
Any `1180x480` panel becomes `1280x480` with DPI 152 and 40 px side insets. Bluetooth audio
uses connection `0xc7` on every car, including Supra. `NBT_BT_CONN=0` falls back to the focus
request. Also unchanged: the timed Bluetooth re-register, the strike-and-hold for a link that
never speaks Android Auto, the `12 34 56 78` BMW-app startup check, CarPlay geometry restore,
the USB log, and the "start with the car" boot-loop guard.

## What changed

1. **A second phone connection no longer replaces the live session.** While a session is up, a new TCP connection is closed. If a session is replaced on purpose, video, audio, and the microphone are torn down first. The listener still prefers the access-point address (`172.16.224.1`, or `NBT_AP_IP`) and refuses a client that is not on that subnet.

2. **Turning touch forwarding off no longer stops the knob, the microphone, or screen handoff.** The wake pipe is always created. Touch events stay off when `NBT_TOUCH_FWD=0`. Input binding on channels 3, 5, and 7 is still accepted, because the phone waits for that answer before it will use the channel.

3. **A fast microphone or screen-focus change is no longer dropped.** Those requests used to share one slot. The main loop could read a value and store zero in the same moment a newer request arrived. They now sit in a short queue.

4. **The BMW-app scan no longer runs for the whole Bluetooth session.** Only the first message of a new link is inspected, and the `DEADBEEF` / `13371337` magic is recognised only at payload offset 8. A later Android Auto frame that happens to contain those bytes is left alone. The four-byte `12 34 56 78` startup magic is still treated as the BMW app.

5. **On/off settings are read the same way.** `0`, `off`, `no`, and `false` are off. `1`, `on`, `yes`, and `true` are on. An unset, empty, or unrecognised word keeps that setting's default. A variable that is merely present does not turn a setting on. This covers wireless mode, the knob, Bluetooth audio, touch forwarding, hide-when-idle, gear and park-assist actions, the packet filter, guidance-over-Bluetooth, the control port, the microphone, reverse focus, BACK-hold focus, Bluetooth refocus, and knob invert.

6. **The menu status file is no longer emptied before it is rewritten.** `/dev/shmem/nbt-aad-state` is one fixed-width line (`idle`, `connecting`, or `connected`, then the pid, then the overlay value). The new line is written first, and the file is shortened after that. Overlay `2` (BACK held) and `3` (a menu key just went to the phone) still fit. The iDrive app checks that the daemon is running by sending `ping` and waiting for `pong`. It no longer binds UDP 52790, which could steal the port from the daemon.

7. **A split message is delivered only when it is complete.** If decryption fails, that channel's partial message is dropped. A LAST fragment is delivered only when the bytes on the wire match the size the phone announced on the FIRST fragment.

8. **The CMake file matches the Makefile.** It lists the same sources, links `a2i_client`, `socket`, `ssl`, `crypto`, and `screen`, and does not pass `-fPIC`. The real head-unit build is still the Makefile (`qcc`, `-std=gnu++0x`).

9. **The older and newer iDrive controllers no longer share one button-state array.** A press on the older CCS controller (C20) is stored separately from the CCS35up hard keys (C47/C4F), so a press on one no longer hides a press on the other.

10. **A Bluetooth close with no accepted Wi-Fi start is a failed handshake.** The phone is treated as joining Wi-Fi only after it accepts the start (status field 4 is 0, or field 3 is 0 when field 4 is absent). Closing right after the access-point details, or sitting for 45 seconds with no accept, is a failure. On that failure the daemon drops and re-registers the Android Auto Bluetooth service. The existing timed re-register is still there for a phone that is connected over Bluetooth and never opens the link.

11. **One tap on the screen reaches the phone as one tap.** The touch forwarder keeps the record sequence of each message. A duplicated message is skipped. A repeated "down" with no message lost in between is sent on as a move. A finger the forwarder has already released (it went quiet for `NBT_TOUCH_IDLE_MS`, or it was missing from a message) is ignored while the head unit still reports it, until the hardware sends "up" or a new "down". A move after a message was actually lost can still start a tap, because that touch's "down" may have been in the lost message.

12. **A session whose stream stops decrypting ends instead of freezing (v1.9.46).** The TLS records are AES-GCM with the sender's sequence number as the nonce, so once one record is lost nothing later in the stream decrypts. Until now an established session in that state stayed up for as long as the phone kept sending: the picture froze, every frame wrote three log lines, the socket data kept the no-data timeout from firing, and the listener refused the phone's reconnects because the dead session still held the slot. Now 32 failures in a row (about half a second of video) end the session and the phone reconnects. The first 3 failures of a run are logged in full, then only the end. The rule for a session that never decrypted anything (16 frames, outcome "stale") is unchanged. Host test: `tests/test_session_decrypt_fail.cpp`.

13. **The Bluetooth re-registration stops after 12 minutes without a link (v1.9.46).** While no phone opens the Android Auto Bluetooth link the daemon deregisters and re-registers the service every 5 s for 2 minutes and then every 30 s, as a kick for a phone that is connected but not opening the link. The 30 s phase had no end: with "Start automatically" and no Android phone around that was ~2,900 register pairs a day sent to the car's Bluetooth stack. Each idle period now re-registers for at most `NBT_SPP_REREG_MAX_S` seconds (default 720; `0` = no limit, the old behaviour) and then stops until the next idle period begins (a link or session ends, the access point comes back). The service stays registered throughout. Host test: `tests/test_spp_rereg.cpp` (the schedule itself had no test before).

14. **Video frames are no longer copied on their way through the queue (v1.9.46).** The queue between the session thread and the decoder worker was a `std::vector` of frames: every push copied the frame in, every pop copied it out, and removing the front copy-assigned every frame still waiting, so a full queue cost around 15 deep copies of H.264 data per frame, all under the lock the network thread needs. The queue is now `FrameQueue` (`src/frame_queue.hpp`, a `std::list` whose items are swapped in and out), with the same cap of 15 and the same drop policy (the oldest frame that is neither codec config nor a keyframe makes room; a queue holding only those rejects the new frame; codec config is never subject to the cap). The "queue full" log lines are written after the lock is released. Host test: `tests/test_frame_queue.cpp`, including that the bytes come out of the queue in the same heap block they went in.

15. **The probe for Android Auto's LayerManager surface stops after 60 s (v1.9.46).** `serviceAaSurface()` asked `LayerManagerControl` every 500 ms whether surface 1282 existed yet, and kept asking for the rest of the run if it never did: two forked processes a second, each loading the head unit's graphics stack. Now it asks every 500 ms for 10 s, then every 5 s until 60 s after the window was created, then logs once that it stopped. A new window starts the schedule again. Host test: `tests/test_aa_surface_poll.cpp`.

16. **The hardware video engine connection is closed before it is reopened (v1.9.46).** When `mmr_context_create` failed, `createDecoder()` ran again on the next phone connection and opened a new `/dev/name/local/mdev` connection over the old one without closing it: one leaked connection per connect attempt while mm-renderer was unwell. Not host-testable (QNX `name_open`).

17. **The microphone capture thread no longer spins when A2I hands it a notification without a buffer (v1.9.46).** The playback thread already slept 2 ms and throttled its log in that case; the capture thread looped at once, at full CPU, with a library log line per attempt. It now does the same as playback. Not host-testable (A2I library).

## Checked here

On a Mac, these host tests passed: companion-app detection (`test_bcl_detect`), both iDrive controller replays (`test_ccs_decoder`, `test_ccs_push`), the car-config / guidance-sink test (`test_car_config`), and the touch parser and forwarder (`test_touch_frame`, `test_touch`, including messages split across datagrams). The changed daemon sources also pass a host syntax check. They have not been built with the QNX compiler and have not been run in a car.
