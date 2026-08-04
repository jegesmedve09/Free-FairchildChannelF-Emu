// Stick numbers
#define PAD_LEFT_STICK  0
#define PAD_RIGHT_STICK 1

// Players
#define PAD_PLAYER_0    0
#define PAD_PLAYER_1    1

#define MAX_PLAYERS     2

#define PAD_LEFT      0x0080
#define PAD_DOWN      0x0040
#define PAD_RIGHT     0x0020
#define PAD_UP        0x0010
#define PAD_START     0x0008
#define PAD_R3        0x0004
#define PAD_L3        0x0002
#define PAD_SELECT    0x0001
#define PAD_SQUARE    0x8000
#define PAD_CROSS     0x4000
#define PAD_CIRCLE    0x2000
#define PAD_TRIANGLE  0x1000
#define PAD_R1        0x0800
#define PAD_L1        0x0400
#define PAD_R2        0x0200
#define PAD_L2        0x0100

#include <libpad.h>

static char padBuf[2][256] __attribute__((aligned(64)));  // Buffer for both pads
static struct padButtonStatus buttons[MAX_PLAYERS];
static u32 old_pad[MAX_PLAYERS] = {0, 0};
static u32 new_pad[MAX_PLAYERS] = {0, 0};
static u32 paddata[MAX_PLAYERS];

int pad_init(void)
{
    padInit(0);

    // Open both ports
    if (padPortOpen(0, 0, padBuf[0]) == 0) return -1;
    if (padPortOpen(1, 0, padBuf[1]) == 0) return -2;  // Player 2 optional
    return 0;
}

static void update_pad(int player)
{
    if (player < 0 || player >= MAX_PLAYERS) return;

    int state = padGetState(player, 0);
    if (state != PAD_STATE_STABLE) {
        old_pad[player] = 0;
        new_pad[player] = 0;
        return;
    }

    padRead(player, 0, &buttons[player]);

    paddata[player] = 0xFFFF ^ buttons[player].btns;
    new_pad[player] = paddata[player] & ~old_pad[player];
    old_pad[player] = paddata[player];
}

s8 pad_get_joy_x(int stick, int player)
{
    update_pad(player);
    if (player < 0 || player >= MAX_PLAYERS) return 0;

    if (stick == PAD_LEFT_STICK)  return (s8)(buttons[player].ljoy_h - 128);
    if (stick == PAD_RIGHT_STICK) return (s8)(buttons[player].rjoy_h - 128);
    return 0;
}

s8 pad_get_joy_y(int stick, int player)
{
    update_pad(player);
    if (player < 0 || player >= MAX_PLAYERS) return 0;

    if (stick == PAD_LEFT_STICK)  return (s8)(buttons[player].ljoy_v - 128);
    if (stick == PAD_RIGHT_STICK) return (s8)(buttons[player].rjoy_v - 128);
    return 0;
}

u32 pad_get_buttons(int player)
{
    update_pad(player);
    return old_pad[player];
}

u32 pad_get_pressed(int player)
{
    update_pad(player);
    return new_pad[player];
}

