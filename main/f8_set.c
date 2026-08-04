#define FLAG_S 0x01
#define FLAG_C 0x02
#define FLAG_Z 0x04
#define FLAG_O 0x08

void clr_ozcs(void) { cpu.W &= ~(FLAG_O|FLAG_Z|FLAG_C|FLAG_S); }
void set_sz(u8 n) {
    if (n == 0) cpu.W |= FLAG_Z;
    if (~n & 0x80) cpu.W |= FLAG_S;
}
// returns result, sets O/C as side effect (matches MAME's do_add)
u8 do_add(u8 n, u8 m, u8 c) {
    u16 r = n + m + c;
    if (r & 0x100) cpu.W |= FLAG_C;
    if ((n ^ r) & (m ^ r) & 0x80) cpu.W |= FLAG_O;
    return (u8)r;
}

u8 do_add_decimal(u8 augend, u8 addend) {
    u8 tmp = augend + addend;
    u8 c = 0, ic = 0;
    if (((augend + addend) & 0xff0) > 0xf0) c = 1;
    if ((augend & 0x0f) + (addend & 0x0f) > 0x0F) ic = 1;
    clr_ozcs();
    do_add(augend, addend, 0);
    set_sz(tmp);
    if (c == 0 && ic == 0) tmp = ((tmp + 0xa0) & 0xf0) + ((tmp + 0x0a) & 0x0f);
    if (c == 0 && ic == 1) tmp = ((tmp + 0xa0) & 0xf0) + (tmp & 0x0f);
    if (c == 1 && ic == 0) tmp = (tmp & 0xf0) + ((tmp + 0x0a) & 0x0f);
    return tmp;
}



u8 isar_addr_direct(void) { return cpu.ISAR & 0x3F; }
u8 isar_addr_inc(void) { u8 a = cpu.ISAR & 0x3F; cpu.ISAR = (cpu.ISAR & 0x38) | ((cpu.ISAR + 1) & 0x07); return a; }
u8 isar_addr_dec(void) { u8 a = cpu.ISAR & 0x3F; cpu.ISAR = (cpu.ISAR & 0x38) | ((cpu.ISAR - 1) & 0x07); return a; }
u8 f8_read_byte(u16 address) { return rom[address % sizeof(rom)]; }

void set_flags_logic(u8 result) {
    cpu.W &= ~(0x08 | 0x04 | 0x02 | 0x01);
    if (result == 0) cpu.W |= 0x04;                 // Zero
    if ((result & 0x80) == 0) cpu.W |= 0x01;         // Sign (inverted)
}

// For add/subtract-type ops: all four flags vary. Pass the two 8-bit operands as added (DS uses b=0xFF)
void set_flags_arith(u8 a, u8 b) {
    u16 result = (u16)a + (u16)b;
    u8 carry7 = (result > 0xFF) ? 1 : 0;
    u8 carry6 = (((a & 0x7F) + (b & 0x7F)) > 0x7F) ? 1 : 0;
    u8 res8 = (u8)result;
    cpu.W &= ~(0x08 | 0x04 | 0x02 | 0x01);
    if (carry6 ^ carry7) cpu.W |= 0x08;              // Overflow
    if (res8 == 0) cpu.W |= 0x04;                    // Zero
    if (carry7) cpu.W |= 0x02;                       // Carry
    if ((res8 & 0x80) == 0) cpu.W |= 0x01;            // Sign (inverted)
}


void helper_bt(u8 mask) { u8 flags = cpu.W & 0x0F; if (flags & mask) { s8 off = (s8)f8_read_byte(cpu.PC0+1); cpu.PC0 += 1 + off; } else { cpu.PC0 += 2; } }
void helper_bf(u8 mask) { u8 flags = cpu.W & 0x0F; if (!(flags & mask)) { s8 off = (s8)f8_read_byte(cpu.PC0+1); cpu.PC0 += 1 + off; } else { cpu.PC0 += 2; } }
void helper_ds_flags(u8 old_val) { set_flags_arith(old_val, 0xFF); }




u8 bcd_add(u8 a, u8 b) {
    u16 sum = a + b;
    if ((sum & 0x0F) > 0x09) sum += 0x06;
    if ((sum & 0xF0) > 0x90) sum += 0x60;
    return (u8)sum;
}

u8 channelf_latch_x = 0;
u8 channelf_latch_y = 0;

u8 handle_video_port_read(u8 port)
{
    u8 val = 0xFF;
    switch (port & 0x07)
    {

        case 0:
        {
            u32 p1_btns = pad_get_buttons(0);
            if (p1_btns & PAD_UP) { val &= ~0x01; }
            if (p1_btns & PAD_DOWN) { val &= ~0x02; }
            if (p1_btns & PAD_LEFT) { val &= ~0x04; }
            if (p1_btns & PAD_RIGHT) { val &= ~0x08; }
            if (p1_btns & PAD_CROSS) { val &= ~0x10; }
            if (p1_btns & PAD_L1) { val &= ~0x40; }
            if (p1_btns & PAD_R2) { val &= ~0x80; }
            return val;
        }
        case 1:
        {
            u32 p1_sysbutton = pad_get_buttons(0);
            if (p1_sysbutton & PAD_SELECT) 
            {
                if (p1_sysbutton & PAD_CROSS)  { val &= ~0x01; } // Console Button 1
                if (p1_sysbutton & PAD_SQUARE) { val &= ~0x02; } // Console Button 2
                if (p1_sysbutton & PAD_CIRCLE) { val &= ~0x04; } // Console Button 3
                if (p1_sysbutton & PAD_TRIANGLE){ val &= ~0x08; } // Console Button 4 (Hold)
                return val;
            }
            else
            {
                u32 p2_btns = pad_get_buttons(1);
                if (p2_btns & PAD_UP) { val &= ~0x01; }
                if (p2_btns & PAD_DOWN) { val &= ~0x02; }
                if (p2_btns & PAD_LEFT) { val &= ~0x04; }
                if (p2_btns & PAD_RIGHT) { val &= ~0x08; }
                if (p2_btns & PAD_CROSS) { val &= ~0x10; }
                if (p2_btns & PAD_L1) { val &= ~0x40; }
                if (p2_btns & PAD_R2) { val &= ~0x80; }
                return val;
            }
        }
        case 2: return 0x01; // vBlanc
        case 3: return 0x00; //idk but important
        case 4: return channelf_latch_x ^ 0x7F;
        case 5: return channelf_latch_y ^ 0x3F;
        default: return 0;
    }
}
u32 palette_a[4] =
{
    GS_SETREG_RGBAQ(0xE0, 0xE0, 0xE0, 128, 0),
    GS_SETREG_RGBAQ(0x10, 0x10, 0x10, 128, 0),
    GS_SETREG_RGBAQ(0x91, 0xFF, 0xA6, 128, 0),
    GS_SETREG_RGBAQ(0xCE, 0xD0, 0xFF, 128, 0),
};
// Palette B (row 2): white, red, green, blue
u32 palette_b[4] =
{
    GS_SETREG_RGBAQ(0x4B, 0x3F, 0xF3, 128, 0),
    GS_SETREG_RGBAQ(0xFF, 0x31, 0x53, 128, 0), // index 1 -> red/pink (aliens) — confirmed
    GS_SETREG_RGBAQ(0x02, 0xCC, 0x5D, 128, 0), // index 2 -> bright green (checker) — confirmed, same as your chart's green
    GS_SETREG_RGBAQ(0xFC, 0xFC, 0xFC, 128, 0), // index 3 -> should be grey/white background — confirmed slot, using your chart's white/light value
};
u8 current_palette = 1;
void handle_video_port_writes(u8 port, u8 value)
{
    if (port == 4)
    {
        channelf_latch_x = (value ^ 0x7F) & 0x7F;
    }
    else if (port == 5)
    {
        channelf_latch_y = value ^ 0x3F;
        printf("Y-LATCH write value=0x%02X -> latch_y=%d\n", value, channelf_latch_y);
    }
    else if (port == 1)
    {
        u8 color_index = (value >> 6) & 0x03;
        u8 x = channelf_latch_x;
        u8 y = channelf_latch_y;
        if (x >= 120 && x < 128) {
            if (x == 125) {
                current_palette = (color_index != 0) ? 1 : 0;   // or whatever bit/condition we determine from real data
            }
        } else if (x < 128 && y < 64) {
            u32 ps2_color = (current_palette == 0) ? palette_a[color_index] : palette_b[color_index];
            channelf_vram[(y * 128) + x] = ps2_color;
        }
    }
}




//00-0F
void LR_A_KU(void) { cpu.A = cpu.scratchpad[12]; cpu.PC0 += 1; }
void LR_A_KL(void) { cpu.A = cpu.scratchpad[13]; cpu.PC0 += 1; }
void LR_A_QU(void) { cpu.A = cpu.scratchpad[14]; cpu.PC0 += 1; }
void LR_A_QL(void) { cpu.A = cpu.scratchpad[15]; cpu.PC0 += 1; }
void LR_KU_A(void) { cpu.scratchpad[12] = cpu.A; cpu.PC0 += 1; }
void LR_KL_A(void) { cpu.scratchpad[13] = cpu.A; cpu.PC0 += 1; }
void LR_QU_A(void) { cpu.scratchpad[14] = cpu.A; cpu.PC0 += 1; }
void LR_QL_A(void) { cpu.scratchpad[15] = cpu.A; cpu.PC0 += 1; }
void LR_K_P(void) { cpu.scratchpad[12] = (cpu.PC1 >> 8) & 0xFF; cpu.scratchpad[13] = cpu.PC1 & 0xFF; cpu.PC0 += 1; }
void LR_P_K(void) { cpu.PC1 = (cpu.scratchpad[12] << 8) | cpu.scratchpad[13]; cpu.PC0 += 1; }
void LR_A_IS(void) { cpu.A = cpu.ISAR; cpu.PC0 += 1; }
void LR_IS_A(void) { cpu.ISAR = cpu.A & 0x3F; cpu.PC0 += 1; }
void PK(void) { cpu.PC1 = cpu.PC0 + 1; cpu.PC0 = (cpu.scratchpad[12] << 8) | cpu.scratchpad[13]; }
void LR_P0_Q(void) { cpu.PC0 = (cpu.scratchpad[14] << 8) | cpu.scratchpad[15]; }
void LR_Q_DC(void) { cpu.scratchpad[14] = (cpu.DC0 >> 8) & 0xFF; cpu.scratchpad[15] = cpu.DC0 & 0xFF; cpu.PC0 += 1; }
void LR_DC_Q(void) { cpu.DC0 = (cpu.scratchpad[14] << 8) | cpu.scratchpad[15]; cpu.PC0 += 1; }

//10-1F
void LR_DC_H(void) { cpu.DC0 = (cpu.scratchpad[10] << 8) | cpu.scratchpad[11]; cpu.PC0 += 1; }
void LR_H_DC(void) { cpu.scratchpad[10] = (cpu.DC0 >> 8) & 0xFF; cpu.scratchpad[11] = cpu.DC0 & 0xFF; cpu.PC0 += 1; }
void SR_1(void) { cpu.A >>= 1; clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void SL_1(void) { cpu.A <<= 1; clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void SR_4(void) { cpu.A >>= 4; clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void SL_4(void) { cpu.A <<= 4; clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void LM(void) { cpu.A = f8_read_byte(cpu.DC0); cpu.DC0++; cpu.PC0 += 1; }
void ST(void) { rom[cpu.DC0 % sizeof(rom)] = cpu.A; cpu.DC0++; cpu.PC0 += 1; }
void COM(void)  { cpu.A = ~cpu.A; clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void LNK(void)  { u8 c = (cpu.W & FLAG_C) ? 1 : 0; clr_ozcs(); cpu.A = do_add(cpu.A, 0, c); set_sz(cpu.A); cpu.PC0 += 1; }
void DI_(void) { cpu.W &= ~(1 << 4); cpu.PC0 += 1; }
void EI_(void) { cpu.W |= (1 << 4);  cpu.PC0 += 1; }
void POP(void) { cpu.PC0 = cpu.PC1; }
void LR_W_J(void) { cpu.W = cpu.scratchpad[9]; cpu.PC0 += 1; }
void LR_J_W(void) { cpu.scratchpad[9] = cpu.W; cpu.PC0 += 1; }
void INC(void)  { clr_ozcs(); cpu.A = do_add(cpu.A, 1, 0); set_sz(cpu.A); cpu.PC0 += 1; }

//20-2F
void LI(void) { cpu.A = f8_read_byte(cpu.PC0+1); cpu.PC0 += 2; }
void NI(void)   { clr_ozcs(); cpu.A &= f8_read_byte(cpu.PC0+1); set_sz(cpu.A); cpu.PC0 += 2; }
void OI(void)  { u8 imm=f8_read_byte(cpu.PC0+1); clr_ozcs(); cpu.A|=imm; set_sz(cpu.A); cpu.PC0+=2; }
void XI(void)  { u8 imm=f8_read_byte(cpu.PC0+1); clr_ozcs(); cpu.A^=imm; set_sz(cpu.A); cpu.PC0+=2; }
void AI(void)  { u8 imm=f8_read_byte(cpu.PC0+1); clr_ozcs(); cpu.A=do_add(cpu.A,imm,0); set_sz(cpu.A); cpu.PC0+=2; }
void CI(void) { u8 imm = f8_read_byte(cpu.PC0+1); clr_ozcs(); set_sz(do_add((u8)~cpu.A, imm, 1)); cpu.PC0 += 2; }
void IN(void) { u8 port = f8_read_byte(cpu.PC0+1); cpu.A = handle_video_port_read(port); cpu.PC0 += 2; }
void OUT(void) { u8 port = f8_read_byte(cpu.PC0+1); handle_video_port_writes(port, cpu.A); cpu.PC0 += 2; }
void PI(void)  { u8 hi = f8_read_byte(cpu.PC0+1), lo = f8_read_byte(cpu.PC0+2); cpu.PC1 = cpu.PC0 + 3; cpu.PC0 = (hi << 8) | lo; }
void JMP(void) { u8 hi = f8_read_byte(cpu.PC0+1), lo = f8_read_byte(cpu.PC0+2); cpu.PC0 = (hi << 8) | lo; }
void DCI(void) { u8 hi = f8_read_byte(cpu.PC0+1), lo = f8_read_byte(cpu.PC0+2); cpu.DC0 = (hi << 8) | lo; cpu.PC0 += 3; }
void NOP(void) { cpu.PC0 += 1; }
void XDC(void) { /* swap DC0/DC1 if i add them */ cpu.PC0 += 1; }
void unassigned_0x2D(void) { cpu.PC0 += 1; }
void unassigned_0x2E(void) { cpu.PC0 += 1; }
void unassigned_0x2F(void) { cpu.PC0 += 1; }

//30-3F
void DS_R0(void) { cpu.scratchpad[0]--; helper_ds_flags(cpu.scratchpad[0]); cpu.PC0 += 1; }
void DS_R1(void) { cpu.scratchpad[1]--; helper_ds_flags(cpu.scratchpad[1]); cpu.PC0 += 1; }
void DS_R2(void) { cpu.scratchpad[2]--; helper_ds_flags(cpu.scratchpad[2]); cpu.PC0 += 1; }
void DS_R3(void) { cpu.scratchpad[3]--; helper_ds_flags(cpu.scratchpad[3]); cpu.PC0 += 1; }
void DS_R4(void) { cpu.scratchpad[4]--; helper_ds_flags(cpu.scratchpad[4]); cpu.PC0 += 1; }
void DS_R5(void) { cpu.scratchpad[5]--; helper_ds_flags(cpu.scratchpad[5]); cpu.PC0 += 1; }
void DS_R6(void) { cpu.scratchpad[6]--; helper_ds_flags(cpu.scratchpad[6]); cpu.PC0 += 1; }
void DS_R7(void) { cpu.scratchpad[7]--; helper_ds_flags(cpu.scratchpad[7]); cpu.PC0 += 1; }
void DS_R8(void) { cpu.scratchpad[8]--; helper_ds_flags(cpu.scratchpad[8]); cpu.PC0 += 1; }
void DS_R9(void) { cpu.scratchpad[9]--; helper_ds_flags(cpu.scratchpad[9]); cpu.PC0 += 1; }
void DS_R10(void) { cpu.scratchpad[10]--; helper_ds_flags(cpu.scratchpad[10]); cpu.PC0 += 1; }
void DS_R11(void) { cpu.scratchpad[11]--; helper_ds_flags(cpu.scratchpad[11]); cpu.PC0 += 1; }
void DS_S(void) { u8 r = isar_addr_direct(); cpu.scratchpad[r]--; helper_ds_flags(cpu.scratchpad[r]); cpu.PC0 += 1; }
void DS_I(void) { u8 r = isar_addr_inc(); cpu.scratchpad[r]--; helper_ds_flags(cpu.scratchpad[r]); cpu.PC0 += 1; }
void DS_D(void) { u8 r = isar_addr_dec(); cpu.scratchpad[r]--; helper_ds_flags(cpu.scratchpad[r]); cpu.PC0 += 1; }
void unassigned_0x3F(void) { cpu.PC0 += 1; }

//40-4F
void LR_A_R0(void) { cpu.A = cpu.scratchpad[0]; cpu.PC0 += 1; }
void LR_A_R1(void) { cpu.A = cpu.scratchpad[1]; cpu.PC0 += 1; }
void LR_A_R2(void) { cpu.A = cpu.scratchpad[2]; cpu.PC0 += 1; }
void LR_A_R3(void) { cpu.A = cpu.scratchpad[3]; cpu.PC0 += 1; }
void LR_A_R4(void) { cpu.A = cpu.scratchpad[4]; cpu.PC0 += 1; }
void LR_A_R5(void) { cpu.A = cpu.scratchpad[5]; cpu.PC0 += 1; }
void LR_A_R6(void) { cpu.A = cpu.scratchpad[6]; cpu.PC0 += 1; }
void LR_A_R7(void) { cpu.A = cpu.scratchpad[7]; cpu.PC0 += 1; }
void LR_A_R8(void) { cpu.A = cpu.scratchpad[8]; cpu.PC0 += 1; }
void LR_A_R9(void) { cpu.A = cpu.scratchpad[9]; cpu.PC0 += 1; }
void LR_A_R10(void) { cpu.A = cpu.scratchpad[10]; cpu.PC0 += 1; }
void LR_A_R11(void) { cpu.A = cpu.scratchpad[11]; cpu.PC0 += 1; }
void LR_A_S(void) { u8 r = isar_addr_direct(); cpu.A = cpu.scratchpad[r]; cpu.PC0 += 1; }
void LR_A_I(void) { u8 r = isar_addr_inc(); cpu.A = cpu.scratchpad[r]; cpu.PC0 += 1; }
void LR_A_D(void) { u8 r = isar_addr_dec(); cpu.A = cpu.scratchpad[r]; cpu.PC0 += 1; }
void unassigned_0x4F(void) { cpu.PC0 += 1; }

//50-5F
void LR_R0_A(void) { cpu.scratchpad[0] = cpu.A; cpu.PC0 += 1; }
void LR_R1_A(void) { cpu.scratchpad[1] = cpu.A; cpu.PC0 += 1; }
void LR_R2_A(void) { cpu.scratchpad[2] = cpu.A; cpu.PC0 += 1; }
void LR_R3_A(void) { cpu.scratchpad[3] = cpu.A; cpu.PC0 += 1; }
void LR_R4_A(void) { cpu.scratchpad[4] = cpu.A; cpu.PC0 += 1; }
void LR_R5_A(void) { cpu.scratchpad[5] = cpu.A; cpu.PC0 += 1; }
void LR_R6_A(void) { cpu.scratchpad[6] = cpu.A; cpu.PC0 += 1; }
void LR_R7_A(void) { cpu.scratchpad[7] = cpu.A; cpu.PC0 += 1; }
void LR_R8_A(void) { cpu.scratchpad[8] = cpu.A; cpu.PC0 += 1; }
void LR_R9_A(void) { cpu.scratchpad[9] = cpu.A; cpu.PC0 += 1; }
void LR_R10_A(void) { cpu.scratchpad[10] = cpu.A; cpu.PC0 += 1; }
void LR_R11_A(void) { cpu.scratchpad[11] = cpu.A; cpu.PC0 += 1; }
void LR_S_A(void) { u8 r = isar_addr_direct(); cpu.scratchpad[r] = cpu.A; cpu.PC0 += 1; }
void LR_I_A(void) { u8 r = isar_addr_inc(); cpu.scratchpad[r] = cpu.A; cpu.PC0 += 1; }
void LR_D_A(void) { u8 r = isar_addr_dec(); cpu.scratchpad[r] = cpu.A; cpu.PC0 += 1; }
void unassigned_0x5F(void) { cpu.PC0 += 1; }

//60-6F
void LISU_0(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (0 << 3); cpu.PC0 += 1; }
void LISU_1(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (1 << 3); cpu.PC0 += 1; }
void LISU_2(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (2 << 3); cpu.PC0 += 1; }
void LISU_3(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (3 << 3); cpu.PC0 += 1; }
void LISU_4(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (4 << 3); cpu.PC0 += 1; }
void LISU_5(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (5 << 3); cpu.PC0 += 1; }
void LISU_6(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (6 << 3); cpu.PC0 += 1; }
void LISU_7(void) { cpu.ISAR = (cpu.ISAR & 0x07) | (7 << 3); cpu.PC0 += 1; }
void LISL_0(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 0; cpu.PC0 += 1; }
void LISL_1(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 1; cpu.PC0 += 1; }
void LISL_2(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 2; cpu.PC0 += 1; }
void LISL_3(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 3; cpu.PC0 += 1; }
void LISL_4(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 4; cpu.PC0 += 1; }
void LISL_5(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 5; cpu.PC0 += 1; }
void LISL_6(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 6; cpu.PC0 += 1; }
void LISL_7(void) { cpu.ISAR = (cpu.ISAR & 0x38) | 7; cpu.PC0 += 1; }

//70-7F
void CLR(void) { cpu.A = 0; cpu.PC0 += 1; }
void LIS_1(void) { cpu.A = 1; cpu.PC0 += 1; }
void LIS_2(void) { cpu.A = 2; cpu.PC0 += 1; }
void LIS_3(void) { cpu.A = 3; cpu.PC0 += 1; }
void LIS_4(void) { cpu.A = 4; cpu.PC0 += 1; }
void LIS_5(void) { cpu.A = 5; cpu.PC0 += 1; }
void LIS_6(void) { cpu.A = 6; cpu.PC0 += 1; }
void LIS_7(void) { cpu.A = 7; cpu.PC0 += 1; }
void LIS_8(void) { cpu.A = 8; cpu.PC0 += 1; }
void LIS_9(void) { cpu.A = 9; cpu.PC0 += 1; }
void LIS_10(void) { cpu.A = 10; cpu.PC0 += 1; }
void LIS_11(void) { cpu.A = 11; cpu.PC0 += 1; }
void LIS_12(void) { cpu.A = 12; cpu.PC0 += 1; }
void LIS_13(void) { cpu.A = 13; cpu.PC0 += 1; }
void LIS_14(void) { cpu.A = 14; cpu.PC0 += 1; }
void LIS_15(void) { cpu.A = 15; cpu.PC0 += 1; }

//80-8F
void BT_0(void) { helper_bt(0x0); cpu.PC0 = cpu.PC0; }
void BP(void) { helper_bt(0x1); }
void BC(void) { helper_bt(0x2); }
void BT_3(void) { helper_bt(0x3); }
void BZ(void) { helper_bt(0x4); }
void BT_5(void) { helper_bt(0x5); }
void BT_6(void) { helper_bt(0x6); }
void BT_7(void) { helper_bt(0x7); }
void AM(void)  { u8 m=f8_read_byte(cpu.DC0); clr_ozcs(); cpu.A=do_add(cpu.A,m,0); set_sz(cpu.A); cpu.DC0++; cpu.PC0+=1; }
void AMD(void) { cpu.A = do_add_decimal(cpu.A, f8_read_byte(cpu.DC0)); cpu.DC0++; cpu.PC0 += 1; } //advanced microdevices used to help hthe IRS spy on us using the TV and for mole pople to plan their attack....
void NM(void)  { u8 m=f8_read_byte(cpu.DC0); clr_ozcs(); cpu.A&=m; set_sz(cpu.A); cpu.DC0++; cpu.PC0+=1; }
void OM(void)  { u8 m=f8_read_byte(cpu.DC0); clr_ozcs(); cpu.A|=m; set_sz(cpu.A); cpu.DC0++; cpu.PC0+=1; }
void XM(void)  { u8 m=f8_read_byte(cpu.DC0); clr_ozcs(); cpu.A^=m; set_sz(cpu.A); cpu.DC0++; cpu.PC0+=1; }
void CM(void) { u8 m = f8_read_byte(cpu.DC0); clr_ozcs(); set_sz(do_add((u8)~cpu.A, m, 1)); cpu.DC0++; cpu.PC0 += 1; }
void ADC_(void) { cpu.DC0 += (s8)cpu.A; cpu.PC0 += 1; }
void BR7(void) { if ((cpu.ISAR & 0x07) != 7) { s8 off=(s8)f8_read_byte(cpu.PC0+1); cpu.PC0 += 1+off; } else cpu.PC0 += 2; }

//90-9F
void BR(void) { s8 off=(s8)f8_read_byte(cpu.PC0+1); cpu.PC0 += 1+off; }
void BM(void) { if ((cpu.W & 0x01) == 0) { s8 off=(s8)f8_read_byte(cpu.PC0+1); cpu.PC0+=1+off; } else cpu.PC0 += 2; }
void BNC(void) { helper_bf(0x2); }
void BF_3(void) { helper_bf(0x3); }
void BNZ(void) { helper_bf(0x4); }
void BF_5(void) { helper_bf(0x5); }
void BF_6(void) { helper_bf(0x6); }
void BF_7(void) { helper_bf(0x7); }
void BNO(void) { helper_bf(0x8); }
void BF_9(void) { helper_bf(0x9); }
void BF_10(void) { helper_bf(0xA); }
void BF_11(void) { helper_bf(0xB); }
void BF_12(void) { helper_bf(0xC); }
void BF_13(void) { helper_bf(0xD); }
void BF_14(void) { helper_bf(0xE); }
void BF_15(void) { helper_bf(0xF); }

//A0-AF
void INS_0(void) { cpu.A = handle_video_port_read(0); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_1(void) { cpu.A = handle_video_port_read(1); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_2(void) { cpu.A = handle_video_port_read(2); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_3(void) { cpu.A = handle_video_port_read(3); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_4(void) { cpu.A = handle_video_port_read(4); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_5(void) { cpu.A = handle_video_port_read(5); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_6(void) { cpu.A = handle_video_port_read(6); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_7(void) { cpu.A = handle_video_port_read(7); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_8(void) { cpu.A = handle_video_port_read(8); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_9(void) { cpu.A = handle_video_port_read(9); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_10(void) { cpu.A = handle_video_port_read(10); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_11(void) { cpu.A = handle_video_port_read(11); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_12(void) { cpu.A = handle_video_port_read(12); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_13(void) { cpu.A = handle_video_port_read(13); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_14(void) { cpu.A = handle_video_port_read(14); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }
void INS_15(void) { cpu.A = handle_video_port_read(15); clr_ozcs(); set_sz(cpu.A); cpu.PC0 += 1; }

//B0-BF
void OUT_0(void) { handle_video_port_writes(0, cpu.A);  cpu.PC0 += 1; }
void OUT_1(void) { handle_video_port_writes(1, cpu.A);  cpu.PC0 += 1; }
void OUT_2(void) { handle_video_port_writes(2, cpu.A);  cpu.PC0 += 1; }
void OUT_3(void) { handle_video_port_writes(3, cpu.A);  cpu.PC0 += 1; }
void OUT_4(void) { handle_video_port_writes(4, cpu.A);  cpu.PC0 += 1; }
void OUT_5(void) { handle_video_port_writes(5, cpu.A);  cpu.PC0 += 1; }
void OUT_6(void) { handle_video_port_writes(6, cpu.A);  cpu.PC0 += 1; }
void OUT_7(void) { handle_video_port_writes(7, cpu.A);  cpu.PC0 += 1; }
void OUT_8(void) { handle_video_port_writes(8, cpu.A);  cpu.PC0 += 1; }
void OUT_9(void) { handle_video_port_writes(9, cpu.A);  cpu.PC0 += 1; }
void OUT_10(void) { handle_video_port_writes(10, cpu.A); cpu.PC0 += 1; }
void OUT_11(void) { handle_video_port_writes(11, cpu.A); cpu.PC0 += 1; }
void OUT_12(void) { handle_video_port_writes(12, cpu.A); cpu.PC0 += 1; }
void OUT_13(void) { handle_video_port_writes(13, cpu.A); cpu.PC0 += 1; }
void OUT_14(void) { handle_video_port_writes(14, cpu.A); cpu.PC0 += 1; }
void OUT_15(void) { handle_video_port_writes(15, cpu.A); cpu.PC0 += 1; }

//C0-CF
void AS_R0(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[0],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R1(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[1],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R2(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[2],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R3(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[3],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R4(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[4],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R5(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[5],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R6(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[6],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R7(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[7],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R8(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[8],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R9(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[9],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R10(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[10],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_R11(void) { clr_ozcs(); cpu.A=do_add(cpu.A,cpu.scratchpad[11],0); set_sz(cpu.A); cpu.PC0+=1; }
void AS_S(void) { u8 r = isar_addr_direct(); clr_ozcs(); cpu.A = do_add(cpu.A, cpu.scratchpad[r], 0); set_sz(cpu.A); cpu.PC0 += 1; }
void AS_I(void) { u8 r = isar_addr_inc(); clr_ozcs(); cpu.A = do_add(cpu.A, cpu.scratchpad[r], 0); set_sz(cpu.A); cpu.PC0 += 1; }
void AS_D(void) { u8 r = isar_addr_dec(); clr_ozcs(); cpu.A = do_add(cpu.A, cpu.scratchpad[r], 0); set_sz(cpu.A); cpu.PC0 += 1; }

void unassigned_0xCF(void) { cpu.PC0 += 1; }

//D0-DF
void ASD_R0(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[0]); cpu.PC0 += 1; }
void ASD_R1(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[1]); cpu.PC0 += 1; }
void ASD_R2(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[2]); cpu.PC0 += 1; }
void ASD_R3(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[3]); cpu.PC0 += 1; }
void ASD_R4(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[4]); cpu.PC0 += 1; }
void ASD_R5(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[5]); cpu.PC0 += 1; }
void ASD_R6(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[6]); cpu.PC0 += 1; }
void ASD_R7(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[7]); cpu.PC0 += 1; }
void ASD_R8(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[8]); cpu.PC0 += 1; }
void ASD_R9(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[9]); cpu.PC0 += 1; }
void ASD_R10(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[10]); cpu.PC0 += 1; }
void ASD_R11(void) { cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[11]); cpu.PC0 += 1; }
void ASD_S(void) { u8 r = isar_addr_direct(); cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[r]); cpu.PC0 += 1; }
void ASD_I(void) { u8 r = isar_addr_inc(); cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[r]); cpu.PC0 += 1; }
void ASD_D(void) { u8 r = isar_addr_dec(); cpu.A = do_add_decimal(cpu.A, cpu.scratchpad[r]); cpu.PC0 += 1; }
void unassigned_0xDF(void) { cpu.PC0 += 1; }

//E0-EF
void XS_R0(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[0]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R1(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[1]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R2(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[2]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R3(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[3]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R4(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[4]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R5(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[5]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R6(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[6]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R7(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[7]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R8(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[8]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R9(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[9]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R10(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[10]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_R11(void) { clr_ozcs(); cpu.A^=cpu.scratchpad[11]; set_sz(cpu.A); cpu.PC0+=1; }
void XS_S(void) { u8 r = isar_addr_direct(); clr_ozcs(); cpu.A ^= cpu.scratchpad[r]; set_sz(cpu.A); cpu.PC0 += 1; }
void XS_I(void) { u8 r = isar_addr_inc(); clr_ozcs(); cpu.A ^= cpu.scratchpad[r]; set_sz(cpu.A); cpu.PC0 += 1; }
void XS_D(void) { u8 r = isar_addr_dec(); clr_ozcs(); cpu.A ^= cpu.scratchpad[r]; set_sz(cpu.A); cpu.PC0 += 1; }

void unassigned_0xEF(void) { cpu.PC0 += 1; }

//F0-FF - NS (AND with scratchpad)
void NS_R0(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[0]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R1(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[1]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R2(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[2]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R3(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[3]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R4(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[4]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R5(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[5]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R6(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[6]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R7(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[7]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R8(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[8]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R9(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[9]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R10(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[10]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_R11(void) { clr_ozcs(); cpu.A&=cpu.scratchpad[11]; set_sz(cpu.A); cpu.PC0+=1; }
void NS_S(void) { u8 r = isar_addr_direct(); clr_ozcs(); cpu.A &= cpu.scratchpad[r]; set_sz(cpu.A); cpu.PC0 += 1; }
void NS_I(void) { u8 r = isar_addr_inc(); clr_ozcs(); cpu.A &= cpu.scratchpad[r]; set_sz(cpu.A); cpu.PC0 += 1; }
void NS_D(void) { u8 r = isar_addr_dec(); clr_ozcs(); cpu.A &= cpu.scratchpad[r]; set_sz(cpu.A); cpu.PC0 += 1; }
void unassigned_0xFF(void) { cpu.PC0 += 1; }

