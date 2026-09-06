/*
 * expression.h - 表情引擎 (SDCC)
 */
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "protocol_sdcc.h"

void expr_init(void);
void expr_set_face(unsigned char expr_id);
void expr_set_custom(unsigned char *seg_data);
void expr_set_led(unsigned char led_val);
void expr_play_sound(unsigned char sound_id);
void expr_set_all(unsigned char expr_id, unsigned char led, unsigned char sound_id);
void expr_animate(void);

#endif
