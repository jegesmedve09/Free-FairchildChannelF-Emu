

void gfx_init(void)
{
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsGlobal = gsKit_init_global();
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->DoubleBuffering = GS_SETTING_OFF;

    if (*(volatile u8 *)(PAL_NTSC))
    { gsGlobal->Mode = GS_MODE_NTSC; }else{ gsGlobal->Mode = GS_MODE_PAL; }
    if (*(volatile u8 *)(INTERLACED))
    { gsGlobal->Interlace = GS_INTERLACED; }else{ gsGlobal->Interlace = GS_NONINTERLACED; }
    gsGlobal->Width = *(volatile u32 *)(GS_WIDTH);
    gsGlobal->Height = *(volatile u32 *)(GS_HEIGHT);
    gsKit_set_test(gsGlobal, GS_ATEST_OFF);
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    video_tex.Width = 128;
    video_tex.Height = 64;
    video_tex.PSM = GS_PSM_CT32;
    video_tex.Mem = (u32*)channelf_vram;
    video_tex.Filter = GS_FILTER_NEAREST;
    video_tex.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(128, 64, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);

    asset_tex.Width = 64;
    asset_tex.Height = 32;
    asset_tex.PSM = GS_PSM_CT32; 
    asset_tex.Mem = (u32*)logo_vram;
    asset_tex.Filter = GS_FILTER_NEAREST;
    asset_tex.Vram = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(64, 32, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);


    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(255,255,255,0,0));
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
}
void gfx_render(void)
{
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0, 0, 0, 0, 0));

    gsKit_texture_upload(gsGlobal, &video_tex);

    if (DEBUG_MODE == 0xFF)
    {
        gsKit_prim_sprite_texture(gsGlobal, &video_tex,
        screen_x,
        screen_y,
        4,
        4,
        100*screen_scale + screen_x,
        62*screen_scale + screen_y,
        100,
        62,
        0,
        GS_SETREG_RGBAQ(128, 128, 128, 128, 0));
    }
    else
    {
        gsKit_prim_sprite_texture(gsGlobal, &video_tex,
        screen_x,
        screen_y,
        0,
        0,
        128*screen_scale + screen_x,
        64*screen_scale + screen_y,
        128,
        64,
        0,
        GS_SETREG_RGBAQ(128, 128, 128, 128, 0)); 
    }

    //gsKit_prim_line(gsGlobal, 0, 0, 639, 0, 1, GS_SETREG_RGBAQ(255,255,255,0,0));
    //gsKit_prim_line(gsGlobal, 0, 0, 0, 447, 1, GS_SETREG_RGBAQ(255,255,255,128,0));
    //gsKit_prim_line(gsGlobal, 0, 447, 639, 447, 1, GS_SETREG_RGBAQ(255,255,255,128,0));
    //gsKit_prim_line(gsGlobal, 639, 0, 639, 447, 1, GS_SETREG_RGBAQ(255,255,255,128,0));



    gsKit_sync_flip(gsGlobal);
    gsKit_queue_exec(gsGlobal);
}

void DVD(void)
{
    int x = 0;
    int y = 0;
    u8 mode = 0;
    u32 input = pad_get_buttons(0);
    while (!input)
    {    
        input = pad_get_buttons(0);
        if (input) { return; }
        switch (mode)
        {
            case 0: { x = x + 1; y = y + 2; break; }
            case 1: { x = x - 1; y = y + 2; break; }
            case 2: { x = x + 1; y = y - 2; break; }
            case 3: { x = x - 1; y = y - 2; break; }
            default: { mode = 0; break; }
        }

        if (x <= 0) { x = 0; if (mode == 3) { mode = 2; } else if (mode == 1) { mode = 0; } }
        if (x >= 640 - 64) { x = 640 - 64; if (mode == 0) { mode = 1; } else if (mode == 2) { mode = 3; } }
        if (y <= 0) { y = 0; if (mode == 2) { mode = 0; } else if (mode == 3) { mode = 1; } }
        if (y >= 448 - 32) { y = 448 - 32; if (mode == 0) { mode = 2; } else if (mode == 1) { mode = 3; } }

        gsKit_clear(gsGlobal, 0x00000000);
        gsKit_texture_upload(gsGlobal, &asset_tex);
        gsKit_prim_sprite_texture(gsGlobal, &asset_tex,
        x,
        y,
        0,
        0,
        x+64,
        y+32,
        64,
        32,
        0,
        GS_SETREG_RGBAQ(128, 128, 128, 128, 0));
        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);

    }
}

