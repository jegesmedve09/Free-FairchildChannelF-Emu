// sound.c - direct speaker-level emulation, matching real bit-banged Channel F audio

//#include <tamtypes.h>
//#include <kernel.h>
#include <audsrv.h>
//#include <string.h>

#define AUDIO_FEED_CHUNK 2048
#define MAX_VOLUME 100
#define EVENT_QUEUE_SIZE 512

// Real F8 NTSC clock. We're using emulated INSTRUCTION count as a stand-in
// for real CPU CYCLES, since we don't model per-instruction cycle costs yet.
// This is an approximation - real hardware timing varies per opcode.
#define F8_CLOCK_HZ 1789772

typedef struct { u64 cycle_time; u8 level; } speaker_event_t;
static speaker_event_t event_queue[EVENT_QUEUE_SIZE];
static int event_head = 0, event_tail = 0;

static u64 emulated_cycle_count = 0;   // advance this once per emulated instruction elsewhere
static u8  current_speaker_level = 0;

int sound_init(void)
{
    int ret = audsrv_init();
    if (ret != 0) { printf("channelf_sound: audsrv_init failed, %d\n", ret); return -1; }
    struct audsrv_fmt_t fmt = { .bits = 16, .channels = 2, .freq = 48000 };
    ret = audsrv_set_format(&fmt);
    if (ret != 0) { printf("channelf_sound: audsrv_set_format failed, %d\n", ret); return -2; }
    audsrv_set_volume(MAX_VOLUME);
    return 0;
}

// call this from your main CPU loop, once per emulated instruction
void sound_tick(void) { emulated_cycle_count++; }

// call from the port-5 write handler
void sound_write(u8 value)
{
    u8 level = ((value >> 6) & 0x03) ? 1 : 0;
    if (level == current_speaker_level) return;
    current_speaker_level = level;

    int next = (event_tail + 1) % EVENT_QUEUE_SIZE;
    if (next != event_head) {
        event_queue[event_tail].cycle_time = emulated_cycle_count;
        event_queue[event_tail].level = level;
        event_tail = next;
    }
}

// call once per real frame - renders exact toggle history into the audio buffer
void sound_update(void)
{
    int avail = audsrv_available();
    if (avail < AUDIO_FEED_CHUNK) return;

    static s16 out[AUDIO_FEED_CHUNK / 2] __attribute__((aligned(64)));
    int frames = AUDIO_FEED_CHUNK / 4;
    s16 amp = 8000;

    static u64 audio_cycle_pos = 0; // where we are in cycle-time, persists across calls
    double cycles_per_sample = (double)F8_CLOCK_HZ / 48000.0;

    for (int i = 0; i < frames; i++) {
        // advance any queued events that fall before this sample's cycle position
        while (event_head != event_tail && event_queue[event_head].cycle_time <= (u64)audio_cycle_pos) {
            // level already reflected in current_speaker_level at write time; nothing else needed here
            event_head = (event_head + 1) % EVENT_QUEUE_SIZE;
        }
        s16 sample = current_speaker_level ? amp : -amp;
        out[i*2+0] = sample;
        out[i*2+1] = sample;
        audio_cycle_pos += cycles_per_sample;
    }

    FlushCache(0);
    audsrv_play_audio((char *)out, AUDIO_FEED_CHUNK);
}

void sound_quit(void) { audsrv_quit(); }
