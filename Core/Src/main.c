/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "usb_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  APP_CMD_SHORT_PRESS = 1
} AppCommand_t;

typedef struct
{
  uint32_t ld3PeriodMs;
  uint32_t shortPressCount;
  uint32_t longPressCount;
  uint32_t touchCount;
  uint32_t fps;
  uint32_t heapFreeKb;
  uint32_t uptimeSec;
  uint16_t touchX;
  uint16_t touchY;
  uint8_t effectMode;
  uint8_t animationEnabled;
  uint8_t touchReady;
  uint8_t touchActive;
} AppStatus_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_WIDTH                     240U
#define LCD_HEIGHT                    320U
#define LCD_FRAMEBUFFER_ADDRESS       0xD0000000U
#define LCD_FRAMEBUFFER_PIXELS        (LCD_WIDTH * LCD_HEIGHT)
#define LCD_FRAMEBUFFER_BYTES         (LCD_FRAMEBUFFER_PIXELS * 2U)
#define LCD_BACKBUFFER_ADDRESS        (LCD_FRAMEBUFFER_ADDRESS + LCD_FRAMEBUFFER_BYTES)
#define SDRAM_TIMEOUT_MS              1000U
#define SDRAM_REFRESH_COUNT           542U
#define SDRAM_MODE_REGISTER           0x0230U
#define LCD_SPI_TIMEOUT_MS            100U
#define APP_EVENT_TOGGLE_DISPLAY      (1U << 0)
#define BUTTON_POLL_PERIOD_MS         20U
#define BUTTON_LONG_PRESS_MS          1000U
#define STATUS_BAR_HEIGHT             28U
#define TOUCH_BAR_HEIGHT              68U
#define EFFECT_TOP                    STATUS_BAR_HEIGHT
#define EFFECT_BOTTOM                 (LCD_HEIGHT - TOUCH_BAR_HEIGHT)
#define TOUCH_BUTTON_Y                (LCD_HEIGHT - TOUCH_BAR_HEIGHT + 8U)
#define TOUCH_BUTTON_HEIGHT           50U
#define TOUCH_BUTTON_WIDTH            76U
#define TOUCH_POLL_PERIOD_MS          50U
#define STMPE811_I2C_ADDRESS          0x82U
#define STMPE811_REG_SYS_CTRL1        0x03U
#define STMPE811_REG_SYS_CTRL2        0x04U
#define STMPE811_REG_INT_STA          0x0BU
#define STMPE811_REG_IO_AF            0x17U
#define STMPE811_REG_ADC_CTRL1        0x20U
#define STMPE811_REG_ADC_CTRL2        0x21U
#define STMPE811_REG_TSC_CTRL         0x40U
#define STMPE811_REG_TSC_CFG          0x41U
#define STMPE811_REG_FIFO_TH          0x4AU
#define STMPE811_REG_FIFO_STA         0x4BU
#define STMPE811_REG_FIFO_SIZE        0x4CU
#define STMPE811_REG_TSC_DATA_XYZ     0x52U
#define STMPE811_REG_TSC_FRACT_XYZ    0x56U
#define STMPE811_REG_TSC_I_DRIVE      0x58U
#define TOUCH_RAW_X_MIN               340U
#define TOUCH_RAW_X_MAX               3900U
#define TOUCH_RAW_Y_MIN               290U
#define TOUCH_RAW_Y_MAX               3800U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

DMA2D_HandleTypeDef hdma2d;

I2C_HandleTypeDef hi2c3;

LTDC_HandleTypeDef hltdc;

SPI_HandleTypeDef hspi5;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

SDRAM_HandleTypeDef hsdram1;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
osThreadId_t ledTaskHandle;
const osThreadAttr_t ledTask_attributes = {
  .name = "ledTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
osThreadId_t buttonTaskHandle;
const osThreadAttr_t buttonTask_attributes = {
  .name = "buttonTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
osThreadId_t statsTaskHandle;
const osThreadAttr_t statsTask_attributes = {
  .name = "statsTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
osThreadId_t touchTaskHandle;
const osThreadAttr_t touchTask_attributes = {
  .name = "touchTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
osMessageQueueId_t appCommandQueueHandle;
osMessageQueueId_t displayCommandQueueHandle;
osEventFlagsId_t appEventFlagsHandle;
osTimerId_t led4TimerHandle;
const osTimerAttr_t led4Timer_attributes = {
  .name = "led4Timer",
};
osMutexId_t displayMutexHandle;
const osMutexAttr_t displayMutex_attributes = {
  .name = "displayMutex",
};
osSemaphoreId_t buttonSemaphoreHandle;
const osSemaphoreAttr_t buttonSemaphore_attributes = {
  .name = "buttonSemaphore",
};
volatile AppStatus_t appStatus = {
  .ld3PeriodMs = 100,
  .shortPressCount = 0,
  .longPressCount = 0,
  .touchCount = 0,
  .fps = 0,
  .heapFreeKb = 0,
  .uptimeSec = 0,
  .touchX = 0,
  .touchY = 0,
  .effectMode = 0,
  .animationEnabled = 1,
  .touchReady = 0,
  .touchActive = 0,
};
static uint32_t appVisibleFramebufferAddress = LCD_FRAMEBUFFER_ADDRESS;
static uint32_t appDrawFramebufferAddress = LCD_BACKBUFFER_ADDRESS;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CRC_Init(void);
static void MX_DMA2D_Init(void);
static void MX_FMC_Init(void);
static void MX_I2C3_Init(void);
static void MX_LTDC_Init(void);
static void MX_SPI5_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */
void StartLedTask(void *argument);
void StartButtonTask(void *argument);
void StartDisplayTask(void *argument);
void StartStatsTask(void *argument);
void StartTouchTask(void *argument);
void Led4TimerCallback(void *argument);
static void App_SDRAM_Init(void);
static void App_LCD_Init(void);
static uint8_t App_Touch_Init(void);
static HAL_StatusTypeDef App_Touch_WriteReg(uint8_t reg, uint8_t value);
static HAL_StatusTypeDef App_Touch_ReadReg(uint8_t reg, uint8_t *value);
static HAL_StatusTypeDef App_Touch_ReadBuffer(uint8_t reg, uint8_t *data, uint16_t size);
static void App_Touch_FlushFifo(void);
static uint16_t App_Touch_ClampRaw(uint16_t value, uint16_t minValue, uint16_t maxValue);
static uint8_t App_Touch_ReadPoint(uint16_t *x, uint16_t *y);
static uint8_t App_Touch_GetButtonIndex(uint16_t x, uint16_t y);
static void App_Touch_HandlePress(uint16_t x, uint16_t y);
static void App_LCD_WriteCommand(uint8_t command);
static void App_LCD_WriteData(uint8_t data);
static void App_LCD_WriteDataBuffer(const uint8_t *data, uint16_t size);
static void App_FramebufferFill(uint16_t color);
static uint16_t App_Rgb565(uint8_t red, uint8_t green, uint8_t blue);
static uint16_t App_ColorLerp(uint16_t from, uint16_t to, uint8_t step, uint8_t maxStep);
static void App_FramebufferDrawHoverStripes(uint32_t phase);
static void App_FramebufferDrawWaterWave(uint32_t phase);
static const uint8_t *App_GetGlyph(char character);
static void App_FramebufferDrawRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint16_t color);
static void App_FramebufferDrawChar(uint32_t x, uint32_t y, char character, uint16_t color, uint16_t background);
static void App_FramebufferDrawText(uint32_t x, uint32_t y, const char *text, uint16_t color, uint16_t background);
static void App_FramebufferDrawNumber2(uint32_t x, uint32_t y, uint32_t value, uint16_t color, uint16_t background);
static void App_FramebufferDrawStatusBar(void);
static void App_FramebufferDrawTouchButton(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                           const char *label, uint16_t fill, uint16_t border, uint16_t text);
static void App_FramebufferDrawTouchControls(void);
static void App_FramebufferDrawTouchMarker(void);
static void App_LTDC_WaitForVerticalBlank(void);
static void App_DisplayPresent(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
1
   HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_CRC_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  App_SDRAM_Init();
  MX_I2C3_Init();
  MX_LTDC_Init();
  MX_SPI5_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  App_LCD_Init();
  appStatus.touchReady = App_Touch_Init();

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  displayMutexHandle = osMutexNew(&displayMutex_attributes);
  if (displayMutexHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  buttonSemaphoreHandle = osSemaphoreNew(1, 0, &buttonSemaphore_attributes);
  if (buttonSemaphoreHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  led4TimerHandle = osTimerNew(Led4TimerCallback, osTimerPeriodic, NULL, &led4Timer_attributes);
  if (led4TimerHandle == NULL)
  {
    Error_Handler();
  }
  if (osTimerStart(led4TimerHandle, 700) != osOK)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  appCommandQueueHandle = osMessageQueueNew(4, sizeof(AppCommand_t), NULL);
  if (appCommandQueueHandle == NULL)
  {
    Error_Handler();
  }
  displayCommandQueueHandle = osMessageQueueNew(4, sizeof(AppCommand_t), NULL);
  if (displayCommandQueueHandle == NULL)
  {
    Error_Handler();
  }
  appEventFlagsHandle = osEventFlagsNew(NULL);
  if (appEventFlagsHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  ledTaskHandle = osThreadNew(StartLedTask, NULL, &ledTask_attributes);
  buttonTaskHandle = osThreadNew(StartButtonTask, NULL, &buttonTask_attributes);
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);
  statsTaskHandle = osThreadNew(StartStatsTask, NULL, &statsTask_attributes);
  touchTaskHandle = osThreadNew(StartTouchTask, NULL, &touchTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
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
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief DMA2D Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA2D_Init(void)
{

  /* USER CODE BEGIN DMA2D_Init 0 */

  /* USER CODE END DMA2D_Init 0 */

  /* USER CODE BEGIN DMA2D_Init 1 */

  /* USER CODE END DMA2D_Init 1 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_ARGB8888;
  hdma2d.Init.OutputOffset = 0;
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMA2D_ConfigLayer(&hdma2d, 1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DMA2D_Init 2 */

  /* USER CODE END DMA2D_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief LTDC Initialization Function
  * @param None
  * @retval None
  */
static void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;
  hltdc.Init.HorizontalSync = 9;
  hltdc.Init.VerticalSync = 1;
  hltdc.Init.AccumulatedHBP = 29;
  hltdc.Init.AccumulatedVBP = 3;
  hltdc.Init.AccumulatedActiveW = 269;
  hltdc.Init.AccumulatedActiveH = 323;
  hltdc.Init.TotalWidth = 279;
  hltdc.Init.TotalHeigh = 327;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 240;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 320;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
  pLayerCfg.FBStartAdress = 0xD0000000;
  pLayerCfg.ImageWidth = 240;
  pLayerCfg.ImageHeight = 320;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */

  /* USER CODE END LTDC_Init 2 */

}

/**
  * @brief SPI5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI5_Init(void)
{

  /* USER CODE BEGIN SPI5_Init 0 */

  /* USER CODE END SPI5_Init 0 */

  /* USER CODE BEGIN SPI5_Init 1 */

  /* USER CODE END SPI5_Init 1 */
  /* SPI5 parameter configuration*/
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_MASTER;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES;
  hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;
  hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI5_Init 2 */

  /* USER CODE END SPI5_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_SDRAM_TimingTypeDef SdramTiming = {0};

  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SDRAM1 memory initialization sequence
  */
  hsdram1.Instance = FMC_SDRAM_DEVICE;
  /* hsdram1.Init */
  hsdram1.Init.SDBank = FMC_SDRAM_BANK2;
  hsdram1.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_8;
  hsdram1.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;
  hsdram1.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_16;
  hsdram1.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;
  hsdram1.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;
  hsdram1.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  hsdram1.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;
  hsdram1.Init.ReadBurst = FMC_SDRAM_RBURST_DISABLE;
  hsdram1.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_1;
  /* SdramTiming */
  SdramTiming.LoadToActiveDelay = 2;
  SdramTiming.ExitSelfRefreshDelay = 7;
  SdramTiming.SelfRefreshTime = 4;
  SdramTiming.RowCycleDelay = 7;
  SdramTiming.WriteRecoveryTime = 3;
  SdramTiming.RPDelay = 2;
  SdramTiming.RCDDelay = 2;

  if (HAL_SDRAM_Init(&hsdram1, &SdramTiming) != HAL_OK)
  {
    Error_Handler( );
  }

  /* USER CODE BEGIN FMC_Init 2 */

  /* USER CODE END FMC_Init 2 */
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, NCS_MEMS_SPI_Pin|CSX_Pin|OTG_FS_PSO_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ACP_RST_GPIO_Port, ACP_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, RDX_Pin|WRX_DCX_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, LD3_Pin|LD4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : NCS_MEMS_SPI_Pin CSX_Pin OTG_FS_PSO_Pin */
  GPIO_InitStruct.Pin = NCS_MEMS_SPI_Pin|CSX_Pin|OTG_FS_PSO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MEMS_INT1_Pin MEMS_INT2_Pin TP_INT1_Pin */
  GPIO_InitStruct.Pin = MEMS_INT1_Pin|MEMS_INT2_Pin|TP_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ACP_RST_Pin */
  GPIO_InitStruct.Pin = ACP_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ACP_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OC_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT1_Pin */
  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TE_Pin */
  GPIO_InitStruct.Pin = TE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RDX_Pin WRX_DCX_Pin */
  GPIO_InitStruct.Pin = RDX_Pin|WRX_DCX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : LD3_Pin LD4_Pin */
  GPIO_InitStruct.Pin = LD3_Pin|LD4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void App_LCD_WriteCommand(uint8_t command)
{
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(WRX_DCX_GPIO_Port, WRX_DCX_Pin, GPIO_PIN_RESET);
  if (HAL_SPI_Transmit(&hspi5, &command, 1, LCD_SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);
}

static void App_LCD_WriteData(uint8_t data)
{
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(WRX_DCX_GPIO_Port, WRX_DCX_Pin, GPIO_PIN_SET);
  if (HAL_SPI_Transmit(&hspi5, &data, 1, LCD_SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);
}

static void App_LCD_WriteDataBuffer(const uint8_t *data, uint16_t size)
{
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(WRX_DCX_GPIO_Port, WRX_DCX_Pin, GPIO_PIN_SET);
  if (HAL_SPI_Transmit(&hspi5, (uint8_t *)data, size, LCD_SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);
}

static void App_LCD_Init(void)
{
  static const uint8_t powerB[] = {0x00, 0xC1, 0x30};
  static const uint8_t powerSeq[] = {0x64, 0x03, 0x12, 0x81};
  static const uint8_t dtca[] = {0x85, 0x00, 0x78};
  static const uint8_t powerA[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
  static const uint8_t dtcb[] = {0x00, 0x00};
  static const uint8_t frameControl[] = {0x00, 0x1B};
  static const uint8_t displayFunction[] = {0x0A, 0xA2};
  static const uint8_t displayFunctionRgb[] = {0x0A, 0xA7, 0x27, 0x04};
  static const uint8_t columnAddress[] = {0x00, 0x00, 0x00, 0xEF};
  static const uint8_t pageAddress[] = {0x00, 0x00, 0x01, 0x3F};
  static const uint8_t interfaceControl[] = {0x01, 0x00, 0x06};
  static const uint8_t positiveGamma[] = {
    0x0F, 0x29, 0x24, 0x0C, 0x0E,
    0x09, 0x4E, 0x78, 0x3C, 0x09,
    0x13, 0x05, 0x17, 0x11, 0x00
  };
  static const uint8_t negativeGamma[] = {
    0x00, 0x16, 0x1B, 0x04, 0x11,
    0x07, 0x31, 0x33, 0x42, 0x05,
    0x0C, 0x0A, 0x28, 0x2F, 0x0F
  };

  HAL_GPIO_WritePin(CSX_GPIO_Port, CSX_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(RDX_GPIO_Port, RDX_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(WRX_DCX_GPIO_Port, WRX_DCX_Pin, GPIO_PIN_SET);

  App_LCD_WriteCommand(0x01);
  HAL_Delay(10);

  App_LCD_WriteCommand(0xCA);
  App_LCD_WriteData(0xC3);
  App_LCD_WriteData(0x08);
  App_LCD_WriteData(0x50);

  App_LCD_WriteCommand(0xCF);
  App_LCD_WriteDataBuffer(powerB, sizeof(powerB));

  App_LCD_WriteCommand(0xED);
  App_LCD_WriteDataBuffer(powerSeq, sizeof(powerSeq));

  App_LCD_WriteCommand(0xE8);
  App_LCD_WriteDataBuffer(dtca, sizeof(dtca));

  App_LCD_WriteCommand(0xCB);
  App_LCD_WriteDataBuffer(powerA, sizeof(powerA));

  App_LCD_WriteCommand(0xF7);
  App_LCD_WriteData(0x20);

  App_LCD_WriteCommand(0xEA);
  App_LCD_WriteDataBuffer(dtcb, sizeof(dtcb));

  App_LCD_WriteCommand(0xB1);
  App_LCD_WriteDataBuffer(frameControl, sizeof(frameControl));

  App_LCD_WriteCommand(0xB6);
  App_LCD_WriteDataBuffer(displayFunction, sizeof(displayFunction));

  App_LCD_WriteCommand(0xC0);
  App_LCD_WriteData(0x10);

  App_LCD_WriteCommand(0xC1);
  App_LCD_WriteData(0x10);

  App_LCD_WriteCommand(0xC5);
  App_LCD_WriteData(0x45);
  App_LCD_WriteData(0x15);

  App_LCD_WriteCommand(0xC7);
  App_LCD_WriteData(0x90);

  App_LCD_WriteCommand(0x36);
  App_LCD_WriteData(0xC8);

  App_LCD_WriteCommand(0x3A);
  App_LCD_WriteData(0x55);

  App_LCD_WriteCommand(0xF2);
  App_LCD_WriteData(0x00);

  App_LCD_WriteCommand(0xB0);
  App_LCD_WriteData(0xC2);

  App_LCD_WriteCommand(0xB6);
  App_LCD_WriteDataBuffer(displayFunctionRgb, sizeof(displayFunctionRgb));

  App_LCD_WriteCommand(0x2A);
  App_LCD_WriteDataBuffer(columnAddress, sizeof(columnAddress));

  App_LCD_WriteCommand(0x2B);
  App_LCD_WriteDataBuffer(pageAddress, sizeof(pageAddress));

  App_LCD_WriteCommand(0xF6);
  App_LCD_WriteDataBuffer(interfaceControl, sizeof(interfaceControl));

  App_LCD_WriteCommand(0x26);
  App_LCD_WriteData(0x01);

  App_LCD_WriteCommand(0xE0);
  App_LCD_WriteDataBuffer(positiveGamma, sizeof(positiveGamma));

  App_LCD_WriteCommand(0xE1);
  App_LCD_WriteDataBuffer(negativeGamma, sizeof(negativeGamma));

  App_LCD_WriteCommand(0x11);
  HAL_Delay(120);

  App_LCD_WriteCommand(0x29);
  HAL_Delay(20);

  App_LCD_WriteCommand(0x2C);
}

static void App_SDRAM_Init(void)
{
  FMC_SDRAM_CommandTypeDef command = {0};

  command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, SDRAM_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_Delay(1);

  command.CommandMode = FMC_SDRAM_CMD_PALL;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, SDRAM_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }

  command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
  command.AutoRefreshNumber = 4;
  command.ModeRegisterDefinition = 0;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, SDRAM_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }

  command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  command.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK2;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = SDRAM_MODE_REGISTER;
  if (HAL_SDRAM_SendCommand(&hsdram1, &command, SDRAM_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_SDRAM_ProgramRefreshRate(&hsdram1, SDRAM_REFRESH_COUNT) != HAL_OK)
  {
    Error_Handler();
  }
}

static HAL_StatusTypeDef App_Touch_WriteReg(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c3,
                           STMPE811_I2C_ADDRESS,
                           reg,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1,
                           LCD_SPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef App_Touch_ReadReg(uint8_t reg, uint8_t *value)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          STMPE811_I2C_ADDRESS,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          value,
                          1,
                          LCD_SPI_TIMEOUT_MS);
}

static HAL_StatusTypeDef App_Touch_ReadBuffer(uint8_t reg, uint8_t *data, uint16_t size)
{
  return HAL_I2C_Mem_Read(&hi2c3,
                          STMPE811_I2C_ADDRESS,
                          reg,
                          I2C_MEMADD_SIZE_8BIT,
                          data,
                          size,
                          LCD_SPI_TIMEOUT_MS);
}

static void App_Touch_FlushFifo(void)
{
  (void)App_Touch_WriteReg(STMPE811_REG_FIFO_STA, 0x01U);
  (void)App_Touch_WriteReg(STMPE811_REG_FIFO_STA, 0x00U);
  (void)App_Touch_WriteReg(STMPE811_REG_INT_STA, 0xFFU);
}

static uint8_t App_Touch_Init(void)
{
  if (HAL_I2C_IsDeviceReady(&hi2c3, STMPE811_I2C_ADDRESS, 3, LCD_SPI_TIMEOUT_MS) != HAL_OK)
  {
    return 0U;
  }

  if (App_Touch_WriteReg(STMPE811_REG_SYS_CTRL1, 0x02U) != HAL_OK)
  {
    return 0U;
  }
  HAL_Delay(10);

  if (App_Touch_WriteReg(STMPE811_REG_SYS_CTRL1, 0x00U) != HAL_OK)
  {
    return 0U;
  }
  HAL_Delay(2);

  if (App_Touch_WriteReg(STMPE811_REG_SYS_CTRL2, 0x00U) != HAL_OK)
  {
    return 0U;
  }

  (void)App_Touch_WriteReg(STMPE811_REG_IO_AF, 0x00U);
  (void)App_Touch_WriteReg(STMPE811_REG_ADC_CTRL1, 0x49U);
  HAL_Delay(2);
  (void)App_Touch_WriteReg(STMPE811_REG_ADC_CTRL2, 0x01U);
  (void)App_Touch_WriteReg(STMPE811_REG_TSC_CFG, 0x9AU);
  (void)App_Touch_WriteReg(STMPE811_REG_FIFO_TH, 0x01U);
  App_Touch_FlushFifo();
  (void)App_Touch_WriteReg(STMPE811_REG_TSC_FRACT_XYZ, 0x01U);
  (void)App_Touch_WriteReg(STMPE811_REG_TSC_I_DRIVE, 0x01U);

  if (App_Touch_WriteReg(STMPE811_REG_TSC_CTRL, 0x01U) != HAL_OK)
  {
    return 0U;
  }

  App_Touch_FlushFifo();
  return 1U;
}

static uint16_t App_Touch_ClampRaw(uint16_t value, uint16_t minValue, uint16_t maxValue)
{
  if (value < minValue)
  {
    return minValue;
  }

  if (value > maxValue)
  {
    return maxValue;
  }

  return value;
}

static uint8_t App_Touch_ReadPoint(uint16_t *x, uint16_t *y)
{
  uint8_t control = 0;
  uint8_t fifoSize = 0;
  uint8_t data[4] = {0};

  if (App_Touch_ReadReg(STMPE811_REG_TSC_CTRL, &control) != HAL_OK)
  {
    appStatus.touchReady = 0U;
    return 0U;
  }

  if ((control & 0x80U) == 0U)
  {
    App_Touch_FlushFifo();
    return 0U;
  }

  if (App_Touch_ReadReg(STMPE811_REG_FIFO_SIZE, &fifoSize) != HAL_OK)
  {
    appStatus.touchReady = 0U;
    return 0U;
  }

  if (fifoSize == 0U)
  {
    return 0U;
  }

  if (App_Touch_ReadBuffer(STMPE811_REG_TSC_DATA_XYZ, data, sizeof(data)) != HAL_OK)
  {
    appStatus.touchReady = 0U;
    return 0U;
  }

  uint16_t rawX = (uint16_t)(((uint16_t)data[0] << 4) | (data[1] >> 4));
  uint16_t rawY = (uint16_t)((((uint16_t)data[1] & 0x0FU) << 8) | data[2]);
  rawX = App_Touch_ClampRaw(rawX, TOUCH_RAW_X_MIN, TOUCH_RAW_X_MAX);
  rawY = App_Touch_ClampRaw(rawY, TOUCH_RAW_Y_MIN, TOUCH_RAW_Y_MAX);

  *x = (uint16_t)(LCD_WIDTH - 1U -
                  (((uint32_t)(rawX - TOUCH_RAW_X_MIN) * (LCD_WIDTH - 1U)) /
                   (TOUCH_RAW_X_MAX - TOUCH_RAW_X_MIN)));
  *y = (uint16_t)(((uint32_t)(rawY - TOUCH_RAW_Y_MIN) * (LCD_HEIGHT - 1U)) /
                  (TOUCH_RAW_Y_MAX - TOUCH_RAW_Y_MIN));

  App_Touch_FlushFifo();
  return 1U;
}

static uint8_t App_Touch_GetButtonIndex(uint16_t x, uint16_t y)
{
  uint16_t candidatesX[2] = {
    x,
    (uint16_t)(LCD_WIDTH - 1U - x)
  };
  uint16_t candidatesY[2] = {
    y,
    (uint16_t)(LCD_HEIGHT - 1U - y)
  };

  for (uint32_t yIndex = 0; yIndex < 2U; yIndex++)
  {
    uint16_t testY = candidatesY[yIndex];

    if ((testY < TOUCH_BUTTON_Y) || (testY >= (TOUCH_BUTTON_Y + TOUCH_BUTTON_HEIGHT)))
    {
      continue;
    }

    for (uint32_t xIndex = 0; xIndex < 2U; xIndex++)
    {
      uint16_t testX = candidatesX[xIndex];

      if ((testX >= 3U) && (testX < (3U + TOUCH_BUTTON_WIDTH)))
      {
        return 1U;
      }

      if ((testX >= 82U) && (testX < (82U + TOUCH_BUTTON_WIDTH)))
      {
        return 2U;
      }

      if ((testX >= 161U) && (testX < (161U + TOUCH_BUTTON_WIDTH)))
      {
        return 3U;
      }
    }
  }

  return 0U;
}

static void App_Touch_HandlePress(uint16_t x, uint16_t y)
{
  AppCommand_t command = APP_CMD_SHORT_PRESS;
  uint8_t buttonIndex = App_Touch_GetButtonIndex(x, y);

  appStatus.touchCount++;

  if (buttonIndex == 0U)
  {
    return;
  }

  if (buttonIndex == 1U)
  {
    (void)osMessageQueuePut(displayCommandQueueHandle, &command, 0, 0);
  }
  else if (buttonIndex == 2U)
  {
    (void)osEventFlagsSet(appEventFlagsHandle, APP_EVENT_TOGGLE_DISPLAY);
  }
  else
  {
    (void)osMessageQueuePut(appCommandQueueHandle, &command, 0, 0);
  }
}

static void App_FramebufferFill(uint16_t color)
{
  volatile uint16_t *framebuffer = (volatile uint16_t *)appDrawFramebufferAddress;

  for (uint32_t index = 0; index < LCD_FRAMEBUFFER_PIXELS; index++)
  {
    framebuffer[index] = color;
  }
}

static uint16_t App_Rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
  return (uint16_t)(((red & 0xF8U) << 8) |
                    ((green & 0xFCU) << 3) |
                    (blue >> 3));
}

static uint16_t App_ColorLerp(uint16_t from, uint16_t to, uint8_t step, uint8_t maxStep)
{
  int32_t fromRed = (from >> 11) & 0x1F;
  int32_t fromGreen = (from >> 5) & 0x3F;
  int32_t fromBlue = from & 0x1F;
  int32_t toRed = (to >> 11) & 0x1F;
  int32_t toGreen = (to >> 5) & 0x3F;
  int32_t toBlue = to & 0x1F;
  uint16_t red = (uint16_t)(fromRed + (((toRed - fromRed) * step) / maxStep));
  uint16_t green = (uint16_t)(fromGreen + (((toGreen - fromGreen) * step) / maxStep));
  uint16_t blue = (uint16_t)(fromBlue + (((toBlue - fromBlue) * step) / maxStep));

  return (uint16_t)((red << 11) | (green << 5) | blue);
}

static const uint8_t *App_GetGlyph(char character)
{
  static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t digits[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}
  };
  static const uint8_t letters[26][7] = {
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
  };

  if ((character >= '0') && (character <= '9'))
  {
    return digits[character - '0'];
  }

  if ((character >= 'a') && (character <= 'z'))
  {
    character = (char)(character - 'a' + 'A');
  }

  if ((character >= 'A') && (character <= 'Z'))
  {
    return letters[character - 'A'];
  }

  return blank;
}

static void App_FramebufferDrawRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint16_t color)
{
  volatile uint16_t *framebuffer = (volatile uint16_t *)appDrawFramebufferAddress;
  uint32_t xEnd = (x + width > LCD_WIDTH) ? LCD_WIDTH : (x + width);
  uint32_t yEnd = (y + height > LCD_HEIGHT) ? LCD_HEIGHT : (y + height);

  for (uint32_t row = y; row < yEnd; row++)
  {
    for (uint32_t col = x; col < xEnd; col++)
    {
      framebuffer[(row * LCD_WIDTH) + col] = color;
    }
  }
}

static void App_FramebufferDrawChar(uint32_t x, uint32_t y, char character, uint16_t color, uint16_t background)
{
  volatile uint16_t *framebuffer = (volatile uint16_t *)appDrawFramebufferAddress;
  const uint8_t *glyph = App_GetGlyph(character);

  for (uint32_t row = 0; row < 7U; row++)
  {
    for (uint32_t col = 0; col < 5U; col++)
    {
      uint16_t pixel = ((glyph[row] & (1U << (4U - col))) != 0U) ? color : background;
      uint32_t px = x + col;
      uint32_t py = y + row;

      if ((px < LCD_WIDTH) && (py < LCD_HEIGHT))
      {
        framebuffer[(py * LCD_WIDTH) + px] = pixel;
      }
    }
  }
}

static void App_FramebufferDrawText(uint32_t x, uint32_t y, const char *text, uint16_t color, uint16_t background)
{
  uint32_t cursor = x;

  while (*text != '\0')
  {
    App_FramebufferDrawChar(cursor, y, *text, color, background);
    cursor += 6U;
    text++;
  }
}

static void App_FramebufferDrawNumber2(uint32_t x, uint32_t y, uint32_t value, uint16_t color, uint16_t background)
{
  value %= 100U;
  App_FramebufferDrawChar(x, y, (char)('0' + (value / 10U)), color, background);
  App_FramebufferDrawChar(x + 6U, y, (char)('0' + (value % 10U)), color, background);
}

static void App_FramebufferDrawStatusBar(void)
{
  uint16_t background = App_Rgb565(6, 10, 18);
  uint16_t textColor = App_Rgb565(230, 245, 255);
  uint16_t accent = (appStatus.effectMode == 0U) ? App_Rgb565(40, 210, 240) : App_Rgb565(250, 190, 40);

  App_FramebufferDrawRect(0, 0, LCD_WIDTH, STATUS_BAR_HEIGHT, background);
  App_FramebufferDrawRect(0, STATUS_BAR_HEIGHT - 2U, LCD_WIDTH, 2U, accent);
  App_FramebufferDrawText(4, 4, (appStatus.effectMode == 0U) ? "FX WAVE" : "FX STRIP", accent, background);
  App_FramebufferDrawText(132, 4, "H", textColor, background);
  App_FramebufferDrawNumber2(144, 4, appStatus.heapFreeKb, textColor, background);
  App_FramebufferDrawText(158, 4, "K", textColor, background);
  App_FramebufferDrawText(184, 4, "U", textColor, background);
  App_FramebufferDrawNumber2(196, 4, appStatus.uptimeSec, textColor, background);
  App_FramebufferDrawText(4, 15, (appStatus.animationEnabled != 0U) ? "RUN" : "PAUSE", textColor, background);
  App_FramebufferDrawText(70, 15, (appStatus.ld3PeriodMs == 100U) ? "LD FAST" : "LD SLOW", textColor, background);
  App_FramebufferDrawText(136, 15, "S", textColor, background);
  App_FramebufferDrawNumber2(148, 15, appStatus.shortPressCount, textColor, background);
  App_FramebufferDrawText(172, 15, "L", textColor, background);
  App_FramebufferDrawNumber2(184, 15, appStatus.longPressCount, textColor, background);
  App_FramebufferDrawText(208, 15, "F", textColor, background);
  App_FramebufferDrawNumber2(220, 15, appStatus.fps, textColor, background);
}

static void App_FramebufferDrawTouchButton(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                           const char *label, uint16_t fill, uint16_t border, uint16_t text)
{
  App_FramebufferDrawRect(x, y, width, height, fill);
  App_FramebufferDrawRect(x, y, width, 2U, border);
  App_FramebufferDrawRect(x, y + height - 2U, width, 2U, border);
  App_FramebufferDrawRect(x, y, 2U, height, border);
  App_FramebufferDrawRect(x + width - 2U, y, 2U, height, border);
  App_FramebufferDrawText(x + 10U, y + 15U, label, text, fill);
}

static void App_FramebufferDrawTouchControls(void)
{
  uint16_t panel = App_Rgb565(8, 14, 24);
  uint16_t text = App_Rgb565(235, 250, 255);
  uint16_t fxColor = (appStatus.effectMode == 0U) ? App_Rgb565(0, 96, 150) : App_Rgb565(135, 76, 0);
  uint16_t runColor = (appStatus.animationEnabled != 0U) ? App_Rgb565(0, 95, 55) : App_Rgb565(120, 36, 36);
  uint16_t ledColor = (appStatus.ld3PeriodMs == 100U) ? App_Rgb565(80, 70, 0) : App_Rgb565(48, 48, 62);
  uint16_t activeBorder = App_Rgb565(255, 255, 255);
  uint16_t fxBorder = App_Rgb565(120, 230, 255);
  uint16_t runBorder = App_Rgb565(190, 240, 210);
  uint16_t ledBorder = App_Rgb565(245, 220, 120);

  if (appStatus.touchActive != 0U)
  {
    uint8_t buttonIndex = App_Touch_GetButtonIndex(appStatus.touchX, appStatus.touchY);

    if (buttonIndex == 1U)
    {
      fxBorder = activeBorder;
    }
    else if (buttonIndex == 2U)
    {
      runBorder = activeBorder;
    }
    else if (buttonIndex == 3U)
    {
      ledBorder = activeBorder;
    }
  }

  App_FramebufferDrawRect(0, LCD_HEIGHT - TOUCH_BAR_HEIGHT, LCD_WIDTH, TOUCH_BAR_HEIGHT, panel);
  App_FramebufferDrawRect(0, LCD_HEIGHT - TOUCH_BAR_HEIGHT, LCD_WIDTH, 2U, App_Rgb565(58, 78, 98));
  App_FramebufferDrawText(108, LCD_HEIGHT - TOUCH_BAR_HEIGHT + 1U, "T",
                          (appStatus.touchReady != 0U) ? App_Rgb565(120, 240, 150) : App_Rgb565(240, 80, 80),
                          panel);
  App_FramebufferDrawNumber2(120, LCD_HEIGHT - TOUCH_BAR_HEIGHT + 1U, appStatus.touchCount, text, panel);
  App_FramebufferDrawTouchButton(3, TOUCH_BUTTON_Y, TOUCH_BUTTON_WIDTH, TOUCH_BUTTON_HEIGHT,
                                 (appStatus.effectMode == 0U) ? "FX WAV" : "FX STR",
                                 fxColor, fxBorder, text);
  App_FramebufferDrawTouchButton(82, TOUCH_BUTTON_Y, TOUCH_BUTTON_WIDTH, TOUCH_BUTTON_HEIGHT,
                                 (appStatus.animationEnabled != 0U) ? "PAUSE" : "PLAY",
                                 runColor, runBorder, text);
  App_FramebufferDrawTouchButton(161, TOUCH_BUTTON_Y, TOUCH_BUTTON_WIDTH, TOUCH_BUTTON_HEIGHT,
                                 (appStatus.ld3PeriodMs == 100U) ? "LD SLO" : "LD FST",
                                 ledColor, ledBorder, text);
}

static void App_FramebufferDrawTouchMarker(void)
{
  if ((appStatus.touchReady == 0U) || (appStatus.touchActive == 0U))
  {
    return;
  }

  uint32_t markerX = (appStatus.touchX > 3U) ? (uint32_t)appStatus.touchX - 3U : 0U;
  uint32_t markerY = (appStatus.touchY > 3U) ? (uint32_t)appStatus.touchY - 3U : 0U;
  App_FramebufferDrawRect(markerX, markerY, 7U, 7U, App_Rgb565(255, 255, 255));
}

static void App_FramebufferDrawHoverStripes(uint32_t phase)
{
  volatile uint16_t *framebuffer = (volatile uint16_t *)appDrawFramebufferAddress;
  const uint16_t palette[] = {
    App_Rgb565(235, 36, 40),
    App_Rgb565(238, 58, 36),
    App_Rgb565(248, 214, 38),
    App_Rgb565(255, 236, 58),
    App_Rgb565(190, 222, 45),
    App_Rgb565(50, 190, 80),
    App_Rgb565(30, 190, 210),
    App_Rgb565(45, 95, 230),
    App_Rgb565(105, 70, 220),
    App_Rgb565(165, 70, 205),
    App_Rgb565(218, 58, 130),
    App_Rgb565(222, 42, 82),
    App_Rgb565(235, 36, 40)
  };
  const uint32_t stripeWidth = 28U;
  const uint32_t paletteCount = sizeof(palette) / sizeof(palette[0]);

  for (uint32_t y = EFFECT_TOP; y < EFFECT_BOTTOM; y++)
  {
    uint32_t localY = y - EFFECT_TOP;
    uint32_t rowGlow = (localY + phase) % 64U;
    uint8_t glowStep = (rowGlow < 32U) ? (uint8_t)rowGlow : (uint8_t)(63U - rowGlow);

    for (uint32_t x = 0; x < LCD_WIDTH; x++)
    {
      uint32_t shiftedX = x + phase;
      uint32_t baseIndex = (shiftedX / stripeWidth) % paletteCount;
      uint32_t nextIndex = (baseIndex + 1U) % paletteCount;
      uint8_t localStep = (uint8_t)(shiftedX % stripeWidth);
      uint16_t baseColor = App_ColorLerp(palette[baseIndex], palette[nextIndex], localStep, stripeWidth - 1U);

      framebuffer[(y * LCD_WIDTH) + x] = App_ColorLerp(baseColor, 0xFFFF, glowStep, 96U);
    }
  }
}

static void App_FramebufferDrawWaterWave(uint32_t phase)
{
  volatile uint16_t *framebuffer = (volatile uint16_t *)appDrawFramebufferAddress;
  const uint16_t deepBlue = App_Rgb565(0, 18, 64);
  const uint16_t blue = App_Rgb565(0, 88, 180);
  const uint16_t cyan = App_Rgb565(28, 210, 235);
  const uint16_t foam = App_Rgb565(210, 255, 255);

  for (uint32_t y = EFFECT_TOP; y < EFFECT_BOTTOM; y++)
  {
    uint32_t localY = y - EFFECT_TOP;
    int32_t rowWave = (int32_t)(((localY * 5U) + phase) % 64U);
    if (rowWave > 31)
    {
      rowWave = 63 - rowWave;
    }

    int32_t rowShift = rowWave - 16;

    for (uint32_t x = 0; x < LCD_WIDTH; x++)
    {
      uint32_t waveX = (uint32_t)((int32_t)x + rowShift + (int32_t)phase + 512) % 96U;
      uint32_t waveY = ((localY * 2U) + phase) % 96U;
      uint32_t mixed = (waveX + waveY) % 96U;
      uint16_t pixel;

      if (mixed < 32U)
      {
        pixel = App_ColorLerp(deepBlue, blue, (uint8_t)mixed, 31U);
      }
      else if (mixed < 64U)
      {
        pixel = App_ColorLerp(blue, cyan, (uint8_t)(mixed - 32U), 31U);
      }
      else
      {
        pixel = App_ColorLerp(cyan, deepBlue, (uint8_t)(mixed - 64U), 31U);
      }

      if (((waveX + (phase / 2U)) % 48U) < 3U)
      {
        pixel = App_ColorLerp(pixel, foam, 20U, 31U);
      }

      framebuffer[(y * LCD_WIDTH) + x] = pixel;
    }
  }
}

void StartLedTask(void *argument)
{
  uint32_t ledPeriodMs = 100;
  AppCommand_t command;

  for (;;)
  {
    if (osMessageQueueGet(appCommandQueueHandle, &command, NULL, 0) == osOK)
    {
      if (command == APP_CMD_SHORT_PRESS)
      {
        ledPeriodMs = (ledPeriodMs == 100) ? 500 : 100;
        appStatus.ld3PeriodMs = ledPeriodMs;
      }
    }

    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
    osDelay(ledPeriodMs);
  }
}

void Led4TimerCallback(void *argument)
{
  HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if ((GPIO_Pin == B1_Pin) && (buttonSemaphoreHandle != NULL))
  {
    (void)osSemaphoreRelease(buttonSemaphoreHandle);
  }
}

void StartButtonTask(void *argument)
{
  uint32_t pressedStartTick = 0;

  for (;;)
  {
    if (osSemaphoreAcquire(buttonSemaphoreHandle, osWaitForever) != osOK)
    {
      continue;
    }

    osDelay(BUTTON_POLL_PERIOD_MS);

    GPIO_PinState currentState = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

    if (currentState == GPIO_PIN_SET)
    {
      pressedStartTick = osKernelGetTickCount();
      continue;
    }

    uint32_t pressedTimeMs = osKernelGetTickCount() - pressedStartTick;

    if (pressedTimeMs >= BUTTON_LONG_PRESS_MS)
    {
      appStatus.longPressCount++;
      (void)osEventFlagsSet(appEventFlagsHandle, APP_EVENT_TOGGLE_DISPLAY);
    }
    else
    {
      AppCommand_t command = APP_CMD_SHORT_PRESS;
      appStatus.shortPressCount++;
      (void)osMessageQueuePut(appCommandQueueHandle, &command, 0, 0);
      (void)osMessageQueuePut(displayCommandQueueHandle, &command, 0, 0);
    }
  }
}

void StartTouchTask(void *argument)
{
  uint8_t wasPressed = 0;
  uint8_t releaseSamples = 0;

  for (;;)
  {
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t isPressed = App_Touch_ReadPoint(&x, &y);

    if (isPressed != 0U)
    {
      releaseSamples = 0U;
      appStatus.touchActive = 1U;
      appStatus.touchReady = 1U;
      appStatus.touchX = x;
      appStatus.touchY = y;

      if (wasPressed == 0U)
      {
        App_Touch_HandlePress(x, y);
        wasPressed = 1U;
      }
    }
    else if (wasPressed != 0U)
    {
      releaseSamples++;
      if (releaseSamples >= 3U)
      {
        wasPressed = 0U;
        releaseSamples = 0U;
        appStatus.touchActive = 0U;
      }
    }
    else
    {
      appStatus.touchActive = 0U;
    }

    osDelay(TOUCH_POLL_PERIOD_MS);
  }
}

void StartDisplayTask(void *argument)
{
  uint32_t phase = 0;
  uint8_t animationEnabled = 1;
  uint8_t effectMode = 0;
  uint32_t frameCount = 0;
  uint32_t fpsStartTick = osKernelGetTickCount();
  uint8_t frameDrawn = 0;
  AppCommand_t command;

  App_FramebufferFill(App_Rgb565(0, 0, 0));
  App_DisplayPresent();

  for (;;)
  {
    frameDrawn = 0;

    uint32_t flags = osEventFlagsWait(appEventFlagsHandle,
                                      APP_EVENT_TOGGLE_DISPLAY,
                                      osFlagsWaitAny,
                                      0);
    if (((flags & osFlagsError) == 0U) && ((flags & APP_EVENT_TOGGLE_DISPLAY) != 0U))
    {
      animationEnabled = !animationEnabled;
      appStatus.animationEnabled = animationEnabled;
    }

    if (osMessageQueueGet(displayCommandQueueHandle, &command, NULL, 0) == osOK)
    {
      if (command == APP_CMD_SHORT_PRESS)
      {
        effectMode = (effectMode + 1U) % 2U;
        appStatus.effectMode = effectMode;
      }
    }

    if (osMutexAcquire(displayMutexHandle, osWaitForever) == osOK)
    {
      if (effectMode == 0U)
      {
        App_FramebufferDrawWaterWave(phase);
      }
      else
      {
        App_FramebufferDrawHoverStripes(phase);
      }

      if (animationEnabled != 0U)
      {
        phase = (phase + 3U) % 192U;
        frameDrawn = 1U;
      }

      App_FramebufferDrawStatusBar();
      App_FramebufferDrawTouchControls();
      App_FramebufferDrawTouchMarker();
      App_DisplayPresent();
      osMutexRelease(displayMutexHandle);
    }

    if (frameDrawn != 0U)
    {
      frameCount++;
    }

    uint32_t now = osKernelGetTickCount();
    if ((now - fpsStartTick) >= 1000U)
    {
      appStatus.fps = (animationEnabled != 0U) ? frameCount : 0U;
      frameCount = 0;
      fpsStartTick = now;
    }

    osDelay(40);
  }
}

void StartStatsTask(void *argument)
{
  for (;;)
  {
    appStatus.heapFreeKb = (uint32_t)(xPortGetFreeHeapSize() / 1024U);
    appStatus.uptimeSec = osKernelGetTickCount() / 1000U;
    osDelay(1000);
  }
}

static void App_LTDC_WaitForVerticalBlank(void)
{
  uint32_t startTick = HAL_GetTick();

  while (((LTDC->CPSR & LTDC_CPSR_CYPOS) < hltdc.Init.AccumulatedActiveH) &&
         ((HAL_GetTick() - startTick) < 20U))
  {
  }
}

static void App_DisplayPresent(void)
{
  uint32_t nextVisibleAddress = appDrawFramebufferAddress;
  uint32_t nextDrawAddress = appVisibleFramebufferAddress;

  App_LTDC_WaitForVerticalBlank();

  if (HAL_LTDC_SetAddress(&hltdc, nextVisibleAddress, 0) != HAL_OK)
  {
    Error_Handler();
  }

  appVisibleFramebufferAddress = nextVisibleAddress;
  appDrawFramebufferAddress = nextDrawAddress;
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_HOST */
  MX_USB_HOST_Init();
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(100);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
