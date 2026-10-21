/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "dac.h"
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "math.h"
#include "OLED.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t normal_ekg[100];
uint32_t broken_ekg[100];

uint16_t speed_levels[] = {999, 699, 499, 299, 149};
uint8_t current_level = 0;
uint8_t max_speed_levels = sizeof(speed_levels) / sizeof(speed_levels[0]);

uint8_t state = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// EKG jel generálása
void GenerateEKG(uint32_t* signal, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        float t = (float)i / (float)length; // Normált idő: 0.0 és 1.0 között
        float val = 2048.0f;                // Nyugalmi alapvonal (kb. 1.65V)

        // P-hullám
        if (t >= 0.10f && t < 0.20f) {
            val += 220.0f * sinf((t - 0.10f) / 0.10f * M_PI);
        }
        // Q-tüske
        else if (t >= 0.30f && t < 0.34f) {
            val -= 300.0f * sinf((t - 0.30f) / 0.04f * M_PI);
        }
        // R-tüske
        else if (t >= 0.34f && t < 0.42f) {
            val += 1800.0f * sinf((t - 0.34f) / 0.08f * M_PI);
        }
        // S-tüske
        else if (t >= 0.42f && t < 0.46f) {
            val -= 600.0f * sinf((t - 0.42f) / 0.04f * M_PI);
        }
        // T-hullám
        else if (t >= 0.58f && t < 0.78f) {
            val += 450.0f * sinf((t - 0.58f) / 0.20f * M_PI);
        }

        // 12-bites korlátozás (0 – 4095)
        if (val > 4095.0f) val = 4095.0f;
        if (val < 0.0f) val = 0.0f;

        signal[i] = (uint32_t)val;
    }
}

// Összetört / Kaotikus szívritmus generálása
void GenerateBrokenHeart(uint32_t* signal, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        float t = (float)i / (float)length;
        float val = 2048.0f;

        if (t < 0.15f) {
            val += 1500.0f * sinf(t * 10.0f * M_PI);
        } else if (t >= 0.15f && t < 0.30f) {
            val -= 1800.0f * sinf((t - 0.15f) * 15.0f * M_PI);
        } else if (t >= 0.30f && t < 0.45f) {
            val += 2000.0f * sinf((t - 0.30f) * 20.0f * M_PI);
        } else if (t >= 0.45f && t < 0.60f) {
            val -= 1200.0f * sinf((t - 0.45f) * 8.0f * M_PI);
        } else if (t >= 0.60f && t < 0.85f) {
            val += (i % 2 == 0 ? 800.0f : -800.0f);
        }

        if (val > 4095.0f) val = 4095.0f;
        if (val < 0.0f) val = 0.0f;

        signal[i] = (uint32_t)val;
    }
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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_DAC_Init();
  MX_I2C2_Init();

  /* USER CODE BEGIN 2 */

  /* EKG tömbök feltöltése kiszámított értékekkel */
  GenerateEKG(normal_ekg, 100);
  GenerateBrokenHeart(broken_ekg, 100);

  /* DAC indítása DMA támogatással (normál EKG) */
  HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, normal_ekg, 100, DAC_ALIGN_12B_R);

  /* Timer2 időzítő indítása */
  HAL_TIM_Base_Start(&htim2);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
      {
        HAL_Delay(50); // Pergésmentesítés (Debounce)

        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
        {
          if (state == 0) // 1. FÁZIS: GYORSULÓ EKG
          {
            current_level++;
            if (current_level < max_speed_levels)
            {
              __HAL_TIM_SET_AUTORELOAD(&htim2, speed_levels[current_level]);
            }
            else
            {
              // Elértük a max sebességet -> Áttérés FLATLINE-ra (Egyenes vonal)
              state = 1;
              HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
              HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048);
              HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
            }
          }
          else if (state == 1) // 2. FÁZIS: EGYENES VONALBÓL ÖSSZETÖRT EKG
          {
            state = 2;
            HAL_DAC_Stop(&hdac, DAC_CHANNEL_1);
            HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, broken_ekg, 100, DAC_ALIGN_12B_R);
            __HAL_TIM_SET_AUTORELOAD(&htim2, 399); // Közepes frekvencia
          }
          else if (state == 2) // 3. FÁZIS: ÚJRAÉLESZTÉS / RESTART
          {
            state = 0;
            current_level = 0;
            __HAL_TIM_SET_AUTORELOAD(&htim2, speed_levels[0]);
            HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
            HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, normal_ekg, 100, DAC_ALIGN_12B_R);
          }

          // Várakozás a gomb elengedésére
          while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {
            HAL_Delay(10);
          }
        }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 64;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

#ifdef  USE_FULL_ASSERT
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
