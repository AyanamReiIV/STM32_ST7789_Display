#ifndef __ST7789_H__
#define __ST7789_H__

#include "main.h"

// 引脚定义
#define DC_PIN    GPIO_PIN_1
#define DC_PORT   GPIOB
#define RST_PIN   GPIO_PIN_3
#define RST_PORT  GPIOA
#define CS_PIN    GPIO_PIN_4
#define CS_PORT   GPIOA
#define BLK_PIN   GPIO_PIN_0
#define BLK_PORT  GPIOB

// 颜色定义
#define COLOR_RGB(r,g,b) (((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3))
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_BLACK		0x0000

#define BACKGROUND_COLOR COLOR_WHITE  // 自定义背景颜色

//屏幕尺寸
// ????:240x320
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

#define COL_MAX     239
#define ROW_MAX     319
#define Y_OFFSET    0

// 显示方向
typedef enum {
    DISPLAY_ROTATION_0,
    DISPLAY_ROTATION_90,
    DISPLAY_ROTATION_180,
    DISPLAY_ROTATION_270
} DisplayRotation;

//字符尺寸（高*宽）
typedef enum{
	CHAR_SIZE_1206,
	CHAR_SIZE_1608,
	CHAR_SIZE_2412,
	CHAR_SIZE_3216,
	CHAR_SIZE_DEFAULT
}CharSize;

void ST7789_Init(void);
void ST7789_SetRotation(DisplayRotation rotation);
void ST7789_SetWindow(uint16_t xStart, uint16_t xEnd, uint16_t yStart, uint16_t yEnd);
void ST7789_DrawPixel(uint16_t x,uint16_t y,uint16_t color);
void ST7789_DrawChar(uint16_t x,uint16_t y,char c, CharSize size ,uint16_t color,uint16_t bg_color);
void ST7789_DrawString(uint16_t x, uint16_t y, const char *str,CharSize size, uint16_t color, uint16_t bg_color);
void ST7789_DrawLine(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void ST7789_SetScreen(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color);
void ST7789_FillScreen(uint16_t color);
void ST7789_DrawChinese24x24(uint16_t x, uint16_t y, uint16_t color,uint8_t count) ;
void ST7789_DrawChinese240x240(uint16_t x, uint16_t y, const uint8_t *code, uint16_t color, uint16_t bg_color);
void ST7789_DrawImage_40x40(uint16_t x, uint16_t y, const unsigned char *img);
void ST7789_DrawImage_80x80(uint16_t x, uint16_t y, const unsigned char *img);
void ST7789_DrawImage_240x240(uint16_t x, uint16_t y, const unsigned char *img, uint16_t color);
void ST7789_RotationDisplayTest(void);
void ST7789_DrawStringTest(void);
void ST7789_DrawChineseTest(void);
void ST7789_DrawImageTest(void);
#endif //__ST7789_H__
