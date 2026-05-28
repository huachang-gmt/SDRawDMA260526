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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
//#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

SD_HandleTypeDef hsd1;

/* USER CODE BEGIN PV */

#define BUFFER_SIZE_BYTES     (32 * 1024)
#define RECORD_SIZE_BYTES     64

#define SD_BLOCK_SIZE         512
#define SD_BLOCK_COUNT        (BUFFER_SIZE_BYTES / SD_BLOCK_SIZE)

#define START_SECTOR          0x1000

volatile uint8_t sd_dma_tx_done = 0;
volatile uint8_t sd_dma_tx_error = 0;

uint32_t global_record_id = 0; // 『 每一筆 』 資料索引  每一筆資料的「全球唯一流水號」
uint32_t current_file_id = 0; // 這筆資料屬於哪個 64MB 檔案 例如： file 0   file 1  file 2 ....  例如： 假設：  file_id = 5   代表：   這筆資料： 屬於：  第5個 64MB segment  之後 packaging： 你只要掃： file_id == 5 就能做成：  LOG0005.TXT

uint32_t current_chunk_count = 0;// 計算目前累積了多少個 32KB chunk   你現在： 每次 DMA：  寫入 32KB    而：  64MB   需要：  2048 次 32KB   所以：   current_chunk_count++;   代表：  目前已經寫了幾個chunk   當：  current_chunk_count >= 2048  代表： 一個64MB segment完成   然後： current_file_id++;     開始下一個檔案。

#define CHUNKS_PER_FILE   2048

#define RECORD_TAIL_MARKER   0xAA55

typedef struct
{
    uint8_t payload[54];

    uint16_t tail;

    uint32_t record_id;

    uint32_t file_id;

} log_record_t;

#define RECORD_COUNT   (BUFFER_SIZE_BYTES / sizeof(log_record_t))

uint32_t current_sector = START_SECTOR;

__attribute__((section(".RAM_D1")))
__attribute__((aligned(32)))
log_record_t sd_buffer[RECORD_COUNT]; // 寫入 SD 卡 的 buffer

__attribute__((section(".RAM_D1")))
__attribute__((aligned(32)))
log_record_t read_buffer[RECORD_COUNT]; // 讀出 SD 卡 的 buffer

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_SDMMC1_SD_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 模擬方式填滿 64 byte 資料
void FillBuffer(void)
{
    uint32_t i;
    uint32_t j;

    log_record_t *record;

    for(i = 0; i < RECORD_COUNT; i++)
    {
        record = &sd_buffer[i];

        for(j = 0; j < 52; j++)
        {
            record->payload[j] =
                (uint8_t)('A' + (j % 26));
        }

        record->payload[52] = '\r';
        record->payload[53] = '\n';

        record->tail = 0xAA55;

        record->record_id = global_record_id++;

        record->file_id = current_file_id;
    }
}

// 計算 SD 卡內有機個 64M Byte 
uint32_t CountSegments(uint32_t expected_segments)
{
    uint32_t segment_count = 0;

    uint32_t segment;

    uint32_t sector;

    for(segment = 0;
        segment < expected_segments;
        segment++)
    {
        sector =
            START_SECTOR +
            (segment * CHUNKS_PER_FILE * SD_BLOCK_COUNT);

        if(HAL_SD_ReadBlocks(&hsd1,
                             (uint8_t*)read_buffer,
                             sector,
                             SD_BLOCK_COUNT,
                             HAL_MAX_DELAY) != HAL_OK)
        {
            break;
        }

        SCB_InvalidateDCache_by_Addr(
            (uint32_t*)read_buffer,
            BUFFER_SIZE_BYTES
        );

        /* 只檢查 tail marker */

        if(read_buffer[0].tail != 0xAA55)
        {
            break;
        }

        segment_count++;
    }

    return segment_count;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  int32_t timeout;
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SDMMC1_SD_Init();
  /* USER CODE BEGIN 2 */
    
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  /* USER CODE END 2 */

  /* Initialize leds */
  //BSP_LED_Init(LED_GREEN);
  //BSP_LED_Init(LED_YELLOW);
  //BSP_LED_Init(LED_RED);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  /*
  2048 loops = 1 個 64MB segment 
  4096 loops = 2 個 64MB segment 
  6144 loops = 3 個 64MB segment 
  8192 loops = 4 個 64MB segment 
  10240 loops = 5 個 64MB segment 

  16384 loops = 8 個 64MB segment 
  */

  while (1)
  {
      uint64_t loop;
      uint64_t max_loop = 16384; // 驗證過，確實會產生 8 個 Pulse 2026-05-28 
      uint32_t segmentation = max_loop / 2048;

      for(loop = 0; loop < max_loop; loop++)  //可形成 4 個 64MB segmentation
      {
          FillBuffer(); 
          //memset(sd_buffer, 0x55, BUFFER_SIZE_BYTES);//驗證 FillBuffer(); 是否有問題之用

          SCB_CleanDCache_by_Addr((uint32_t*)sd_buffer, BUFFER_SIZE_BYTES);

          sd_dma_tx_done = 0;
          sd_dma_tx_error = 0;

          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);//寫入到SD卡前拉高電位

          if(HAL_SD_WriteBlocks_DMA(&hsd1,
                                     (uint8_t*)sd_buffer,
                                    current_sector,
                                    SD_BLOCK_COUNT) != HAL_OK)
          {
              Error_Handler();
          }

          while((sd_dma_tx_done == 0) && (sd_dma_tx_error == 0))
          {
            /* DMA背景寫入期間 CM7持續做別的工作 本行用於證明這是非阻塞式 工作模式，當 HAL_SD_WriteBlocks_DMA 執行後， CM7 控制權可以去做其他事情，不必等待 SD 卡寫完 */
            //HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_6);
          }

          // ★ 等 SD card internal write 完成（關鍵）
          while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
          {
          }

          HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);// 完成SD卡寫入，電位拉低

          if(sd_dma_tx_error)
          {
              Error_Handler();
          }

          current_sector += SD_BLOCK_COUNT;

          current_chunk_count++;

          if(current_chunk_count >= CHUNKS_PER_FILE)
          {
              current_chunk_count = 0;
              current_file_id++;
          }

          //HAL_Delay(10);//不需要透過這種固定式延遲，由 while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) 來判斷是否寫入完成
      }


      // 上面是寫入資料到 SD 卡，以 Raw data 方式，這裡是讀出 SD 卡資料。
      // 每讀出一個 64M Byte 單位，就顯示一個脈波高電位
      uint32_t segment_count;

      segment_count = CountSegments(segmentation);

      while(1)
      {
          uint32_t i;

          for(i = 0; i < segment_count; i++)
          {
              HAL_GPIO_WritePin(GPIOA,
                                GPIO_PIN_6,
                                GPIO_PIN_SET);

              HAL_Delay(1);

              HAL_GPIO_WritePin(GPIOA,
                                GPIO_PIN_6,
                                GPIO_PIN_RESET);

              HAL_Delay(1);
          }

          HAL_Delay(10);
      }

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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 2;
  RCC_OscInitStruct.PLL.PLLN = 36;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SDMMC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDMMC1_SD_Init(void)
{

  /* USER CODE BEGIN SDMMC1_Init 0 */

  /* USER CODE END SDMMC1_Init 0 */

  /* USER CODE BEGIN SDMMC1_Init 1 */

  /* USER CODE END SDMMC1_Init 1 */
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_ENABLE;
  hsd1.Init.ClockDiv = 0;
  if (HAL_SD_Init(&hsd1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SDMMC1_Init 2 */

  if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
  {
      Error_Handler();
  }

  /* USER CODE END SDMMC1_Init 2 */

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(PULSE_Output_GPIO_Port, PULSE_Output_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : SD_Detect_Pin */
  GPIO_InitStruct.Pin = SD_Detect_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SD_Detect_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PULSE_Output_Pin */
  GPIO_InitStruct.Pin = PULSE_Output_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(PULSE_Output_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    sd_dma_tx_done = 1;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
    sd_dma_tx_error = 1;
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB; // 兼具寫與讀情況下，這裡要修改
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

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
