#include "s31/gb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gnuboy.h"

struct s31_gb { uint16_t *frame; int16_t *audio; uint8_t *rom; s31_buttons_t buttons; bool loaded; bool presented; };
__attribute__((weak)) int s31_board_present_gb(const uint16_t *pixels, uint16_t width, uint16_t height) { (void)pixels;(void)width;(void)height;return 0; }
static s31_gb_t *active;
static void video_cb(void *buffer) { if(active&&!active->presented){active->presented=true;printf("GB first frame\n");}s31_board_present_gb(buffer, GB_WIDTH, GB_HEIGHT); }
static void audio_cb(void *buffer, size_t length) { (void)buffer; (void)length; }
static int pad(s31_buttons_t b) { return ((b&S31_BUTTON_RIGHT)?GB_PAD_RIGHT:0)|((b&S31_BUTTON_LEFT)?GB_PAD_LEFT:0)|((b&S31_BUTTON_UP)?GB_PAD_UP:0)|((b&S31_BUTTON_DOWN)?GB_PAD_DOWN:0)|((b&S31_BUTTON_A)?GB_PAD_A:0)|((b&S31_BUTTON_B)?GB_PAD_B:0)|((b&S31_BUTTON_SELECT)?GB_PAD_SELECT:0)|((b&S31_BUTTON_START)?GB_PAD_START:0); }
s31_gb_t *s31_gb_create(void){s31_gb_t *g=calloc(1,sizeof(*g));if(!g)return NULL;g->frame=calloc(GB_WIDTH*GB_HEIGHT,sizeof(uint16_t));g->audio=calloc(2048,sizeof(int16_t));if(!g->frame||!g->audio||gnuboy_init(48000,GB_AUDIO_STEREO_S16,GB_PIXEL_565_LE,video_cb,audio_cb)){s31_gb_destroy(g);return NULL;}gnuboy_set_framebuffer(g->frame);gnuboy_set_soundbuffer(g->audio,2048);active=g;return g;}
void s31_gb_destroy(s31_gb_t *g){if(!g)return;gnuboy_free_rom();free(g->rom);free(g->frame);free(g->audio);if(active==g)active=NULL;free(g);}
int s31_gb_load_rom(s31_gb_t *g,const uint8_t *d,size_t n){if(!g||!d||!n||g->loaded)return -1;g->rom=malloc(n);if(!g->rom)return -1;memcpy(g->rom,d,n);if(gnuboy_load_rom(g->rom,n)){free(g->rom);g->rom=NULL;return -1;}gnuboy_reset(true);g->loaded=true;return 0;}
void s31_gb_set_input(s31_gb_t *g,s31_buttons_t b){if(g)g->buttons=b;}
void s31_gb_run_frame(s31_gb_t *g){if(g&&g->loaded){gnuboy_set_pad(pad(g->buttons));gnuboy_run(true);}}
int s31_gb_load_sram(s31_gb_t *g,const char *p){return g&&g->loaded?gnuboy_load_sram(p):-1;}
int s31_gb_save_sram(s31_gb_t *g,const char *p){return g&&g->loaded?gnuboy_save_sram(p,false):-1;}
