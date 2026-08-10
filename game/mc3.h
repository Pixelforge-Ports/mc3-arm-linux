#pragma once

/*
 * What the parts of this port need from each other.
 *
 * Nothing generic belongs here - portbase owns the loader, the thunks, the
 * fake JVM and the input interface. This is only the handful of facts that are
 * about Modern Combat 3 specifically and are needed in more than one file.
 */

struct so_module;
struct _JNIEnv;

/* The loaded guest library. Same pointer port_guest_module() returns; declared
 * here so the JNI classes can forward to the game's own native methods without
 * threading it through the class registry. */
so_module *mc3_module(void);

/* Fill in the three GDRM lock words. Called from main() right after
 * JNI_OnLoad returns - see game/patch.cpp for why not earlier. */
void mc3_unlock_drm(so_module *mod);

/*
 * Create the directories under the writable storage that the engine writes
 * into without ever creating them. Called from main() before the boot
 * sequence; implemented next to the path rules in game/io_paths.cpp.
 */
void mc3_prepare_writable_storage(void);

/*
 * Mission briefings, skipped.
 *
 * On Android a briefing is an MP4 played by a separate activity: the engine
 * fills an Intent with "video_name"/"language", calls startActivity, and then
 * waits for that activity to call back into the .so through
 * Java_video_MyVideoView_SetFinished. There is no activity here and no media
 * stack behind it, so the skip arms a short countdown and the frame loop
 * delivers the callback a few frames later - delivering it synchronously from
 * inside playMovie reaches the engine before it starts waiting, and it then
 * waits forever (black screen, music playing). Implemented in
 * jni/video_player.cpp; startActivity arms it, main.cpp ticks it every frame.
 */
void mc3_video_skip(_JNIEnv *env, const char *video_name, const char *language);
void mc3_video_skip_tick(void);

/*
 * A swipe gesture, as the campaign's cover/vault prompts ask for: 0 up,
 * 1 down, 2 left, 3 right. Queued here (from the dpad, or an emulator
 * command) and drawn over the following frames by the input bridge's tick.
 */
void android_input_request_swipe(int dir);

/*
 * Whether the engine says it is in a mission rather than in a menu.
 *
 * The game exports this (GloftM3HM.isGamePlay) and the reference port polls it
 * to decide whether a button press should reach the game. Here it is the one
 * cheap way to tell "the port drew 600 frames of the title screen" apart from
 * "the port is playing", which a frame counter cannot. Implemented in
 * game/input_bridge.cpp, next to the symbol it resolves.
 */
bool mc3_is_gameplay(void);
