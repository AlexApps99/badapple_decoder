// TODO make the file format's first two bytes specify W and H
// That way it's a lot more flexible
// TODO make the decoding more efficient
// E.g delta frames can be implemented with a different write mode on the display driver
// Frame clearing/no change can be implemented trivially as well

#include "dispbios.h"
#include "filebios.h"
#include "fxlib.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "timer.h"

#define W 80
#define H 64
#define PIXELS (W * H)
#define VRAM_BYTES (PIXELS / 8)
#define MIN_RLE_SIZE (PIXELS / 128)

static unsigned char vram[VRAM_BYTES] = {0};
static unsigned char buf[MIN_RLE_SIZE] = {0};
static unsigned short int pixels = 0;
static int f = -1;
const FONTCHARACTER file_name[] = {'\\', '\\', 'c', 'r', 'd', '0', '\\', 'B',
                                   'A',  'D',  '.', 'e', 'n', 'c', 0};

static GRAPHDATA screen;
static DISPGRAPH area;

static void full() { Bfile_ReadFile(f, vram, VRAM_BYTES, -1); }

static void rle_dec(unsigned char data) {
  char n = data >> 1;
  unsigned char on = data & 1;
  // fprintf(stderr, "%d %d %d %d %d\n", i, buf[i], pixels, n, on);
  unsigned char offset = pixels % 8;
  unsigned char old_vram = vram[pixels / 8];
  if (offset != 0) {
    // Set from point unwritten to, to end of byte
    if (on != 0) {
      vram[pixels / 8] |= 0xFF >> offset;
    } else {
      vram[pixels / 8] &= ~(0xFF >> offset);
    }
    // If the rest of the byte is entirely filled
    if (n >= 8 - offset) {
      pixels += 8 - offset;
      n -= 8 - offset;
    } else {
      // Overwrote too much, need to correct
      n -= 8 - offset;
      vram[pixels / 8] &= 0xFF << -n;
      vram[pixels / 8] |= old_vram & ~(0xFF << -n);
      pixels += 8 - offset + n;
      n = 0;
    }
  }
  // Any overlap will be corrected below
  while (n > 0) {
    old_vram = vram[pixels / 8];
    vram[pixels / 8] = 0xFF * on;
    pixels += 8;
    n -= 8;
  }
  // overlap
  if (n != 0) {
    // Overwrote too much, need to correct
    pixels -= 8;
    vram[pixels / 8] &= 0xFF << -n;
    vram[pixels / 8] |= old_vram & ~(0xFF << -n);
    pixels += 8;
    pixels += n;
    n = 0;
  }
}

static void rle_dec_d(unsigned char data) {
  char n = data >> 1;
  unsigned char on = data & 1;
  // fprintf(stderr, "%d %d %d %d %d\n", i, buf[i], pixels, n, on);
  unsigned char offset = pixels % 8;
  unsigned char mask = 0;
  unsigned char old_vram = 0;
  if (offset != 0) {
    // Set from point unwritten to, to end of byte
    if (on != 0) {
      mask = 0xFF >> offset;
    }
    // If the rest of the byte is entirely filled
    if (n >= 8 - offset) {
      vram[pixels / 8] ^= mask;
      pixels += 8 - offset;
      n -= 8 - offset;
    } else {
      // Overwrote too much, need to correct
      n -= 8 - offset;
      mask &= 0xFF << -n;
      vram[pixels / 8] ^= mask;
      pixels += 8 - offset + n;
      n = 0;
    }
  }
  // Any overlap will be corrected below
  // The code below here is sus AF
  while (n > 0) {
    old_vram = vram[pixels / 8];
    vram[pixels / 8] ^= 0xFF * on;
    pixels += 8;
    n -= 8;
  }
  // overlap
  if (n != 0) {
    // Overwrote too much, need to correct
    pixels -= 8;
    vram[pixels / 8] = old_vram ^ ((0xFF << -n) * on);
    pixels += 8;
    pixels += n;
    n = 0;
  }
}

static void full_rle() {
  unsigned char i = 0;
  pixels = 0;
  // By reading a lot at once this will
  // mean less read calls
  Bfile_ReadFile(f, buf, MIN_RLE_SIZE, -1);
  for (; i < MIN_RLE_SIZE; i++) {
    rle_dec(buf[i]);
  }
  while (pixels < PIXELS) {
    Bfile_ReadFile(f, buf, 1, -1);
    rle_dec(buf[0]);
  }
}

static void delta() {
  unsigned char i = 0;
  pixels = 0;
  // By reading a lot at once this will
  // mean less read calls
  Bfile_ReadFile(f, buf, MIN_RLE_SIZE, -1);
  for (; i < MIN_RLE_SIZE; i++) {
    rle_dec_d(buf[i]);
  }
  while (pixels < PIXELS) {
    Bfile_ReadFile(f, buf, 1, -1);
    rle_dec_d(buf[0]);
  }
}

static void fill(unsigned char col) { memset(vram, col, VRAM_BYTES); }

static void decode(unsigned char code) {
  switch (code) {
  case 0: // Blank
    break;
  case 1: // Full
    full();
    break;
  case 2: // Full (RLE)
    full_rle();
    break;
  case 3: // Delta
    delta();
    break;
  case 4: // Clear screen
    fill(0x00);
    break;
  case 5: // Clear screen
    fill(0xFF);
    break;
  default:
    break;
  }
}

static void quit() {
  KillTimer(ID_USER_TIMER1);
  Bfile_CloseFile(f);
}

static void step_frame() {
  if (!Bfile_ReadFile(f, buf, 1, -1) == 1) {
    Bfile_SeekFile(f, 0);
    Bfile_ReadFile(f, buf, 1, -1);
  }
  decode(buf[0]);
  Bdisp_WriteGraph_VRAM(&area);
  Bdisp_PutDisp_DD();
}

int AddIn_main(int _unused1, unsigned short _unused2) {
  unsigned int key = 0;
  screen.width = W;
  screen.height = H;
  screen.pBitmap = vram;
  area.x = ((128 - W) / 2);
  area.y = 0;
  area.GraphData = screen;
  area.WriteModify = IMB_WRITEMODIFY_NORMAL;
  area.WriteKind = IMB_WRITEKIND_OVER;
  Bdisp_AllClr_DDVRAM();
  f = Bfile_OpenFile(file_name, _OPENMODE_READ);
  if (f < 0)
    return -1;
  SetQuitHandler(quit);
  SetTimer(ID_USER_TIMER1, 50, step_frame);
  while (key != KEY_CTRL_AC) {
    GetKey(&key);
  }
  quit();
  return 0;
}

// Please do not change the following source.
#pragma section _BR_Size
unsigned long BR_Size;
#pragma section
#pragma section _TOP
int InitializeSystem(int isAppli, unsigned short OptionNum) {
  return INIT_ADDIN_APPLICATION(isAppli, OptionNum);
}
#pragma section
