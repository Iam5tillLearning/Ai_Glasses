#include "lvgl/lvgl.h"
#include <jbd013_api.h>
#include <hal_driver.h>

int clr_char(void);
int display_string(const char* text);
uint32_t custom_tick_get(void);