/*
 * android/media/AudioTrack - where the game's mixer output goes.
 *
 * Modern Combat 3 does its own mixing and hands finished PCM to Android one
 * buffer at a time through AudioTrack.write. That makes this class the entire
 * audio output path: nothing else in the binary opens a device.
 *
 * The overload matters and cost this port a silent run. Android has a byte[]
 * form and a short[] form of write(), and they count differently - bytes for
 * one, *samples* for the other. This engine uses the byte[] form, and the
 * donor proves it: "([BII)I" is the only one of the two descriptors present in
 * libModernCombat3.so's .rodata. Registering only "([SII)I" produced
 *
 *     Class AudioTrack does not have method write([BII)I.
 *
 * which is not a quiet degradation - GetMethodID answers NULL, the engine
 * writes into nothing, and its mixer either stalls waiting for a queue that
 * never drains or spins feeding a device that never consumes. Both are
 * registered now; the short[] entry doubles its count before queueing, because
 * SDL_QueueAudio takes bytes and registering one implementation under both
 * descriptors would play half the audio as a stutter.
 *
 * Two more things about this class are already known and are recorded here
 * rather than rediscovered:
 *
 *   - write([SII)I is the method that first hit the ldrd alignment fault
 *     portbase fixes at the va_list template (jni/jni_internals.h). It is the
 *     first method in any of these ports with two adjacent int arguments. That
 *     fix is in the base; nothing is needed here.
 *
 *   - The engine calls getMinBufferSize before it constructs anything and
 *     sizes its own ring from the answer. Returning 0 or a nonsense value is
 *     not a quiet degradation - it is a division by zero inside the mixer.
 */

#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <SDL2/SDL.h>

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "port_env.h"
#include "trace.h"
#include "mc3_classes.h"

/* AudioFormat.ENCODING_PCM_16BIT and CHANNEL_OUT_STEREO, for the log line. */
static const char *encoding_name(jint format)
{
    switch (format) {
    case 2:  return "PCM_16BIT";
    case 3:  return "PCM_8BIT";
    case 4:  return "PCM_FLOAT";
    default: return "unknown";
    }
}

static const char *channels_name(jint config)
{
    switch (config) {
    case 4:  return "MONO";
    case 12: return "STEREO";
    default: return "unknown";
    }
}

static SDL_AudioFormat sdl_format(jint android_format)
{
    switch (android_format) {
    case 2:  return AUDIO_S16SYS;
    case 3:  return AUDIO_U8;
    case 4:  return AUDIO_F32SYS;
    default: return AUDIO_S16SYS;
    }
}

/*
 * One track's output device, and the counters that make the path auditable
 * from the log alone.
 */
struct Mc3AudioSink {
    SDL_AudioSpec     desired;
    SDL_AudioSpec     obtained;
    SDL_AudioDeviceID device;
    int               playing;
    unsigned long     writes;
    unsigned long long bytes;
};

/*
 * Open an output, preferring anything that is not HDMI.
 *
 * Taken from the sibling ports rather than written fresh, because the two
 * failure modes it handles are the device's and not the emulator's: some
 * PortMaster frontends keep the speaker PCM busy for a moment after launching
 * a port, while an unused HDMI endpoint opens immediately and plays into
 * nothing. First pass skips HDMI and waits out "busy"; second pass accepts
 * whatever is left.
 */
static SDL_AudioDeviceID open_output(const SDL_AudioSpec *desired,
                                     SDL_AudioSpec *obtained)
{
    const char *forced = port_getenv("AUDIODEV");
    if (forced && *forced) {
        SDL_AudioDeviceID device =
            SDL_OpenAudioDevice(forced, 0, desired, obtained, 0);
        if (!device)
            trace("AudioTrack: %s=%s failed: %s",
                  port_env_name("AUDIODEV"), forced, SDL_GetError());
        return device;
    }

    SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, desired, obtained, 0);
    if (device)
        return device;

    const char *err = SDL_GetError();
    char default_error[256];
    snprintf(default_error, sizeof(default_error), "%s", err ? err : "unknown error");

    int count = SDL_GetNumAudioDevices(0);
    for (int pass = 0; pass < 2 && !device; pass++) {
        for (int i = 0; i < count && !device; i++) {
            const char *name = SDL_GetAudioDeviceName(i, 0);
            if (!name || !*name)
                continue;
            bool hdmi = strcasestr(name, "hdmi") != NULL;
            if (pass == 0 && hdmi)
                continue;

            int attempts = pass == 0 ? 5 : 1;
            for (int attempt = 0; attempt < attempts; attempt++) {
                device = SDL_OpenAudioDevice(name, 0, desired, obtained, 0);
                if (device)
                    break;
                const char *attempt_error = SDL_GetError();
                if (!attempt_error || !strcasestr(attempt_error, "busy") ||
                    attempt + 1 >= attempts)
                    break;
                SDL_Delay(400);
            }
            if (device)
                trace("AudioTrack: default output failed (%s); opened \"%s\" "
                      "on pass %d", default_error, name, pass + 1);
        }
    }

    if (!device)
        trace("AudioTrack: no output accepted %d Hz/%u ch/0x%x: %s "
              "(%d enumerated device(s))",
              desired->freq, (unsigned int)desired->channels,
              (unsigned int)desired->format, default_error, count);
    return device;
}

static jobject AudioTrack_init(JNIEnv *env, jobject self, jclass clazz,
                               jint stream, jint sample_rate, jint channels,
                               jint format, jint buffer_bytes, jint mode)
{
    (void)env; (void)clazz;

    /* Placement-new rather than trusting the calloc: portbase's NewObject
     * hands out zeroed storage with no vtable, and this is the first class in
     * this port whose instances are dereferenced as objects afterwards. */
    AndroidAudioTrack *track = new (self) AndroidAudioTrack();
    Mc3AudioSink *sink = (Mc3AudioSink *)calloc(1, sizeof(Mc3AudioSink));
    track->sink = sink;

    trace("AudioTrack: stream=%d %d Hz %s %s buffer=%d bytes mode=%d",
          stream, sample_rate, channels_name(channels), encoding_name(format),
          buffer_bytes, mode);

    if (!sink)
        return (jobject)self;

    if (port_getenv_bool("NO_AUDIO", 0)) {
        trace("AudioTrack: %s is set - output stays a discard sink",
              port_env_name("NO_AUDIO"));
        return (jobject)self;
    }

    sink->desired.freq     = sample_rate;
    sink->desired.format   = sdl_format(format);
    sink->desired.channels = (channels == 4) ? 1 : 2;
    /*
     * buffer_bytes is the engine's own producer ring, not a hardware period.
     * Passing it through makes SDL ask for a device buffer measured in
     * seconds; queue-driven playback only wants a conventional period.
     */
    sink->desired.samples  = 1024;
    sink->desired.callback = NULL;

    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) &&
        SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        trace("AudioTrack: SDL audio subsystem failed to initialize: %s",
              SDL_GetError());
        return (jobject)self;
    }

    const char *driver = SDL_GetCurrentAudioDriver();
    int outputs = SDL_GetNumAudioDevices(0);
    trace("AudioTrack: SDL audio ready driver=%s outputs=%d request=%d Hz/%u "
          "ch/0x%x period=%u",
          driver ? driver : "(none)", outputs, sink->desired.freq,
          (unsigned int)sink->desired.channels,
          (unsigned int)sink->desired.format,
          (unsigned int)sink->desired.samples);
    for (int i = 0; i < outputs && i < 8; i++) {
        const char *name = SDL_GetAudioDeviceName(i, 0);
        trace("AudioTrack: output[%d]=%s", i, name ? name : "(null)");
    }

    sink->device = open_output(&sink->desired, &sink->obtained);
    if (sink->device)
        trace("AudioTrack: opened device=%u obtained=%d Hz/%u ch/0x%x period=%u",
              (unsigned int)sink->device, sink->obtained.freq,
              (unsigned int)sink->obtained.channels,
              (unsigned int)sink->obtained.format,
              (unsigned int)sink->obtained.samples);

    return (jobject)self;
}

static jint AudioTrack_getMinBufferSize(JNIEnv *env, jclass clazz,
                                        jint sample_rate, jint channels, jint format)
{
    (void)env; (void)clazz; (void)sample_rate; (void)channels; (void)format;
    /* 4096 bytes, as the reference port answers - one frame's worth at 44.1 kHz
     * stereo 16-bit with room to spare, and a power of two, which is what the
     * engine's ring allocator expects. */
    return 4096;
}

static jint AudioTrack_getNativeOutputSampleRate(JNIEnv *env, jclass clazz, jint stream)
{
    (void)env; (void)clazz; (void)stream;
    return 44100;
}

static Mc3AudioSink *sink_of(jobject self)
{
    AndroidAudioTrack *track = (AndroidAudioTrack *)self;
    return track ? track->sink : NULL;
}

static void AudioTrack_play(JNIEnv *env, jobject self)
{
    (void)env;
    Mc3AudioSink *sink = sink_of(self);
    if (!sink || !sink->device)
        return;
    sink->playing = 1;
    SDL_PauseAudioDevice(sink->device, 0);
}

static void AudioTrack_pause(JNIEnv *env, jobject self)
{
    (void)env;
    Mc3AudioSink *sink = sink_of(self);
    if (!sink || !sink->device)
        return;
    sink->playing = 0;
    SDL_PauseAudioDevice(sink->device, 1);
}

static void AudioTrack_stop(JNIEnv *env, jobject self)
{
    (void)env;
    Mc3AudioSink *sink = sink_of(self);
    if (!sink || !sink->device)
        return;
    sink->playing = 0;
    SDL_PauseAudioDevice(sink->device, 1);
    SDL_ClearQueuedAudio(sink->device);
}

static void AudioTrack_release(JNIEnv *env, jobject self)
{
    (void)env;
    Mc3AudioSink *sink = sink_of(self);
    if (!sink || !sink->device)
        return;
    SDL_ClearQueuedAudio(sink->device);
}

/*
 * The counter line the harness reads.
 *
 * Reported at powers of two rather than on a timer: the log stays bounded over
 * a long run while still giving several early points, which is where a path
 * that queues nothing would show. "writes flow" and "the device accepted them"
 * are separate facts and both are printed.
 */
static void report_writes(Mc3AudioSink *sink, int bytes)
{
    sink->writes++;
    sink->bytes += (unsigned long long)bytes;

    bool milestone = (sink->writes & (sink->writes - 1)) == 0;
    if (sink->writes <= 4 || milestone)
        trace("AudioTrack: writes=%lu bytes=%llu last=%d queued=%u device=%u",
              sink->writes, sink->bytes, bytes,
              sink->device ? SDL_GetQueuedAudioSize(sink->device) : 0u,
              (unsigned int)sink->device);
}

/*
 * Queue one buffer and hold the engine until there is room for the next.
 *
 * Android's blocking write waits for buffer space, not for the speaker to
 * drain: waiting for an empty queue between every block is what produced an
 * audible scheduling gap on the sibling port. Two SDL periods is the
 * high-water mark that stopped it.
 *
 * Returns the count to report back, in the caller's own units - the engine
 * blocks until write() has taken the whole buffer and retries the remainder
 * otherwise, so a short count spins its mixer thread against a sink it thinks
 * never drains. A missing device is reported as fully accepted for the same
 * reason: no endpoint is a reason for silence, not a reason to stall the game.
 */
static jint queue_pcm(jobject self, const void *pcm, int bytes, jint report)
{
    Mc3AudioSink *sink = sink_of(self);
    if (!sink)
        return report;

    report_writes(sink, bytes);

    if (!sink->device) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            trace("AudioTrack: no SDL output device - PCM is being discarded. "
                  "The game runs at the right rate but plays silent.");
        }
        return report;
    }

    if (SDL_QueueAudio(sink->device, pcm, (Uint32)bytes) != 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            trace("AudioTrack: SDL_QueueAudio failed: %s", SDL_GetError());
        }
        return report;
    }

    if (!sink->playing) {
        sink->playing = 1;
        SDL_PauseAudioDevice(sink->device, 0);
    }

    unsigned int bytes_per_frame =
        (SDL_AUDIO_BITSIZE(sink->obtained.format) / 8) * sink->obtained.channels;
    if (bytes_per_frame) {
        unsigned int high_water = sink->obtained.samples * bytes_per_frame * 2;
        while (SDL_GetQueuedAudioSize(sink->device) > high_water)
            SDL_Delay(1);
    }

    return report;
}

/*
 * The array the engine hands over, checked before it is followed.
 *
 * The sibling port learned this the hard way: the first run of the short[]
 * path arrived with an array pointer of 0x83469cb5 and a count of -599784200 -
 * audio sample data being read as arguments, because the method had been
 * registered with a dispatcher shape nothing calls. Dereferencing that is a
 * fault deep inside the audio path with the engine's return address in lr,
 * which reads like engine corruption. Checking costs one compare.
 */
static const void *array_base(ArrayObject *data, jint offset, jint count,
                              jsize element_size, const char *what)
{
    if (!data || data->element_size != element_size || !data->elements) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            trace("AudioTrack.write(%s): array at %p is not one this loader "
                  "allocated (element_size=%d, elements=%p) - dropping the write "
                  "rather than following the pointer.",
                  what, (void *)data, data ? (int)data->element_size : 0,
                  data ? data->elements : NULL);
        }
        return NULL;
    }

    if (offset < 0 || count < 0 ||
        (size_t)offset + (size_t)count > (size_t)data->count) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            trace("AudioTrack.write(%s): range offset=%d count=%d exceeds array "
                  "length=%d - dropping write",
                  what, offset, count, (int)data->count);
        }
        return NULL;
    }

    return (const void *)((uintptr_t)data->elements +
                          (size_t)offset * (size_t)element_size);
}

/* write(byte[], int, int) - the one this engine calls. Counts are bytes. */
static jint AudioTrack_write(JNIEnv *env, jobject self, jclass clazz,
                             jbyteArray buffer, jint offset, jint length)
{
    (void)env; (void)clazz;

    static bool announced = false;
    if (!announced) {
        announced = true;
        trace("AudioTrack.write(byte[]): track=%p data=%p offset=%d length=%d",
              (void *)self, (void *)buffer, offset, length);
    }

    const void *pcm = array_base((ArrayObject *)buffer, offset, length,
                                 (jsize)sizeof(jbyte), "byte[]");
    if (!pcm)
        return length;

    return queue_pcm(self, pcm, length, length);
}

/*
 * write(short[], int, int) - not called by this build, and registered anyway.
 *
 * Two of the four sibling ports use this form and Gameloft ships the same
 * engine under several SKUs, so an MC3 build that asks for it would otherwise
 * fail exactly the way this one failed on the byte[] form: with one line in
 * the log and no sound. The count is in samples, so it doubles on the way to
 * SDL, and the return value goes back in samples.
 */
static jint AudioTrack_write_shorts(JNIEnv *env, jobject self, jclass clazz,
                                    jshortArray buffer, jint offset, jint count)
{
    (void)env; (void)clazz;

    const void *pcm = array_base((ArrayObject *)buffer, offset, count,
                                 (jsize)sizeof(jshort), "short[]");
    if (!pcm)
        return count;

    return queue_pcm(self, pcm, count * 2, count);
}

static jint AudioTrack_getPlayState(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return 3; /* PLAYSTATE_PLAYING - the sink is always ready. */
}

const ManagedMethod androidAudioTrackMethods[] = {
    REGISTER_INIT_METHOD(AndroidAudioTrack, AudioTrack_init, "(IIIIII)V"),
    ManagedMethod::RegisterStatic<&AudioTrack_getMinBufferSize>(
        AndroidAudioTrack::clazz, "getMinBufferSize", "(III)I"),
    ManagedMethod::RegisterStatic<&AudioTrack_getNativeOutputSampleRate>(
        AndroidAudioTrack::clazz, "getNativeOutputSampleRate", "(I)I"),
    /*
     * RegisterNonVirtual, not Register, and the distinction is load-bearing -
     * it is the difference between audio and one confusing log line.
     *
     * This engine calls write through CallNonvirtual*, and portbase's
     * iface_CallNonVirtualMethod always casts addr_variadic to
     * (JNIEnv *, jobject, jclass, va_list) - it does not consult takes_class,
     * because only a dispatcher of that shape can be given the jclass the
     * caller supplied. Registered with Register, the three-argument dispatcher
     * then receives the jclass in the register it reads its va_list from, and
     * the first va_arg dereferences the Class: the array pointer arriving here
     * was clazz->classpath, and the check in array_base() printed it as
     *
     *     element_size=1967206241, elements=0x546f6964
     *
     * which spells "a/Au" and "dioT" - bytes 12..19 of the string
     * "android/media/AudioTrack". Nothing about that reads as a registration
     * mistake.
     *
     * RegisterNonVirtual is right for both call paths and Register is right for
     * only one: iface_CallMethod/V do consult takes_class and pass
     * method->clazz themselves, so an engine calling write the ordinary way
     * still lands on the same four-argument dispatcher with a valid va_list.
     *
     * (iface_CallMethodA - the jvalue-array path - does not honour takes_class
     * and would mis-dispatch this. No engine here reaches it, and every <init>
     * in these ports already has the same exposure; fixing it belongs in
     * portbase, not in one game's audio class.)
     */
    ManagedMethod::RegisterNonVirtual<&AudioTrack_write>(
        AndroidAudioTrack::clazz, "write", "([BII)I"),
    ManagedMethod::RegisterNonVirtual<&AudioTrack_write_shorts>(
        AndroidAudioTrack::clazz, "write", "([SII)I"),
    ManagedMethod::Register<&AudioTrack_play>(
        AndroidAudioTrack::clazz, "play", "()V"),
    ManagedMethod::Register<&AudioTrack_pause>(
        AndroidAudioTrack::clazz, "pause", "()V"),
    ManagedMethod::Register<&AudioTrack_stop>(
        AndroidAudioTrack::clazz, "stop", "()V"),
    ManagedMethod::Register<&AudioTrack_release>(
        AndroidAudioTrack::clazz, "release", "()V"),
    ManagedMethod::Register<&AudioTrack_getPlayState>(
        AndroidAudioTrack::clazz, "getPlayState", "()I"),
    {NULL},
};

Class AndroidAudioTrack::clazz = {
    .classpath       = "android/media/AudioTrack",
    .classname       = "AudioTrack",
    .managed_methods = androidAudioTrackMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidAudioTrack),
};

static const int registered = ClassRegistry::register_class(AndroidAudioTrack::clazz);
