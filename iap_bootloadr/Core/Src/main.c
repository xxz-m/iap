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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//Bootloader 起始地址
#define BOOTLOADER_ADDR      0x08000000U
//App 起始地址。
#define APP_ADDR             0x08020000U
//内部 Flash 结束边界的下一个地址
#define FLASH_END_ADDR       0x08100000U
// SRAM 起始地址。
#define SRAM_START_ADDR      0x20000000U
//SRAM 结束边界的下一个地址。
#define SRAM_END_ADDR        0x20020000U
#define IAP_FILE_SIZE        4300U
#define IAP_PACKET_SIZE      256U
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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t boot_app_is_valid(uint32_t app_addr);
static void boot_jump_to_app(uint32_t app_addr);
static HAL_StatusTypeDef boot_erase_app_flash(uint32_t Sector, uint32_t NbSectors);
static HAL_StatusTypeDef boot_write_app_flash(uint32_t addr, uint8_t *data, uint32_t len);
static void boot_iap_receive_fixed_file(void);
uint8_t uart2_rx_byte[1] = {0};
static uint8_t iap_rx_buf[IAP_FILE_SIZE];
static volatile uint8_t iap_update_request = 0;

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	printf("bootloader start\r\n");
	HAL_Delay(1000);
	if (boot_app_is_valid(APP_ADDR)) {
			printf("valid app, wait key to jump\r\n");
			HAL_Delay(100);
	} else {
			printf("no valid app, stay in bootloader\r\n");
	}
	HAL_UART_Receive_IT(&huart1, uart2_rx_byte, 1);
  /* USER CODE END 2 */
	
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
		if (iap_update_request) {
			iap_update_request = 0;
			boot_iap_receive_fixed_file();
		}

		if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3)==RESET)
		{
				HAL_Delay(100);
				if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3)==RESET){
						printf("boot_jump_to_app_ran\r\n");
						boot_jump_to_app(APP_ADDR);
				}
		}
		HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
    HAL_Delay(500);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
    return ch;
}

/*
判断app的合法性

*/
static uint8_t boot_app_is_valid(uint32_t app_addr)
{
	//1.将目标地址转换为msp
	uint32_t app_map=*(__IO uint32_t *)app_addr;
	uint32_t app_reset = *(__IO uint32_t *)(app_addr + 4U);
	if((app_map<SRAM_START_ADDR )|| (app_map>SRAM_END_ADDR))
		return 0;
	if((app_reset<APP_ADDR)||(app_reset>=FLASH_END_ADDR))
		return 0;
	printf("app_msp   = 0x%08X\r\n", app_map);
	printf("app_reset = 0x%08X\r\n", app_reset);

	return 1;
}

//定义函数指针类型
typedef void (*boot_app_entry_t)(void);

static void boot_jump_to_app(uint32_t app_addr)
{
    //1.读取 App 向量表
    uint32_t app_msp = *(__IO uint32_t *)app_addr;
    uint32_t app_reset = *(__IO uint32_t *)(app_addr + 4U);
    //2. Reset_Handler 地址转成可调用函数
    boot_app_entry_t app_entry = (boot_app_entry_t)app_reset;
	//3. 关闭全局中断
    __disable_irq();
	//4.反初始化串口
    HAL_UART_DeInit(&huart1);
    HAL_UART_DeInit(&huart2);
    //5.复位 HAL 初始化过的一些外设状态
    HAL_DeInit();
	//6.停止 SysTick
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
	//7.清除 NVIC 中断
    for (uint8_t i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
	//8.设置 App 向量表
    SCB->VTOR = app_addr;
    __set_MSP(app_msp);
	//9.跳转 App
		__enable_irq();
    app_entry();
}

static HAL_StatusTypeDef boot_erase_app_flash(uint32_t Sector, uint32_t NbSectors)
{
	HAL_StatusTypeDef status;
	FLASH_EraseInitTypeDef erase_init;
	uint32_t sector_error = 0;

	// APP 从 FLASH_SECTOR_5 开始，禁止擦除 bootloader 区域。
	if (Sector < FLASH_SECTOR_5) {
		return HAL_ERROR;
	}

	// 擦除或写入 Flash 前必须先解锁。
	HAL_FLASH_Unlock();

	// 内部 Flash 按扇区擦除。
	erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
	erase_init.Sector = Sector;
	erase_init.NbSectors = NbSectors;
	erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	status = HAL_FLASHEx_Erase(&erase_init, &sector_error);

	// 操作结束后重新锁定 Flash。
	HAL_FLASH_Lock();

	return status;
}
static HAL_StatusTypeDef boot_write_app_flash(uint32_t addr, uint8_t *data, uint32_t len)
{
	HAL_StatusTypeDef status = HAL_OK;
	uint32_t i = 0;
	uint32_t word = 0;
	// 判断addcr是不是在起始地址-终止地址之间
	if (addr < APP_ADDR || addr >= FLASH_END_ADDR) {
		return HAL_ERROR;
	}
	// 擦除或写入 Flash 前必须先解锁。
	HAL_FLASH_Unlock();

	while (i < len) {
		word = 0xFFFFFFFF;

		for (uint8_t j = 0; j < 4; j++) {
			if ((i + j) < len) {
				word &= ~((uint32_t)0xFF << (8 * j));
				word |= ((uint32_t)data[i + j] << (8 * j));
			}
		}
		 //往 STM32 内部 Flash写数据
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word);
		if (status != HAL_OK) {
			break;
		}

		i += 4;
	}
	//关闭锁
	HAL_FLASH_Lock();

	return status;
}

static void boot_iap_receive_fixed_file(void)
{
	HAL_UART_AbortReceive_IT(&huart1);
	printf("iap start, erasing app flash\r\n");

	if (boot_erase_app_flash(FLASH_SECTOR_5, 1) != HAL_OK) {
		printf("erase failed\r\n");
		HAL_UART_Receive_IT(&huart1, uart2_rx_byte, 1);
		return;
	}

	printf("erase ok, file size: %lu, send bin file now\r\n", IAP_FILE_SIZE);

	if (HAL_UART_Receive(&huart1, iap_rx_buf, IAP_FILE_SIZE, 30000) != HAL_OK) {
		printf("receive timeout\r\n");
		HAL_UART_Receive_IT(&huart1, uart2_rx_byte, 1);
		return;
	}

	printf("receive ok, writing flash\r\n");

	if (boot_write_app_flash(APP_ADDR, iap_rx_buf, IAP_FILE_SIZE) != HAL_OK) {
		printf("write failed\r\n");
		HAL_UART_Receive_IT(&huart1, uart2_rx_byte, 1);
		return;
	}

	if (boot_app_is_valid(APP_ADDR)) {
		printf("iap update ok\r\n");
	} else {
		printf("iap update invalid\r\n");
	}

	HAL_UART_Receive_IT(&huart1, uart2_rx_byte, 1);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == USART1) {
		if (uart2_rx_byte[0] == 'U') {
			iap_update_request = 1;
			return;
		}

		HAL_UART_Transmit(&huart1, uart2_rx_byte, 1, 100);
		HAL_UART_Receive_IT(&huart1, uart2_rx_byte, 1);
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













