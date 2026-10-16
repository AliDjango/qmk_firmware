#pragma once
#include <stdbool.h>
#include <stdint.h>

void brightness_popup_render(bool oled, bool power, uint8_t percent, uint32_t elapsed, uint16_t duration);

void rgb_effect_popup_render(uint8_t mode, uint32_t elapsed, uint16_t duration);

void rgb_setting_popup_render(const char *label, uint8_t percent, uint32_t elapsed, uint16_t duration);

void base_layer_popup_render(bool dvorak, uint32_t elapsed, uint16_t duration);
