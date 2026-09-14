#ifndef GECND_SDL2_SHIM_H
#define GECND_SDL2_SHIM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "../ipc.h"

void     shim_set_error(const char *fmt, ...);
uint64_t shim_now_ms(void);

void     shim_events_init(void);
void     shim_events_quit(void);
void     shim_events_push(const SDL_Event *ev);
void     shim_events_key(uint16_t scancode, uint32_t keycode, bool pressed);
void     shim_events_window(uint8_t event, int32_t data1, int32_t data2);
void     shim_events_pump(void);

void     shim_ipc_connect(void);
void     shim_ipc_close(void);
void     shim_ipc_send(uint8_t type, uint8_t flag, uint16_t code, uint32_t arg);
void     shim_ipc_pump(void);

void     shim_joystick_announce(void);
void     shim_joystick_input(uint8_t pad, bool pressed);
void     shim_joystick_quit(void);

void     shim_video_pump(void);
void     shim_video_quit(void);
uint32_t shim_window_id(void);

#endif
