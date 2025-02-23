#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psxgpu.h>
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/string.h>
#include <mruby/array.h>

#include "spi.h"

static uint32_t *xfb = NULL;
// static GXRModeObj *rmode = NULL;

#define BUFSIZE 100

struct InputBuf {
  uint16_t buffer[BUFSIZE];
  uint32_t index;
} input_buf;

static mrb_value btn_mrb_buffer;

extern const uint8_t program[];

static mrb_value print_msg(mrb_state *mrb, mrb_value self) {
  char *unwrapped_content;
  mrb_value str_content;

  mrb_get_args(mrb, "S", &str_content);
  unwrapped_content = mrb_str_to_cstr(mrb, str_content);
  printf("%s\n", unwrapped_content);

  return mrb_nil_value();
}

// TODO: Just to make compiler happy
uint32_t PAD_BUTTON_START = 1;
uint32_t PAD_BUTTON_LEFT  = 2;
uint32_t PAD_BUTTON_RIGHT = 4;
uint32_t PAD_BUTTON_UP    = 8;
uint32_t PAD_BUTTON_DOWN  = 16;
uint32_t PAD_BUTTON_A     = 32;
uint32_t PAD_BUTTON_B     = 64;

static mrb_value get_button_masks(mrb_state *mrb, mrb_value self) {
  mrb_value mask_array;
  mask_array = mrb_ary_new(mrb);

  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_START));
  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_LEFT));
  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_RIGHT));
  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_UP));
  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_DOWN));
  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_A));
  mrb_ary_push(mrb, mask_array, mrb_fixnum_value(PAD_BUTTON_B));

  return mask_array;
}

static uint32_t CvtRGB (uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2)
{
	int y1, cb1, cr1, y2, cb2, cr2, cb, cr;

	y1 = (299 * r1 + 587 * g1 + 114 * b1) / 1000;
	cb1 = (-16874 * r1 - 33126 * g1 + 50000 * b1 + 12800000) / 100000;
	cr1 = (50000 * r1 - 41869 * g1 - 8131 * b1 + 12800000) / 100000;

	y2 = (299 * r2 + 587 * g2 + 114 * b2) / 1000;
	cb2 = (-16874 * r2 - 33126 * g2 + 50000 * b2 + 12800000) / 100000;
	cr2 = (50000 * r2 - 41869 * g2 - 8131 * b2 + 12800000) / 100000;

	cb = (cb1 + cb2) >> 1;
	cr = (cr1 + cr2) >> 1;

	return (y1 << 24) | (cb << 16) | (y2 << 8) | cr;
}

static uint32_t PACK_PIXEL(int r, int g, int b) {
  return CvtRGB(r, g, b, r, g, b);
}

static mrb_value draw20x20_640(mrb_state *mrb, mrb_value self) {
  mrb_int x, y, r, g, b;
  mrb_get_args(mrb, "iiiii", &x, &y, &r, &g, &b);

  int i = 0, j = 0;

  if(r == 0 && g == 0 && b == 0) {
    for(i = 0; i < 20; i++) {
      for(j = 0; j < 10; j++) {
        xfb[x+j + (y+i) * 320] = PACK_PIXEL(r, g, b);
      }
    }
  } else {
    int r_light = (r+128 <= 255) ? r+128 : 255;
    int g_light = (g+128 <= 255) ? g+128 : 255;
    int b_light = (b+128 <= 255) ? b+128 : 255;

    int r_dark = (r-64 >= 0) ? r-64 : 0;
    int g_dark = (g-64 >= 0) ? g-64 : 0;
    int b_dark = (b-64 >= 0) ? b-64 : 0;

    // TODO: implement lines and use them.
    for(j = 0; j < 10; j++) {
      xfb[x+j + (y) * 320] = PACK_PIXEL(30, 30, 30);
      xfb[x+j + (y+19) * 320] = PACK_PIXEL(30, 30, 30);
    }
    for(j = 1; j < 9; j++) {
      xfb[x+j + (y+1) * 320] = PACK_PIXEL(r_light, g_light, b_light);
    }
    for(j = 2; j < 10; j++) {
      xfb[x+j + (y+19) * 320] = PACK_PIXEL(r_dark, g_dark, b_dark);
    }
    for(i = 2; i < 19; i++) {
      xfb[x + (y+i) * 320] = PACK_PIXEL(30, 30, 30);
      xfb[x+1 + (y+i) * 320] = PACK_PIXEL(r_light, g_light, b_light);
      for(j = 2; j < 9; j++) {
        xfb[x+j + (y+i) * 320] = PACK_PIXEL(r, g, b);
      }
      xfb[x+9 + (y+i) * 320] = PACK_PIXEL(r_dark, g_dark, b_dark);
    }
  }

  return mrb_nil_value();
}

static mrb_value init_controller_buffer(mrb_state *mrb, mrb_value self) {
  btn_mrb_buffer = mrb_ary_new(mrb);;
  input_buf.index = 0;

  int i = 0;
  while(i < BUFSIZE) {
    mrb_ary_set(mrb, btn_mrb_buffer, i, mrb_nil_value());
    input_buf.buffer[i] = 0; i ++ ;
  }

  return mrb_nil_value();
}

void *read_buttons() {
/*
  while(1) {
    input_buf.index = (input_buf.index + 1) % BUFSIZE;
		PAD_ScanPads();
		uint16_t btns = PAD_ButtonsHeld(0);
		input_buf.buffer[input_buf.index] = btns;

		LWP_YieldThread();
  }
*/
}

static mrb_value start_controller_reader(mrb_state *mrb, mrb_value self) {
  // lwp_t thread;
  // LWP_CreateThread(&thread, read_buttons, NULL, NULL, 0, 0);
  return mrb_fixnum_value(0);
}

static mrb_value get_button_state(mrb_state *mrb, mrb_value self) {
  // PAD_ScanPads();
	// uint16_t btns = PAD_ButtonsHeld(0);
  // return mrb_fixnum_value(btns);
  return mrb_fixnum_value(0);
}

static mrb_value get_button_states(mrb_state *mrb, mrb_value self) {
  // unimplemented
  return mrb_fixnum_value(0);
}

static mrb_value start_btn(mrb_state *mrb, mrb_value self) {
  mrb_int state;
  mrb_get_args(mrb, "i", &state);

  return mrb_bool_value(state & PAD_BUTTON_START);
}

static mrb_value dpad_left(mrb_state *mrb, mrb_value self) {
  mrb_int state;
  mrb_get_args(mrb, "i", &state);

  return mrb_bool_value(state & PAD_BUTTON_LEFT);
}

static mrb_value dpad_right(mrb_state *mrb, mrb_value self) {
  mrb_int state;
  mrb_get_args(mrb, "i", &state);

  return mrb_bool_value(state & PAD_BUTTON_RIGHT);
}

static mrb_value dpad_up(mrb_state *mrb, mrb_value self) {
  // unimplemented
  return mrb_fixnum_value(0);
}

static mrb_value dpad_down(mrb_state *mrb, mrb_value self) {
  mrb_int state;
  mrb_get_args(mrb, "i", &state);

  return mrb_bool_value(state & PAD_BUTTON_DOWN);
}

static mrb_value btn_a(mrb_state *mrb, mrb_value self) {
  mrb_int state;
  mrb_get_args(mrb, "i", &state);

  return mrb_bool_value(state & PAD_BUTTON_A);
}

static mrb_value btn_b(mrb_state *mrb, mrb_value self) {
  mrb_int state;
  mrb_get_args(mrb, "i", &state);

  return mrb_bool_value(state & PAD_BUTTON_B);
}

static mrb_value clear_score(mrb_state *mrb, mrb_value self) {
  char* clear_str = "Press START";
  printf("\x1b[8;53H");
  printf("%s", clear_str);
  return mrb_nil_value();
}

static mrb_value render_score(mrb_state *mrb, mrb_value self) {
  mrb_int score;
  mrb_get_args(mrb, "i", &score);
  char buf[20];
  snprintf(buf, 20, "Score: %8" PRId32, score);
  printf("\x1b[8;53H");
  printf("%s", buf);

  return mrb_nil_value();
}

static mrb_value get_current_button_index(mrb_state *mrb, mrb_value self) {
  // unimplemented
  return mrb_fixnum_value(0);
}

static mrb_value waitvbl(mrb_state *mrb, mrb_value self) {
  // VIDEO_WaitVSync();
  return mrb_nil_value();
}

static mrb_value get_next_button_state(mrb_state *mrb, mrb_value self) {
  mrb_int wanted_index;
  mrb_get_args(mrb, "i", &wanted_index);
  int curr_index = input_buf.index;

  if(wanted_index >= BUFSIZE || wanted_index < 0) { wanted_index = wanted_index % BUFSIZE; }

  if(wanted_index == (curr_index + 1) % BUFSIZE) {
    return mrb_nil_value();
  } else {
    return mrb_fixnum_value(input_buf.buffer[wanted_index]);
  }
}

#define SCREEN_XRES 320
#define SCREEN_YRES 240

#define OT_LENGTH 16
#define BUFFER_LENGTH 8192

typedef struct {
  DISPENV disp_env;
  DRAWENV draw_env;

  uint32_t ot[OT_LENGTH];
  uint8_t  buffer[BUFFER_LENGTH];
} RenderBuffer;

typedef struct {
  RenderBuffer buffers[2];
  uint8_t      *next_packet;
  int          active_buffer;
} RenderContext;

// TODO: Global bad
RenderContext ctx;

void setup_context(RenderContext *ctx, int w, int h, int r, int g, int b) {
  // Place the two framebuffers vertically in VRAM.
  SetDefDrawEnv(&(ctx->buffers[0].draw_env), 0, 0, w, h);
  SetDefDispEnv(&(ctx->buffers[0].disp_env), 0, 0, w, h);
  SetDefDrawEnv(&(ctx->buffers[1].draw_env), 0, h, w, h);
  SetDefDispEnv(&(ctx->buffers[1].disp_env), 0, h, w, h);

  // Set the default background color and enable auto-clearing.
  setRGB0(&(ctx->buffers[0].draw_env), r, g, b);
  setRGB0(&(ctx->buffers[1].draw_env), r, g, b);
  ctx->buffers[0].draw_env.isbg = 1;
  ctx->buffers[1].draw_env.isbg = 1;

  // Initialize the first buffer and clear its OT so that it can be used for
  // drawing.
  ctx->active_buffer = 0;
  ctx->next_packet   = ctx->buffers[0].buffer;
  ClearOTagR(ctx->buffers[0].ot, OT_LENGTH);

  // Turn on the video output.
  SetDispMask(1);
}

static mrb_value init_context(mrb_state *mrb, mrb_value self) {
  setup_context(&ctx, SCREEN_XRES, SCREEN_YRES, 63, 0, 127);

  return mrb_nil_value();
}

void flip_buffers(RenderContext *ctx) {
  // Wait for the GPU to finish drawing, then wait for vblank in order to
  // prevent screen tearing.
  DrawSync(0);
  VSync(0);

  RenderBuffer *draw_buffer = &(ctx->buffers[ctx->active_buffer]);
  RenderBuffer *disp_buffer = &(ctx->buffers[ctx->active_buffer ^ 1]);

  // Display the framebuffer the GPU has just finished drawing and start
  // rendering the display list that was filled up in the main loop.
  PutDispEnv(&(disp_buffer->disp_env));
  DrawOTagEnv(&(draw_buffer->ot[OT_LENGTH - 1]), &(draw_buffer->draw_env));

  // Switch over to the next buffer, clear it and reset the packet allocation
  // pointer.
  ctx->active_buffer ^= 1;
  ctx->next_packet    = disp_buffer->buffer;
  ClearOTagR(disp_buffer->ot, OT_LENGTH);
}

static mrb_value flip_buffers_(mrb_state *mrb, mrb_value self) {
  flip_buffers(&ctx);

  return mrb_nil_value();
}

void *new_primitive(RenderContext *ctx, int z, size_t size) {
  // Place the primitive after all previously allocated primitives, then
  // insert it into the OT and bump the allocation pointer.
  RenderBuffer *buffer = &(ctx->buffers[ctx->active_buffer]);
  uint8_t      *prim   = ctx->next_packet;

  addPrim(&(buffer->ot[z]), prim);
  ctx->next_packet += size;

  // Make sure we haven't yet run out of space for future primitives.
  assert(ctx->next_packet <= &(buffer->buffer[BUFFER_LENGTH]));

  return (void *) prim;
}

static mrb_value draw_rect(mrb_state *mrb, mrb_value self) {
  mrb_int x, y, w, h, r, g, b;
  mrb_get_args(mrb, "iiiiiii", &x, &y, &w, &h, &r, &g, &b);

  TILE *tile = (TILE *) new_primitive(&ctx, 1, sizeof(TILE));

  setTile(tile);
  setXY0 (tile, x, y);
  setWH  (tile, w, h);
  setRGB0(tile, r, g, b);

  return mrb_nil_value();
}

static volatile uint8_t  pad_buff[2][34];
static volatile size_t   pad_buff_len[2];

void poll_cb(uint32_t port, const volatile uint8_t *buff, size_t rx_len) {
  // Copy the response to a persistent buffer so it can be accessed from the
  // main loop and displayed on screen.
  pad_buff_len[port] = rx_len;
  if (rx_len)
    memcpy((void *) pad_buff[port], (void *) buff, rx_len);

  PadResponse *pad = (PadResponse *) buff;
}

static mrb_value read_pad(mrb_state *mrb, mrb_value self) {
  uint8_t port = 0;
	for (uint32_t i = 0; i < pad_buff_len[port]; i++)
  printf(
			((i - 2) % 8) ? " %02x" : "\n %02x",
			pad_buff[port][i]
  );
  printf("pad_buff_len[port]: %d\n", pad_buff_len[port]);
  if (pad_buff_len[port] >= 2) {
    // let's take the latest one for now...
    return mrb_fixnum_value(
      pad_buff[port][pad_buff_len[port]-1] |
        (pad_buff[port][pad_buff_len[port]-2] << 8)
    );
  } else {
    return mrb_fixnum_value(65535);
  }
}

int errno = 0;

int main(int argc, char **argv) {
  ResetGraph(0);
  FntLoad(960, 0);
  SPI_Init(&poll_cb);

  mrb_state *mrb = mrb_open();
  if (!mrb) { return 1; }
  struct RClass *psx_mruby = mrb_define_module(mrb, "PsxMruby");
  mrb_define_module_function(mrb, psx_mruby, "print_msg", print_msg, MRB_ARGS_REQ(1));
  mrb_define_module_function(mrb, psx_mruby, "init_context", init_context, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, psx_mruby, "flip_buffers", flip_buffers_, MRB_ARGS_NONE());
  mrb_define_module_function(mrb, psx_mruby, "draw_rect", draw_rect, MRB_ARGS_REQ(2));
  mrb_define_module_function(mrb, psx_mruby, "read_pad", read_pad, MRB_ARGS_NONE());

  printf("**************************************\n\n");
  mrb_load_irep(mrb, program);
  printf("**************************************\n\n");

	return 0;
}
