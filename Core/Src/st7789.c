#include "st7789.h"
#include "stdlib.h"

extern SPI_HandleTypeDef hspi1;

static SPI_HandleTypeDef *hspi = &hspi1;
static DisplayRotation g_rotation = DISPLAY_ROTATION_0;
static uint8_t madctl = 0x00;

/* GPIO ?? */
#define DC_SET_LOW()      do { HAL_GPIO_WritePin(DC_PORT,  DC_PIN,  GPIO_PIN_RESET); } while(0)
#define DC_SET_HIGH()     do { HAL_GPIO_WritePin(DC_PORT,  DC_PIN,  GPIO_PIN_SET);   } while(0)

#define CS_SET_LOW()      do { HAL_GPIO_WritePin(CS_PORT,  CS_PIN,  GPIO_PIN_RESET); } while(0)
#define CS_SET_HIGH()     do { HAL_GPIO_WritePin(CS_PORT,  CS_PIN,  GPIO_PIN_SET);   } while(0)

#define BLK_SET_LOW()     do { HAL_GPIO_WritePin(BLK_PORT, BLK_PIN, GPIO_PIN_RESET); } while(0)
#define BLK_SET_HIGH()    do { HAL_GPIO_WritePin(BLK_PORT, BLK_PIN, GPIO_PIN_SET);   } while(0)

#define RST_SET_LOW()     do { HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_RESET); } while(0)
#define RST_SET_HIGH()    do { HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_SET);   } while(0)


/* ???? */
static void WriteCommand(uint8_t cmd)
{
    CS_SET_LOW();
    DC_SET_LOW();
    HAL_SPI_Transmit(hspi, &cmd, 1, 100);
    CS_SET_HIGH();
}

static void WriteData(uint8_t data)
{
    CS_SET_LOW();
    DC_SET_HIGH();
    HAL_SPI_Transmit(hspi, &data, 1, 100);
    CS_SET_HIGH();
}

static void WriteMultiData(uint8_t *data, uint16_t len)
{
    CS_SET_LOW();
    DC_SET_HIGH();
    HAL_SPI_Transmit(hspi, data, len, 100);
    CS_SET_HIGH();
}

static void Swap(uint16_t *a, uint16_t *b)
{
    uint16_t temp;

    temp = *a;
    *a = *b;
    *b = temp;
}


/* ST7789 ??? */
void ST7789_Init(void)
{
    uint8_t porch_data[5]      = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    uint8_t power_data[2]      = {0xA6, 0xA1};
    uint8_t gamma_positive[14] = {0xD0,0x0D,0x14,0x0B,0x0B,0x07,0x3A,0x44,0x50,0x08,0x13,0x13,0x2D,0x32};
    uint8_t gamma_negative[14] = {0xD0,0x0D,0x14,0x0B,0x0B,0x07,0x3A,0x44,0x50,0x08,0x13,0x13,0x2D,0x32};

    /* ???? */
    BLK_SET_HIGH();

    /* ???? */
    RST_SET_LOW();
    HAL_Delay(150);
    RST_SET_HIGH();
    HAL_Delay(150);

    /* Sleep Out */
    WriteCommand(0x11);
    HAL_Delay(120);

    /* Porch setting */
    WriteCommand(0xB2);
    WriteMultiData(porch_data, sizeof(porch_data));

    /* Display inversion off */
    WriteCommand(0x20);

    /* Gate control */
    WriteCommand(0xB7);
    WriteData(0x56);

    /* VCOM */
    WriteCommand(0xBB);
    WriteData(0x18);

    /* LCM control */
    WriteCommand(0xC0);
    WriteData(0x2C);

    /* VDV and VRH command enable */
    WriteCommand(0xC2);
    WriteData(0x01);

    /* VRH set */
    WriteCommand(0xC3);
    WriteData(0x1F);

    /* VDV set */
    WriteCommand(0xC4);
    WriteData(0x20);

    /* Frame rate control */
    WriteCommand(0xC6);
    WriteData(0x0F);

    /* Power control */
    WriteCommand(0xD0);
    WriteMultiData(power_data, sizeof(power_data));

    /* Gamma */
    WriteCommand(0xE0);
    WriteMultiData(gamma_positive, sizeof(gamma_positive));

    WriteCommand(0xE1);
    WriteMultiData(gamma_negative, sizeof(gamma_negative));

    /* Memory access control */
    WriteCommand(0x36);
    WriteData(0x00);    /* ??,240x320 */

    /* RGB565 */
    WriteCommand(0x3A);
    WriteData(0x55);

    /* SPI ???? */
    WriteCommand(0xE7);
    WriteData(0x00);

    /* Display inversion on */
    WriteCommand(0x21);

    /* Display on */
    WriteCommand(0x29);
    HAL_Delay(100);

    g_rotation = DISPLAY_ROTATION_0;
}


/* ?????? */
void ST7789_SetRotation(DisplayRotation rotation)
{
    g_rotation = rotation;

    switch (rotation)
    {
        case DISPLAY_ROTATION_90:
            madctl = 0x60;
            break;

        case DISPLAY_ROTATION_180:
            madctl = 0xC0;
            break;

        case DISPLAY_ROTATION_270:
            madctl = 0xA0;
            break;

        case DISPLAY_ROTATION_0:
        default:
            madctl = 0x00;
            break;
    }

    WriteCommand(0x36);
    WriteData(madctl);
}


/* ??????:??????? xStart, yStart, xEnd, yEnd ?? */
void ST7789_SetWindow(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
    uint8_t col_data[4];
    uint8_t row_data[4];

    uint16_t colStart;
    uint16_t colEnd;
    uint16_t rowStart;
    uint16_t rowEnd;

    if (xStart > xEnd)
    {
        Swap(&xStart, &xEnd);
    }

    if (yStart > yEnd)
    {
        Swap(&yStart, &yEnd);
    }

    /* 240x320:x ?? 239,y ?? 319 */
    if (xStart > COL_MAX) xStart = COL_MAX;
    if (xEnd   > COL_MAX) xEnd   = COL_MAX;
    if (yStart > ROW_MAX) yStart = ROW_MAX;
    if (yEnd   > ROW_MAX) yEnd   = ROW_MAX;

    switch (g_rotation)
    {
        case DISPLAY_ROTATION_90:
            colStart = yStart;
            colEnd   = yEnd;
            rowStart = TFT_WIDTH - 1 - xEnd;
            rowEnd   = TFT_WIDTH - 1 - xStart;
            break;

        case DISPLAY_ROTATION_180:
            colStart = TFT_WIDTH - 1 - xEnd;
            colEnd   = TFT_WIDTH - 1 - xStart;
            rowStart = TFT_HEIGHT - 1 - yEnd;
            rowEnd   = TFT_HEIGHT - 1 - yStart;
            break;

        case DISPLAY_ROTATION_270:
            colStart = TFT_HEIGHT - 1 - yEnd;
            colEnd   = TFT_HEIGHT - 1 - yStart;
            rowStart = xStart;
            rowEnd   = xEnd;
            break;

        case DISPLAY_ROTATION_0:
        default:
            colStart = xStart;
            colEnd   = xEnd;
            rowStart = yStart + Y_OFFSET;
            rowEnd   = yEnd   + Y_OFFSET;
            break;
    }

    if (colStart > colEnd)
    {
        Swap(&colStart, &colEnd);
    }

    if (rowStart > rowEnd)
    {
        Swap(&rowStart, &rowEnd);
    }

    col_data[0] = colStart >> 8;
    col_data[1] = colStart & 0xFF;
    col_data[2] = colEnd >> 8;
    col_data[3] = colEnd & 0xFF;

    row_data[0] = rowStart >> 8;
    row_data[1] = rowStart & 0xFF;
    row_data[2] = rowEnd >> 8;
    row_data[3] = rowEnd & 0xFF;

    /* ??? */
    WriteCommand(0x2A);
    WriteMultiData(col_data, 4);

    /* ??? */
    WriteCommand(0x2B);
    WriteMultiData(row_data, 4);

    /* ??? */
    WriteCommand(0x2C);
}


/* ?? */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t buff[2];

    if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT))
    {
        return;
    }

    buff[0] = color >> 8;
    buff[1] = color & 0xFF;

    ST7789_SetWindow(x, y, x, y);
    WriteMultiData(buff, 2);
}


/* ??:Bresenham ?? */
void ST7789_DrawLine(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
    int16_t x0 = (int16_t)xStart;
    int16_t y0 = (int16_t)yStart;
    int16_t x1 = (int16_t)xEnd;
    int16_t y1 = (int16_t)yEnd;

    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);

    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;

    int16_t err = dx - dy;
    int16_t e2;

    while (1)
    {
        if ((x0 >= 0) && (x0 < TFT_WIDTH) && (y0 >= 0) && (y0 < TFT_HEIGHT))
        {
            ST7789_DrawPixel((uint16_t)x0, (uint16_t)y0, color);
        }

        if ((x0 == x1) && (y0 == y1))
        {
            break;
        }

        e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}


/* ???????? */
void ST7789_SetScreen(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
    uint8_t buff[2];
    uint32_t numPixels;
    uint32_t i;

    if (xStart > xEnd)
    {
        Swap(&xStart, &xEnd);
    }

    if (yStart > yEnd)
    {
        Swap(&yStart, &yEnd);
    }

    if (xStart >= TFT_WIDTH || yStart >= TFT_HEIGHT)
    {
        return;
    }

    if (xEnd >= TFT_WIDTH)
    {
        xEnd = TFT_WIDTH - 1;
    }

    if (yEnd >= TFT_HEIGHT)
    {
        yEnd = TFT_HEIGHT - 1;
    }

    buff[0] = color >> 8;
    buff[1] = color & 0xFF;

    ST7789_SetWindow(xStart, yStart, xEnd, yEnd);

    numPixels = (uint32_t)(xEnd - xStart + 1) * (uint32_t)(yEnd - yStart + 1);

    CS_SET_LOW();
    DC_SET_HIGH();

    for (i = 0; i < numPixels; i++)
    {
        HAL_SPI_Transmit(hspi, buff, 2, 100);
    }

    CS_SET_HIGH();
}


/* ???? */
void ST7789_FillScreen(uint16_t color)
{
    uint8_t buff[2];
    uint32_t i;

    buff[0] = color >> 8;
    buff[1] = color & 0xFF;

    /* ??:????? TFT_WIDTH - 1, TFT_HEIGHT - 1 */
    ST7789_SetWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    CS_SET_LOW();
    DC_SET_HIGH();

    for (i = 0; i < ((uint32_t)TFT_WIDTH * (uint32_t)TFT_HEIGHT); i++)
    {
        HAL_SPI_Transmit(hspi, buff, 2, 100);
    }

    CS_SET_HIGH();
}


/* ????????????,?? main.c ??????????????
   ?????????,??????????????? */

void ST7789_DrawChar(uint16_t x, uint16_t y, char c, CharSize size, uint16_t color, uint16_t bg_color)
{
    (void)x;
    (void)y;
    (void)c;
    (void)size;
    (void)color;
    (void)bg_color;
}

void ST7789_DrawString(uint16_t x, uint16_t y, const char *str, CharSize size, uint16_t color, uint16_t bg_color)
{
    (void)x;
    (void)y;
    (void)str;
    (void)size;
    (void)color;
    (void)bg_color;
}

void ST7789_DrawChinese24x24(uint16_t x, uint16_t y, uint16_t color, uint8_t count)
{
    (void)x;
    (void)y;
    (void)color;
    (void)count;
}

void ST7789_DrawChinese240x240(uint16_t x, uint16_t y, const uint8_t *font, uint16_t color, uint16_t bg_color)
{
    (void)x;
    (void)y;
    (void)font;
    (void)color;
    (void)bg_color;
}

void ST7789_DrawImage_40x40(uint16_t x, uint16_t y, const unsigned char *img)
{
    (void)x;
    (void)y;
    (void)img;
}

void ST7789_DrawImage_80x80(uint16_t x, uint16_t y, const unsigned char *img)
{
    (void)x;
    (void)y;
    (void)img;
}

void ST7789_DrawImage_240x240(uint16_t x, uint16_t y, const unsigned char *img, uint16_t color)
{
    (void)x;
    (void)y;
    (void)img;
    (void)color;
}

void ST7789_RotationDisplayTest(void)
{
    ST7789_FillScreen(COLOR_RED);
    HAL_Delay(500);

    ST7789_FillScreen(COLOR_GREEN);
    HAL_Delay(500);

    ST7789_FillScreen(COLOR_BLUE);
    HAL_Delay(500);

    ST7789_FillScreen(COLOR_BLACK);
    HAL_Delay(500);
}

void ST7789_DrawStringTest(void)
{
    ST7789_FillScreen(COLOR_BLACK);
}

void ST7789_DrawChineseTest(void)
{
    ST7789_FillScreen(COLOR_BLACK);
}

void ST7789_DrawImageTest(void)
{
    ST7789_FillScreen(COLOR_BLACK);
}