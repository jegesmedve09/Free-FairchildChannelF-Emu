#include <tamtypes.h>
#include <kernel.h>
#include <string.h>
#include <stdio.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <iopcontrol.h>
#include <sifrpc.h>
//#include <gsToolkit.h>
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
    u16 PC0; //primary PC
    u16 PC1; //stack PC)
    u16 DC0; //data counter something
    u8 A;   //accoumluator or how the fuck you write it correctly
    u8 W;   // Status Flags register
    u8 ISAR;// Indirect Scratchpad Address Register, basically the IRS but with an A added and scrambled
    u8 scratchpad[64]; // meomory

}CPU;

CPU cpu;
u8 rom[8192];
u32 channelf_vram[128 * 64];
GSTEXTURE video_tex;

GSGLOBAL *gsGlobal;
GSTEXTURE fontfile;

int screen_x=20;
int screen_y=20;
int screen_scale=4;

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




#include "pad.c"
#include "instset.c"
#include "insttable.c"
#include "gfx.c"



void halt()
{
    printf("PC: 0x%04X, Opcode: 0x%02X\n", cpu.PC0, rom[cpu.PC0]);
    printf("\n=== ROM HEX DUMP ===\n");
    for (int i = 0; i < sizeof(rom); i++)
    {
        if (i % 16 == 0) {
            printf("\n0x%04X: ", i);
        }
        printf("%02X ", rom[i]);
    }
    printf("\n\n=== END OF DUMP ===\n");


    printf("\n\n=== CPU SCRATCHPAD DUMP (64 bytes) ===\n");
    for (int i = 0; i < 64; i++)
    {
        if (i % 16 == 0) printf("\n0x%02X: ", i);
        printf("%02X ", cpu.scratchpad[i]);
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

int main(void)
{
    SifInitRpc(0);
    while (!SifIopSync()) {};
    FuckAroundSilentlyMs(2000);

    gfx_init();
    pad_init();

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
        // Light Grey color index mapping: RGBA format (Alpha layer must be 128/0x80!)
        channelf_vram[i] = GS_SETREG_RGBAQ(i, i%2, i/2, 128, 0); 
    }

    memset(rom, 0, sizeof(rom));
    
    strcpy((char*)ARGV_ADDRESS, "test.chf");

    char *argvpath = (char *)ARGV_ADDRESS;
    char biospath[2048];
    char fullpath[2048];

    memset(biospath, 0, sizeof(biospath));
    strncpy(biospath, path_portableinator("bios/bios.bin"), sizeof(biospath) - 1);
    
    memset(fullpath, 0, sizeof(fullpath));
    strncpy(fullpath, path_portableinator("roms/"), sizeof(fullpath) - 1);
    strncat(fullpath, argvpath, sizeof(fullpath) - strlen(fullpath) - 1);

    FILE *bios = fopen(biospath, "rb");
    FILE *file = fopen(fullpath, "rb");

    if (bios == NULL)
    {
        *GS_BGCOLOR = 0xFF00;
        while(1) {}
    }
    if (file == NULL)
    {
        *GS_BGCOLOR = 0xFF00;
        while(1) {}
    }
    
    size_t BIOS_bytes_read = fread(&rom[0x0000], 1, 2048, bios);
    size_t bytes_read = fread(&rom[0x0800], 1, sizeof(rom)-2048, file);
    fclose(bios);
    fclose(file);

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

    #define DEBUG_LOG_ADDRESS 0x01EFF200
    #define DEBUG_COUNTER_ADDRESS 0x01EFF204
    //u32 instruction_count = 0;
    //while (1) {gfx_render();}    
    while (1)
    { 
        for (int i = 0; i < 5000; i++)
        {
            u8 opcode = rom[cpu.PC0];
            //printf("PC: 0x%04X, Opcode: 0x%02X\n", cpu.PC0, rom[cpu.PC0]);
            //*(volatile u32*)(DEBUG_LOG_ADDRESS) = (cpu.PC0 << 16) | opcode;
            //*(volatile u32*)(DEBUG_COUNTER_ADDRESS) = ++instruction_count;
            //cpu.PC0++;
            insttable[opcode]();
            //if (cpu.PC0 == 0x0008) { cpu.W = 4; }
            
            //FuckAroundSilentlyMs(100);
        }
        gfx_render();
        if (pad_get_pressed(0) && PAD_START )
        {
            while (1)
            {
                


            }
        }
        //FuckAroundSilentlyMs(100);
        
    }
    return 0;
}

