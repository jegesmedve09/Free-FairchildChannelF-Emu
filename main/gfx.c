

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

    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(255,255,255,0,0));
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
}



//void gfx_render(void)
//{
//    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0, 0, 0, 0, 0));
//    gsKit_texture_upload(gsGlobal, &video_tex);

    //gsKit_prim_sprite_texture(gsGlobal, &video_tex, 0, 0, 0.0f, 0.0f, gsGlobal->Width, gsGlobal->Height, 128.0f, 64.0f, 0, GS_SETREG_RGBAQ(128, 128, 128, 128, 0));    
//    gsKit_prim_sprite_texture(gsGlobal, &video_tex, 0, 0, 0.0f, 0.0f, gsGlobal->Width, gsGlobal->Height, 128.0f, 64.0f, 0, GS_SETREG_RGBAQ(128, 128, 128, 128, 0));    
//    gsKit_queue_exec(gsGlobal);
//    gsKit_sync_flip(gsGlobal);
//}


void gfx_render(void)
{
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0, 0, 0, 0, 0));
    
    // Ensure the texture is uploaded/bound properly each frame
    gsKit_texture_upload(gsGlobal, &video_tex);

    // Correct signature parameters: Destination (X1, Y1, X2, Y2), Source UV (U1, V1, U2, V2)
    gsKit_prim_sprite_texture(gsGlobal, &video_tex, 
                              screen_x, screen_y,                         // Screen X1, Y1
                              0, 0,                         // Texture U1, V1
                              (float)video_tex.Width*screen_scale,//(float)gsGlobal->Width,             // Screen X2 (Scaled width)
                              (float)video_tex.Height*screen_scale,//(float)gsGlobal->Height,            // Screen Y2 (Scaled height)
                              (float)video_tex.Width,             // Texture U2 (128.0f)
                              (float)video_tex.Height,            // Texture V2 (64.0f)
                              0,                                  // Z-depth
                              GS_SETREG_RGBAQ(128, 128, 128, 128, 0)); // Modulation color

    gsKit_prim_line(gsGlobal, 0, 0, 639, 0, 1, GS_SETREG_RGBAQ(255,255,255,0,0));
    gsKit_prim_line(gsGlobal, 0, 0, 0, 447, 1, GS_SETREG_RGBAQ(255,255,255,128,0));
    gsKit_prim_line(gsGlobal, 0, 447, 639, 447, 1, GS_SETREG_RGBAQ(255,255,255,128,0));
    gsKit_prim_line(gsGlobal, 639, 0, 639, 447, 1, GS_SETREG_RGBAQ(255,255,255,128,0));



    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);
}


int load_png(char *path, GSTEXTURE *texture)
{
    int ret = gsKit_texture_png(gsGlobal, texture, path_portableinator(path));
    if (ret != 0)
    {
        printf("PNG Load Fail: %s (err %d)\n", path, ret);
        return -1;
    }
    texture->Filter = GS_FILTER_NEAREST;
    return 0;
}

void gfx_draw_image(int x, int y, GSTEXTURE *texture, bool xflip, bool yflip)
{
    int u1 = 0, u2 = texture->Width;
    int v1 = 0, v2 = texture->Height;

    if (xflip)
    {
        int tmp = u1;
        u1 = u2;
        u2 = tmp;
    }
    if (yflip)
    {
        int tmp = v1;
        v1 = v2;
        v2 = tmp;
    }

    gsKit_prim_sprite_texture(gsGlobal, texture, x, y, u1, v1, x + texture->Width, y + texture->Height, u2, v2, 1, 0x80808080);
}
void gfx_draw_text(char *text, int x, int y)
{
    char *text_copy = strdup(text); 
    if (!text_copy) { return; }

    int cursor_x = x;
    char *token = strtok(text_copy, ",");

    while (token != NULL)
    {
        int index = atoi(token);
        
        if (index > 0)
        {
            int cell_idx = index - 1; 
            int col = cell_idx % 32;
            int row = cell_idx / 32;
            
            int u1 = col * 16;
            int v1 = row * 32;
            
            gsKit_prim_sprite_texture(gsGlobal, &fontfile, cursor_x, y, u1, v1, cursor_x + 16, y + 32, u1 + 16, v1 + 32, 1, GS_SETREG_RGBAQ(255, 255, 255, 128, 0));

            cursor_x += 16;
        }
        if (index >= 27 && index <= 32)
        {
            cursor_x += 8;
        }

        token = strtok(NULL, ",");
    }
    
    free(text_copy);
}


