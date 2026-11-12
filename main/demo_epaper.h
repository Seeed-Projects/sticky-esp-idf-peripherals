#pragma once

#include "epaper_panel.h"

// Init SPI bus, create SSD1677 panel, clear screen (full refresh), show gray4 demo.
// 初始化 SPI 总线、创建 SSD1677 面板、全刷清屏，并显示 gray4 演示。
seeed_epaper_panel_handle_t demo_epaper_init(void);

// Draw a filled dot (8x8 pixels) at the given coordinates using partial refresh.
// 使用局部刷新在指定坐标绘制一个 8x8 像素的实心点。
void demo_epaper_draw_dot(seeed_epaper_panel_handle_t panel, uint16_t x, uint16_t y);
