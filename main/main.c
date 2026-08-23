#include <tamtypes.h>
#include <kernel.h>
#include <string.h>
#include <stdio.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <iopcontrol.h>
#include <sifrpc.h>
#include <math.h>
#include <dmaKit.h>
#include <stdlib.h>

#define WORKPATH_ADDRESS    0x01EFFC00
#define PORTABLE_ADDRESS    0x01EFF800
#define ARGV_ADDRESS    0x01EFF400

#define SHARED_BASE_ADDR    0x01EFF7F0

#define INTERLACED          (SHARED_BASE_ADDR + 0x00)
#define PAL_NTSC            (SHARED_BASE_ADDR + 0x01)
#define PRIM_ALPHA          (SHARED_BASE_ADDR + 0x02)
#define HAS_TO_BE_TRUE      (SHARED_BASE_ADDR + 0x03)
#define GS_WIDTH            (SHARED_BASE_ADDR + 0x04)
#define GS_HEIGHT           (SHARED_BASE_ADDR + 0x08)
#define SIGNATURE_SPACE     (SHARED_BASE_ADDR + 0x0C)
#define SIGNATURE           0x4741597E


typedef struct
{
    u16 PC0;
    u16 PC1;
    u16 DC0;
    u16 DC1; //unimplemented only in XDC
    u8 A;
    u8 W;
    u8 ISAR;
    u8 scratchpad[64]; // meomory

}CPU;

CPU cpu;
u8 rom[65536];
u32 channelf_vram[128 * 64];
u32 backup_vram[128 * 64];
u32 logo_vram[64 * 32];
GSTEXTURE video_tex;
GSTEXTURE asset_tex;
GSGLOBAL *gsGlobal;
u16 ninput_delay = 0;
void FuckAroundSilentlyMs(int miliseconds)
{
    unsigned int start, now;

    __asm__ volatile("mfc0 %0, $9" : "=r"(start));

    while (1)
    {
        __asm__ volatile("mfc0 %0, $9" : "=r"(now));
        if ((now - start) >= (unsigned int)(miliseconds * 147456))
            break;
    }
}

char *path_portableinator(const char *path)
{
    static char result[2048];

    strncpy(result, (char*)PORTABLE_ADDRESS, sizeof(result) - 1);
    result[sizeof(result) - 1] = '\0';
    strncat(result, path, sizeof(result) - strlen(result) - 1);
    result[sizeof(result) - 1] = '\0';
    return result;
}

int screen_x=10;
int screen_y=20;
float screen_scale=6.2;
int ins_speed=5000;
u8 DEBUG_MODE = 0xFF;

#include "sound.c"
#include "pad.c"
#include "gfx.c"
#include "f8_set.c"
#include "f8_table.c"


void halt()
{
    printf("PC: 0x%04X, Opcode: 0x%02X\n", cpu.PC0, rom[cpu.PC0]);
    printf("\n=== ROM HEX DUMP ===\n");
    for (int i = 0; i < sizeof(rom); i++)
    {
        if (i % 16 == 0)
        {
            printf("\n0x%04X: ", i);
        }
        printf("%02X ", rom[i]);
    }
    printf("\n\n=== END OF DUMP ===\n");


    printf("\n\n=== CPU SCRATCHPAD DUMP (64 bytes) ===\n");
    for (int i = 0; i < 64; i++)
    {
        if (i % 16 == 0) printf("\n0x%02X: ", i);
        {
            printf("%02X ", cpu.scratchpad[i]);
        }
    }
    printf("\n\n=== END OF DUMPS ===\n");                    

    while (1) {}

}

void reset_cpu()
{
    cpu.PC0 = 0x0000;
    cpu.PC1 = 0x0000;
    cpu.A = 0x00;
    cpu.W = 0x00;
    cpu.ISAR = 0x00;
    
    memset(cpu.scratchpad, 0, 64);
}

int map_value(float x, float in_min, float in_max, int out_min, int out_max)
{
    float result = (x - in_min) * (float)(out_max - out_min) / (in_max - in_min) + (float)out_min;
    
    // Casts to int (truncates). Add 0.5f before casting if you want proper rounding:
    return (int)(result + 0.5f); 
}

void draw_setsclen(void)
{
    for(int i = 0; i < (128 * 64); i++)
    {
        channelf_vram[i] = GS_SETREG_RGBAQ(255, 200, 200, 128, 0); 
    }
    //left arrow
    channelf_vram[(30 * 128) + 5] = 0x00000000;
    channelf_vram[(29 * 128) + 6] = 0x00000000;
    channelf_vram[(31 * 128) + 6] = 0x00000000;
    channelf_vram[(28 * 128) + 7] = 0x00000000;
    channelf_vram[(32 * 128) + 7] = 0x00000000;
    //right arrow
    channelf_vram[(30 * 128) + 98] = 0x00000000;
    channelf_vram[(29 * 128) + 97] = 0x00000000;
    channelf_vram[(31 * 128) + 97] = 0x00000000;
    channelf_vram[(28 * 128) + 96] = 0x00000000;
    channelf_vram[(32 * 128) + 96] = 0x00000000;
    //top arrow
    channelf_vram[(5 * 128) + 51] = 0x00000000;
    channelf_vram[(6 * 128) + 50] = 0x00000000;
    channelf_vram[(6 * 128) + 52] = 0x00000000;
    channelf_vram[(7 * 128) + 49] = 0x00000000;
    channelf_vram[(7 * 128) + 53] = 0x00000000;
    //bottom arrow
    channelf_vram[(60 * 128) + 51] = 0x00000000;
    channelf_vram[(59 * 128) + 50] = 0x00000000;
    channelf_vram[(59 * 128) + 52] = 0x00000000;  
    channelf_vram[(58 * 128) + 49] = 0x00000000;
    channelf_vram[(58 * 128) + 53] = 0x00000000;
    // R
    channelf_vram[(36*128)+22]=0;
    channelf_vram[(36*128)+23]=0;
    channelf_vram[(37*128)+22]=0;
    channelf_vram[(37*128)+24]=0;
    channelf_vram[(38*128)+22]=0;
    channelf_vram[(38*128)+23]=0;
    channelf_vram[(39*128)+22]=0;
    channelf_vram[(39*128)+24]=0;
    channelf_vram[(40*128)+22]=0;
    channelf_vram[(40*128)+24]=0;
    // 1 
    channelf_vram[(36*128)+27]=0;
    channelf_vram[(37*128)+26]=0;
    channelf_vram[(37*128)+27]=0;
    channelf_vram[(38*128)+27]=0;
    channelf_vram[(39*128)+27]=0;
    channelf_vram[(40*128)+26]=0;
    channelf_vram[(40*128)+27]=0;
    channelf_vram[(40*128)+28]=0;

    // L
    channelf_vram[(36*128)+74]=0;
    channelf_vram[(37*128)+74]=0;
    channelf_vram[(38*128)+74]=0;
    channelf_vram[(39*128)+74]=0;
    channelf_vram[(40*128)+74]=0;
    channelf_vram[(40*128)+75]=0;
    channelf_vram[(40*128)+76]=0;
    // 1
    channelf_vram[(36*128)+79]=0; // 79
    channelf_vram[(37*128)+78]=0; //95 -78
    channelf_vram[(37*128)+79]=0; //79
    channelf_vram[(38*128)+79]=0; //79
    channelf_vram[(39*128)+79]=0; //79
    channelf_vram[(40*128)+78]=0; // 78
    channelf_vram[(40*128)+79]=0; // 79
    channelf_vram[(40*128)+80]=0; //80

    for (int x = 31; x <= 71; x++)
    {
        channelf_vram[(38*128)+x] = 0x00000000;
    }
    for (int x = 31; x <= 72; x += 10)
    {
        channelf_vram[(37*128)+x] = 0x00000000;
        channelf_vram[(39*128)+x] = 0x00000000;
    }
}

int sh_menu(void)
{
    draw_setsclen();   
    FuckAroundSilentlyMs(500);
    while (1)
    {
        u32 pad = pad_get_buttons(0);
        if (pad & PAD_CROSS) { if (!DEBUG_MODE) { DEBUG_MODE = 0xFF; } else { DEBUG_MODE = 0x00; } FuckAroundSilentlyMs(500); }
        if (pad & PAD_START) { FuckAroundSilentlyMs(500); return 0; }
        
        if (DEBUG_MODE == 0xFF)
        {
            u32 pad = pad_get_buttons(0);
            if (pad & PAD_LEFT) { if (screen_x >= 0 ) { screen_x--; } }
            if (pad & PAD_RIGHT) { if (screen_x <= 639-100*screen_scale ) { screen_x++; } }
            if (pad & PAD_UP) { if (screen_y >= 0 ) { screen_y--; } }
            if (pad & PAD_DOWN) { if (screen_y <= 447-62*screen_scale ) { screen_y++; } }
            if (pad & PAD_L1) {  screen_scale = screen_scale - 0.1; if (screen_scale <= 1.0f) { screen_scale = 1.0f; } }
            if (pad & PAD_R1) {  screen_scale = screen_scale + 0.1; if (screen_scale >= 6.2f) { screen_scale = 6.2f; } }
        }
        else
        {
            if (pad & PAD_LEFT) { screen_x--; }
            if (pad & PAD_RIGHT) { screen_x++; }
            if (pad & PAD_UP) { screen_y--; }
            if (pad & PAD_DOWN) {screen_y++; }
            if (pad & PAD_L1) { screen_scale = screen_scale - 0.1 ; }
            if (pad & PAD_R1) { screen_scale = screen_scale + 0.1 ; }
        }

        for (int x = 31; x <= 71; x++)
        {
            channelf_vram[(37*128)+x] = GS_SETREG_RGBAQ(255, 200, 200, 128, 0);
            channelf_vram[(38*128)+x] = 0x00000000;
            channelf_vram[(39*128)+x] = GS_SETREG_RGBAQ(255, 200, 200, 128, 0);
        }

        for (int x = 31; x <= 72; x += 10)
        {
            channelf_vram[(37*128)+x] = 0x00000000;
            channelf_vram[(39*128)+x] = 0x00000000;
        }

        int percent = map_value(screen_scale, 1.0f, 6.2f, 31, 71);
        channelf_vram[(37*128)+percent] = 0xFFFF0080;
        channelf_vram[(38*128)+percent] = 0xFFFF0080;
        channelf_vram[(39*128)+percent] = 0xFFFF0080;
        gfx_render();
    }
    return -1;
}


int main(void)
{
    SifInitRpc(0);
    while (!SifIopSync()) {};
    FuckAroundSilentlyMs(2000);

    gfx_init();
    pad_init();
    sound_init();
    
    printf("gsGlobal->Width=%d Height=%d Mode=%d Interlace=%d\n",
    gsGlobal->Width, gsGlobal->Height, gsGlobal->Mode, gsGlobal->Interlace);
    printf("ScreenBuffer[0]=0x%08X video_tex.Vram=0x%08X\n",
    gsGlobal->ScreenBuffer[0], video_tex.Vram);
    
    if (*(volatile u32*)SIGNATURE_SPACE != SIGNATURE)
    {
        *GS_BGCOLOR = 0xFF0000;
        while (1) {}
    }

    for(int i = 0; i < (128 * 64); i++)
    {
        channelf_vram[i] = GS_SETREG_RGBAQ(i, i%2, i/2, 128, 0); 
    }

    memset(rom, 0, sizeof(rom));
    
    strcpy((char*)ARGV_ADDRESS, "test.chf");

    char *argvpath = (char *)ARGV_ADDRESS;
    char biospath[2048];
    char rompath[2048];
    char assetpath[2048];

    memset(biospath, 0, sizeof(biospath));
    strncpy(biospath, path_portableinator("bios/bios.bin"), sizeof(biospath) - 1);
    
    memset(rompath, 0, sizeof(rompath));
    strncpy(rompath, path_portableinator("roms/"), sizeof(rompath) - 1);
    strncat(rompath, argvpath, sizeof(rompath) - strlen(rompath) - 1);
    
    memset(assetpath, 0, sizeof(assetpath));
    strncpy(assetpath, path_portableinator("afk.bin"), sizeof(assetpath) - 1);

    FILE *bios = fopen(biospath, "rb");
    FILE *file = fopen(rompath, "rb");
    FILE *asset = fopen(assetpath, "rb");

    if (bios == NULL)
    {
        *GS_BGCOLOR = 0xFF0000;
        while(1) {}
    }
    if (file == NULL)
    {
        *GS_BGCOLOR = 0x00FF00;
        while(1) {}
    }
    if (asset == NULL)
    {
        *GS_BGCOLOR = 0x0000FF;
        while(1) {}
    }
    
    size_t BIOS_bytes_read = fread(&rom[0x0000], 1, 2048, bios);
    size_t bytes_read = fread(&rom[0x0800], 1, sizeof(rom)-2048, file);
    fread(&logo_vram, 1, sizeof(logo_vram), asset);
    fclose(bios);
    fclose(file);
    fclose(asset);

    printf("BIOS bytes: %d | ROM bytes: %d\n", (int)BIOS_bytes_read, (int)bytes_read);
    printf("Starting emulation at PC: 0x%04X, Opcode: 0x%02X\n", cpu.PC0, rom[cpu.PC0]);
    if (BIOS_bytes_read == 0)
    {
        *GS_BGCOLOR = 0xFF00FF;
        while(1) {}
    }
    if (bytes_read == 0)
    {
        *GS_BGCOLOR = 0x00FFFF;
        while(1) {}
    }
    reset_cpu();

    while (1)
    {
        u32 pad = pad_get_buttons(0);
        if (pad & PAD_START)
        {   
            memcpy(backup_vram, channelf_vram, sizeof(channelf_vram));
            
            sh_menu();

            memcpy(channelf_vram, backup_vram, sizeof(backup_vram));
            
        }
        if (ninput_delay >= 3000)//65530)
        {
            ninput_delay = 0;
            DVD();            
        }
        else if (pad)
        {
            ninput_delay = 0;
        }
        else
        {
            ninput_delay++;
        }
        for (int i = 0; i < ins_speed; i++)
        {
            u8 opcode = rom[cpu.PC0];
            insttable[opcode]();
        }
        gfx_render();
        sound_tick();
        
    }
    return 0;
}

