/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ST7789 240x320 icon display, PB3/PB4/PB5/PB6 control
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "st7789.h"


/* ?? main.h ??????,????? */
#ifndef GPIO_TEST_Pin
#define GPIO_TEST_Pin GPIO_PIN_13
#endif

#ifndef GPIO_TEST_GPIO_Port
#define GPIO_TEST_GPIO_Port GPIOC
#endif

#ifndef RST_Pin
#define RST_Pin GPIO_PIN_3
#endif

#ifndef RST_GPIO_Port
#define RST_GPIO_Port GPIOA
#endif

#ifndef CS_Pin
#define CS_Pin GPIO_PIN_4
#endif

#ifndef CS_GPIO_Port
#define CS_GPIO_Port GPIOA
#endif

#ifndef BLK_Pin
#define BLK_Pin GPIO_PIN_0
#endif

#ifndef BLK_GPIO_Port
#define BLK_GPIO_Port GPIOB
#endif

#ifndef DC_Pin
#define DC_Pin GPIO_PIN_1
#endif

#ifndef DC_GPIO_Port
#define DC_GPIO_Port GPIOB
#endif


/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

/* ?? DMA ??,????? MSP / IT ??????? */
DMA_HandleTypeDef hdma_spi1_tx;

osThreadId defaultTaskHandle;
osThreadId displayTaskHandle;


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);

void StartDefaultTask(void const * argument);
void StartDisplayTask(void const * argument);


/* USER CODE BEGIN 0 */

/* =========================
   ??
   ========================= */

/* 0:? 0.2 ?????,???????,??
   1:? 0.2 ????????? */
#define APP_FORCE_REFRESH     0

#define APP_REFRESH_MS        200


/* =========================
   ??????
   ========================= */
#define KEY_LEFT_PORT         GPIOB
#define KEY_LEFT_PIN          GPIO_PIN_3

#define KEY_FORWARD_PORT      GPIOB
#define KEY_FORWARD_PIN       GPIO_PIN_4

#define KEY_RIGHT_PORT        GPIOB
#define KEY_RIGHT_PIN         GPIO_PIN_5

#define KEY_WARNING_PORT      GPIOB
#define KEY_WARNING_PIN       GPIO_PIN_6


/* =========================
   RGB565 ??
   EVA ??:?? + ?? + ????
   ========================= */
#define APP_RGB565(r, g, b)   (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define APP_COLOR_BLACK       APP_RGB565(0,   0,   0)
#define APP_COLOR_RED         APP_RGB565(230, 0,   30)
#define APP_COLOR_DARK_RED    APP_RGB565(80,  0,   10)
#define APP_COLOR_WHITE       APP_RGB565(255, 255, 255)
#define APP_COLOR_GRAY        APP_RGB565(60,  60,  60)
#define APP_COLOR_DIM         APP_RGB565(25,  25,  25)


/* =========================
   ????
   ========================= */
typedef enum
{
    APP_STATE_NERVOUS = 0,
    APP_STATE_LEFT,
    APP_STATE_FORWARD,
    APP_STATE_RIGHT,
    APP_STATE_WARNING
} AppState_t;


/* =========================
   ??????
   ========================= */

static void APP_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT)
    {
        return;
    }

    if ((x + w) > TFT_WIDTH)
    {
        w = TFT_WIDTH - x;
    }

    if ((y + h) > TFT_HEIGHT)
    {
        h = TFT_HEIGHT - y;
    }

    if (w == 0 || h == 0)
    {
        return;
    }

    ST7789_SetScreen(x, y, x + w - 1, y + h - 1, color);
}


static void APP_FillHLine(int32_t x0, int32_t x1, int32_t y, uint16_t color)
{
    int32_t t;

    if (y < 0 || y >= TFT_HEIGHT)
    {
        return;
    }

    if (x0 > x1)
    {
        t = x0;
        x0 = x1;
        x1 = t;
    }

    if (x1 < 0 || x0 >= TFT_WIDTH)
    {
        return;
    }

    if (x0 < 0)
    {
        x0 = 0;
    }

    if (x1 >= TFT_WIDTH)
    {
        x1 = TFT_WIDTH - 1;
    }

    ST7789_SetScreen((uint16_t)x0, (uint16_t)y, (uint16_t)x1, (uint16_t)y, color);
}


static void APP_SwapI32(int32_t *a, int32_t *b)
{
    int32_t t;

    t = *a;
    *a = *b;
    *b = t;
}


static int32_t APP_InterpX(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t y)
{
    if (y1 == y0)
    {
        return x0;
    }

    return x0 + (x1 - x0) * (y - y0) / (y1 - y0);
}


static void APP_FillTriangle(int32_t x0, int32_t y0,
                             int32_t x1, int32_t y1,
                             int32_t x2, int32_t y2,
                             uint16_t color)
{
    int32_t y;
    int32_t xa;
    int32_t xb;

    if (y0 > y1)
    {
        APP_SwapI32(&y0, &y1);
        APP_SwapI32(&x0, &x1);
    }

    if (y1 > y2)
    {
        APP_SwapI32(&y1, &y2);
        APP_SwapI32(&x1, &x2);
    }

    if (y0 > y1)
    {
        APP_SwapI32(&y0, &y1);
        APP_SwapI32(&x0, &x1);
    }

    for (y = y0; y <= y2; y++)
    {
        if (y < y1)
        {
            xa = APP_InterpX(x0, y0, x1, y1, y);
            xb = APP_InterpX(x0, y0, x2, y2, y);
        }
        else
        {
            xa = APP_InterpX(x1, y1, x2, y2, y);
            xb = APP_InterpX(x0, y0, x2, y2, y);
        }

        APP_FillHLine(xa, xb, y, color);
    }
}


/* =========================
   5x7 ????
   ?????????
   ========================= */

static uint8_t APP_GetFont5x7(char c, uint8_t row)
{
    uint8_t v = 0x00;

    switch (c)
    {
        case 'A':
            {
                const uint8_t p[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11};
                v = p[row];
            }
            break;

        case 'D':
            {
                const uint8_t p[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E};
                v = p[row];
            }
            break;

        case 'E':
            {
                const uint8_t p[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
                v = p[row];
            }
            break;

        case 'F':
            {
                const uint8_t p[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10};
                v = p[row];
            }
            break;

        case 'G':
            {
                const uint8_t p[7] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E};
                v = p[row];
            }
            break;

        case 'H':
            {
                const uint8_t p[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11};
                v = p[row];
            }
            break;

        case 'I':
            {
                const uint8_t p[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F};
                v = p[row];
            }
            break;

        case 'L':
            {
                const uint8_t p[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F};
                v = p[row];
            }
            break;

        case 'N':
            {
                const uint8_t p[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11};
                v = p[row];
            }
            break;

        case 'O':
            {
                const uint8_t p[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
                v = p[row];
            }
            break;

        case 'R':
            {
                const uint8_t p[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11};
                v = p[row];
            }
            break;

        case 'S':
            {
                const uint8_t p[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E};
                v = p[row];
            }
            break;

        case 'T':
            {
                const uint8_t p[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
                v = p[row];
            }
            break;

        case 'U':
            {
                const uint8_t p[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};
                v = p[row];
            }
            break;

        case 'V':
            {
                const uint8_t p[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04};
                v = p[row];
            }
            break;

        case 'W':
            {
                const uint8_t p[7] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0A};
                v = p[row];
            }
            break;

        case 'o':
            {
                const uint8_t p[7] = {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E};
                v = p[row];
            }
            break;

        case 'u':
            {
                const uint8_t p[7] = {0x00,0x00,0x11,0x11,0x11,0x13,0x0D};
                v = p[row];
            }
            break;

        case 's':
            {
                const uint8_t p[7] = {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E};
                v = p[row];
            }
            break;

        case ' ':
        default:
            v = 0x00;
            break;
    }

    return v;
}


static uint16_t APP_TextWidth(const char *str, uint8_t scale)
{
    uint16_t len = 0;

    while (*str)
    {
        len++;
        str++;
    }

    if (len == 0)
    {
        return 0;
    }

    return (uint16_t)(len * 6 * scale - scale);
}


static void APP_DrawChar5x7(uint16_t x, uint16_t y, char c, uint8_t scale, uint16_t color)
{
    uint8_t row;
    uint8_t col;
    uint8_t bits;

    for (row = 0; row < 7; row++)
    {
        bits = APP_GetFont5x7(c, row);

        for (col = 0; col < 5; col++)
        {
            if (bits & (1 << (4 - col)))
            {
                APP_FillRect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}


static void APP_DrawText(uint16_t x, uint16_t y, const char *str, uint8_t scale, uint16_t color)
{
    while (*str)
    {
        APP_DrawChar5x7(x, y, *str, scale, color);
        x += 6 * scale;
        str++;
    }
}


static void APP_DrawTextCenter(uint16_t y, const char *str, uint8_t scale, uint16_t color)
{
    uint16_t w;
    uint16_t x;

    w = APP_TextWidth(str, scale);

    if (w >= TFT_WIDTH)
    {
        x = 0;
    }
    else
    {
        x = (TFT_WIDTH - w) / 2;
    }

    APP_DrawText(x, y, str, scale, color);
}


/* =========================
   EVA ????
   ========================= */

static void APP_DrawEvaFrame(void)
{
    /* ????? */
    APP_FillRect(0, 0, TFT_WIDTH, 5, APP_COLOR_DARK_RED);
    APP_FillRect(0, TFT_HEIGHT - 5, TFT_WIDTH, 5, APP_COLOR_DARK_RED);
    APP_FillRect(0, 0, 5, TFT_HEIGHT, APP_COLOR_DARK_RED);
    APP_FillRect(TFT_WIDTH - 5, 0, 5, TFT_HEIGHT, APP_COLOR_DARK_RED);

    /* ????? */
    APP_FillRect(10, 10, 35, 5, APP_COLOR_RED);
    APP_FillRect(10, 10, 5, 35, APP_COLOR_RED);

    APP_FillRect(TFT_WIDTH - 45, 10, 35, 5, APP_COLOR_RED);
    APP_FillRect(TFT_WIDTH - 15, 10, 5, 35, APP_COLOR_RED);

    APP_FillRect(10, TFT_HEIGHT - 15, 35, 5, APP_COLOR_RED);
    APP_FillRect(10, TFT_HEIGHT - 45, 5, 35, APP_COLOR_RED);

    APP_FillRect(TFT_WIDTH - 45, TFT_HEIGHT - 15, 35, 5, APP_COLOR_RED);
    APP_FillRect(TFT_WIDTH - 15, TFT_HEIGHT - 45, 5, 35, APP_COLOR_RED);
}


static void APP_ClearEvaScreen(void)
{
    ST7789_FillScreen(APP_COLOR_BLACK);
    APP_DrawEvaFrame();
}


/* =========================
   ??????
   ========================= */

static void APP_DrawLeftScreen(void)
{
    APP_ClearEvaScreen();

    APP_DrawTextCenter(28, "LEFT", 4, APP_COLOR_RED);

    /* ?????? */
    APP_FillTriangle(35, 170,
                     120, 85,
                     120, 255,
                     APP_COLOR_RED);

    APP_FillRect(115, 135, 85, 70, APP_COLOR_RED);

    /* ????,?? EVA ??? */
    APP_FillRect(125, 150, 60, 12, APP_COLOR_BLACK);
    APP_FillRect(125, 178, 60, 12, APP_COLOR_BLACK);

    /* ???? */
    ST7789_DrawLine(70, 170, 112, 128, APP_COLOR_WHITE);
    ST7789_DrawLine(70, 170, 112, 212, APP_COLOR_WHITE);
}


static void APP_DrawForwardScreen(void)
{
    APP_ClearEvaScreen();

    APP_DrawTextCenter(28, "FORWARD", 3, APP_COLOR_RED);

    /* ???? */
    APP_FillTriangle(120, 55,
                     40, 155,
                     200, 155,
                     APP_COLOR_RED);

    APP_FillRect(85, 150, 70, 120, APP_COLOR_RED);

    /* ???? */
    APP_FillRect(105, 160, 30, 95, APP_COLOR_BLACK);
    APP_FillRect(75, 170, 90, 10, APP_COLOR_BLACK);

    /* ???? */
    ST7789_DrawLine(120, 78, 72, 140, APP_COLOR_WHITE);
    ST7789_DrawLine(120, 78, 168, 140, APP_COLOR_WHITE);
}


static void APP_DrawRightScreen(void)
{
    APP_ClearEvaScreen();

    APP_DrawTextCenter(28, "RIGHT", 4, APP_COLOR_RED);

    /* ???? */
    APP_FillTriangle(205, 170,
                     120, 85,
                     120, 255,
                     APP_COLOR_RED);

    APP_FillRect(40, 135, 85, 70, APP_COLOR_RED);

    /* ???? */
    APP_FillRect(55, 150, 60, 12, APP_COLOR_BLACK);
    APP_FillRect(55, 178, 60, 12, APP_COLOR_BLACK);

    /* ???? */
    ST7789_DrawLine(170, 170, 128, 128, APP_COLOR_WHITE);
    ST7789_DrawLine(170, 170, 128, 212, APP_COLOR_WHITE);
}


static void APP_DrawWarningScreen(void)
{
    APP_ClearEvaScreen();

    APP_DrawTextCenter(28, "WARNING", 3, APP_COLOR_RED);

    /* ?????? */
    APP_FillTriangle(120, 65,
                     25, 265,
                     215, 265,
                     APP_COLOR_RED);

    /* ?????,???????? */
    APP_FillTriangle(120, 105,
                     65, 235,
                     175, 235,
                     APP_COLOR_BLACK);

    /* ????? */
    APP_FillRect(108, 130, 24, 70, APP_COLOR_RED);
    APP_FillRect(106, 215, 28, 28, APP_COLOR_RED);

    /* ????? */
    APP_FillRect(35, 285, 170, 8, APP_COLOR_RED);
}


static void APP_DrawNervousScreen(void)
{
    APP_ClearEvaScreen();

    /* ???? NERVous */
    APP_DrawTextCenter(115, "NERVous", 4, APP_COLOR_RED);

    /* ????? */
    APP_FillRect(40, 95, 160, 4, APP_COLOR_RED);
    APP_FillRect(40, 180, 160, 4, APP_COLOR_RED);

    /* ????? */
    APP_DrawTextCenter(220, "STAND BY", 2, APP_COLOR_DARK_RED);

    /* ???? */
    APP_FillRect(114, 252, 12, 12, APP_COLOR_RED);
}


/* =========================
   ???????
   ========================= */

static AppState_t APP_ReadStateFromPins(void)
{
    GPIO_PinState b3;
    GPIO_PinState b4;
    GPIO_PinState b5;
    GPIO_PinState b6;

    b3 = HAL_GPIO_ReadPin(KEY_LEFT_PORT, KEY_LEFT_PIN);
    b4 = HAL_GPIO_ReadPin(KEY_FORWARD_PORT, KEY_FORWARD_PIN);
    b5 = HAL_GPIO_ReadPin(KEY_RIGHT_PORT, KEY_RIGHT_PIN);
    b6 = HAL_GPIO_ReadPin(KEY_WARNING_PORT, KEY_WARNING_PIN);

    /*
       ???:
       ????,???????????
       ????? PB3 ?????,????????????
    */
    if (b6 == GPIO_PIN_SET)
    {
        return APP_STATE_WARNING;
    }
    else if (b3 == GPIO_PIN_SET)
    {
        return APP_STATE_LEFT;
    }
    else if (b4 == GPIO_PIN_SET)
    {
        return APP_STATE_FORWARD;
    }
    else if (b5 == GPIO_PIN_SET)
    {
        return APP_STATE_RIGHT;
    }
    else
    {
        return APP_STATE_NERVOUS;
    }
}


static void APP_ShowState(AppState_t state)
{
    switch (state)
    {
        case APP_STATE_LEFT:
            APP_DrawLeftScreen();
            break;

        case APP_STATE_FORWARD:
            APP_DrawForwardScreen();
            break;

        case APP_STATE_RIGHT:
            APP_DrawRightScreen();
            break;

        case APP_STATE_WARNING:
            APP_DrawWarningScreen();
            break;

        case APP_STATE_NERVOUS:
        default:
            APP_DrawNervousScreen();
            break;
    }
}

/* USER CODE END 0 */


int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI1_Init();

    HAL_GPIO_WritePin(BLK_GPIO_Port, BLK_Pin, GPIO_PIN_RESET);

    osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
    defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

    osThreadDef(displayTask, StartDisplayTask, osPriorityAboveNormal, 0, 512);
    displayTaskHandle = osThreadCreate(osThread(displayTask), NULL);

    osKernelStart();

    while (1)
    {
    }
}


/**
  * @brief System Clock Configuration
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK  |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1  |
                                  RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief DMA Initialization Function
  */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}


/**
  * @brief SPI1 Initialization Function
  */
static void MX_SPI1_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;

    /*
       ????? /4 ??????,?? /8?
       ??????,???? SPI_BAUDRATEPRESCALER_4?
    */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;

    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 10;

    if (HAL_SPI_Init(&hspi1) != HAL_OK)
    {
        Error_Handler();
    }
}


/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    /*
       ??:
       PB3 ? PB4 ??? JTAG ?????
       ???? JTAG,?? SWD ?????
       ?? PB3/PB4 ?????? GPIO ?????
    */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    HAL_GPIO_WritePin(GPIO_TEST_GPIO_Port, GPIO_TEST_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BLK_GPIO_Port, BLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET);

    /* PC13 ?? LED */
    GPIO_InitStruct.Pin = GPIO_TEST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIO_TEST_GPIO_Port, &GPIO_InitStruct);

    /* PA3 = RST */
    GPIO_InitStruct.Pin = RST_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RST_GPIO_Port, &GPIO_InitStruct);

    /* PA4 = CS */
    GPIO_InitStruct.Pin = CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

    /* PB0 = BLK */
    GPIO_InitStruct.Pin = BLK_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(BLK_GPIO_Port, &GPIO_InitStruct);

    /* PB1 = DC */
    GPIO_InitStruct.Pin = DC_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DC_GPIO_Port, &GPIO_InitStruct);

    /*
       PB3/PB4/PB5/PB6 ??:
       PB3 ?:??
       PB4 ?:??
       PB5 ?:??
       PB6 ?:??

       ????????,???????????
       ????????????????,????? GPIO_NOPULL?
    */
    GPIO_InitStruct.Pin = KEY_LEFT_PIN | KEY_FORWARD_PIN | KEY_RIGHT_PIN | KEY_WARNING_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}


/* USER CODE BEGIN 4 */

void StartDefaultTask(void const * argument)
{
    for (;;)
    {
        HAL_GPIO_TogglePin(GPIO_TEST_GPIO_Port, GPIO_TEST_Pin);
        osDelay(500);
    }
}


void StartDisplayTask(void const * argument)
{
    AppState_t current_state;
    AppState_t last_state;

    osDelay(200);

    HAL_GPIO_WritePin(BLK_GPIO_Port, BLK_Pin, GPIO_PIN_SET);

    osDelay(100);

    ST7789_Init();

    osDelay(100);

    ST7789_SetRotation(DISPLAY_ROTATION_0);
    ST7789_FillScreen(APP_COLOR_BLACK);

    current_state = APP_ReadStateFromPins();
    last_state = current_state;
    APP_ShowState(current_state);

    for (;;)
    {
        current_state = APP_ReadStateFromPins();

#if APP_FORCE_REFRESH
        APP_ShowState(current_state);
        last_state = current_state;
#else
        if (current_state != last_state)
        {
            last_state = current_state;
            APP_ShowState(current_state);
        }
#endif

        osDelay(APP_REFRESH_MS);
    }
}

/* USER CODE END 4 */


/**
  * @brief Period elapsed callback
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        HAL_IncTick();
    }
}


/**
  * @brief Error Handler
  */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
        HAL_GPIO_TogglePin(GPIO_TEST_GPIO_Port, GPIO_TEST_Pin);

        for (volatile uint32_t i = 0; i < 500000; i++)
        {
        }
    }
}


#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif