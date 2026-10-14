/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - rajzolas 2 potmeterrel OLED-en
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
#include "adc.h"
#include "i2c.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "OLED.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Mozgoatlag ablakmerete (kisebb = gyorsabb valasz, nagyobb = stabilabb) */
#define AVG_N            8
/* Kijelzo merete */
#define OLED_W           128
#define OLED_H           64
/* Radir merete pixelben (negyzet oldala) */
#define ERASER_SIZE      3
/* Beepitett gomb: PC13. Nucleo-144 lapon nyomasra HIGH (ha forditva: GPIO_PIN_RESET) */
#define BTN_PORT         GPIOC
#define BTN_PIN          GPIO_PIN_13
#define BTN_PRESSED      GPIO_PIN_SET
/* Gomb prellmentesites (ms) */
#define BTN_LOCKOUT_MS   250
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t ADC_ReadBoth(uint16_t *ch_a, uint16_t *ch_b);
static void Draw_Point(int16_t x, int16_t y, uint8_t erase);
static void Draw_Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t erase);
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
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  OLED_Init();
  OLED_Fill(Black);
  OLED_UpdateScreen();

  /* Mozgoatlag: ket csatorna, AVG_N elemu gyuru puffer + futo osszeg */
  uint16_t buf[2][AVG_N] = {0};
  uint32_t sum[2] = {0, 0};
  uint8_t  idx = 0;

  /* Puffer elotoltese az elso meresekkel, hogy ne "felfusson" az atlag */
  for (uint8_t i = 0; i < AVG_N; i++)
  {
	  uint16_t a = 0, b = 0;
	  ADC_ReadBoth(&a, &b);
	  buf[0][i] = a;
	  buf[1][i] = b;
	  sum[0] += a;
	  sum[1] += b;
  }

  int16_t x = 0, y = 0;
  int16_t prev_x = (int16_t)((sum[0] / AVG_N) * (OLED_W - 1) / 4095);
  int16_t prev_y = (int16_t)((sum[1] / AVG_N) * (OLED_H - 1) / 4095);

  /* 0 = rajzolas, 1 = torles */
  uint8_t erase = 0;
  GPIO_PinState btn_prev = GPIO_PIN_RESET;
  uint32_t last_toggle = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  /* Mindket potmeter beolvasasa (blokkolo modban) es mozgoatlag */
	  uint16_t a, b;
	  if (ADC_ReadBoth(&a, &b))
	  {
		  sum[0] = sum[0] - buf[0][idx] + a;
		  sum[1] = sum[1] - buf[1][idx] + b;
		  buf[0][idx] = a;
		  buf[1][idx] = b;
		  idx++;
		  if (idx >= AVG_N)
		  {
			  idx = 0;
		  }
	  }

	  /* ADC (0..4095) -> kijelzo koordinata */
	  x = (int16_t)((sum[0] / AVG_N) * (OLED_W - 1) / 4095);
	  y = (int16_t)((sum[1] / AVG_N) * (OLED_H - 1) / 4095);

	  /* Gomb: felfuto el eseten valtunk rajzolas <-> torles kozott */
	  GPIO_PinState btn = HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN);
	  if ((btn == BTN_PRESSED) && (btn_prev != BTN_PRESSED) &&
	      ((HAL_GetTick() - last_toggle) > BTN_LOCKOUT_MS))
	  {
		  erase ^= 1;
		  last_toggle = HAL_GetTick();
		  /* Ne huzzon vonalat a mod valtas "ugrasa" miatt */
		  prev_x = x;
		  prev_y = y;
	  }
	  btn_prev = btn;

	  /* Rajzolas / torles az elozo pontbol az ujba (folyamatos vonal) */
	  Draw_Line(prev_x, prev_y, x, y, erase);
	  prev_x = x;
	  prev_y = y;

	  OLED_UpdateScreen();

	  HAL_Delay(5);

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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Mindket csatorna (rank 1 = PC0/IN10, rank 2 = PC3/IN13) beolvasasa.
  *         CubeMX: Scan Conversion Mode = Enabled, Number Of Conversion = 2,
  *         EOC Selection = EOC flag at the end of single channel conversion.
  * @retval 1 ha sikeres, 0 ha timeout
  */
static uint8_t ADC_ReadBoth(uint16_t *ch_a, uint16_t *ch_b)
{
	HAL_ADC_Start(&hadc1);

	if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
	{
		HAL_ADC_Stop(&hadc1);
		return 0;
	}
	*ch_a = (uint16_t)HAL_ADC_GetValue(&hadc1);

	if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
	{
		HAL_ADC_Stop(&hadc1);
		return 0;
	}
	*ch_b = (uint16_t)HAL_ADC_GetValue(&hadc1);

	HAL_ADC_Stop(&hadc1);
	return 1;
}

/**
  * @brief  Egy pont rajzolasa (erase=0: 1 pixel feher) vagy torlese
  *         (erase=1: ERASER_SIZE x ERASER_SIZE fekete negyzet).
  * @note   Feltetelezi, hogy az OLED konyvtarban van OLED_DrawPixel(x, y, szin).
  */
static void Draw_Point(int16_t x, int16_t y, uint8_t erase)
{
	if (!erase)
	{
		OLED_DrawPixel((uint8_t)x, (uint8_t)y, White);
		return;
	}

	int16_t r = ERASER_SIZE / 2;
	for (int16_t dy = -r; dy <= r; dy++)
	{
		for (int16_t dx = -r; dx <= r; dx++)
		{
			int16_t px = x + dx;
			int16_t py = y + dy;
			if (px >= 0 && px < OLED_W && py >= 0 && py < OLED_H)
			{
				OLED_DrawPixel((uint8_t)px, (uint8_t)py, Black);
			}
		}
	}
}

/**
  * @brief  Vonal (Bresenham) ket pont kozott, hogy gyors tekeresnel se
  *         maradjanak hezagok a rajzban.
  */
static void Draw_Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t erase)
{
	int16_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
	int16_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
	int16_t sx = (x0 < x1) ? 1 : -1;
	int16_t sy = (y0 < y1) ? 1 : -1;
	int16_t err = dx - dy;

	while (1)
	{
		Draw_Point(x0, y0, erase);
		if (x0 == x1 && y0 == y1)
		{
			break;
		}
		int16_t e2 = 2 * err;
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
