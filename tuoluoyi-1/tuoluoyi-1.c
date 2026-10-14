/**
  ******************************************************************************
  * @file    FreeRTOS/FreeRTOS_ThreadCreation/Src/main.c
  * @author  MCD Application Team
  * @version V1.2.2
  * @date    25-May-2015
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stm32f1xx_hal.h>
#include <../CMSIS_RTOS/cmsis_os.h>
#include "bsp_spi1.h"
#include "bsp_can.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CAN_RX_DBG_BUF_SIZE     16U   /* CAN 接收调试缓冲区深度 */

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
osThreadId LEDThread1Handle, LEDThread2Handle, TaskMainHandle, TaskCanHandle;

/* CAN 接收报文调试缓冲区 (debug 时在 Watch 窗口添加下面的变量名即可观察)
   注意: 必须为非 static 全局变量, 否则会被优化掉而无法在 Watch 中看到 */
CAN_RxMsg_t rxMsgBuf[CAN_RX_DBG_BUF_SIZE];  /* 滚动保存最近 16 帧报文 (含 ID/DLC/Data) */
CAN_RxMsg_t rxMsgLast;                      /* 最近收到的一帧 (单帧观察更方便) */
volatile uint16_t rxMsgTotal;               /* 累计接收帧数, 一直在涨说明通信正常 */
volatile uint8_t  rxMsgWrIdx;               /* 下一个写入位置 (0 ~ 15) */
volatile uint8_t  rxMsgWrapped;             /* 是否已写满一圈并回绕, 1=缓冲区已满过 */
volatile uint32_t rxMsgLastTick;            /* 收到最后一帧时的 HAL tick (ms) */

/* Private function prototypes -----------------------------------------------*/
static void LED_Thread1(void const *argument);
static void LED_Thread2(void const *argument);
static void task_main(void const *argument);
static void task_can(void const *argument);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
	HAL_Init();  
	
	/* 初始化 SPI1 (PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI) */
	SPI1_Init();

	/* 初始化 SPI2 (PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI) */
	SPI2_Init();

	/* 初始化 CAN1 (PA11=RX, PA12=TX, 正常模式, 1Mbps) */
	CAN_Init();

	__GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin = GPIO_PIN_9 | GPIO_PIN_9;

	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Thread 1 definition */
//	osThreadDef(LED1, LED_Thread1, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
  
	 /*  Thread 2 definition */
	osThreadDef(LED2, LED_Thread2, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
  
	/* Start thread 1 */
//	LEDThread1Handle = osThreadCreate(osThread(LED1), NULL);
  
	/* Start thread 2 */
	LEDThread2Handle = osThreadCreate(osThread(LED2), NULL);
  
	/* task_main definition */
	osThreadDef(TASK_MAIN, task_main, osPriorityNormal, 1, configMINIMAL_STACK_SIZE);
  
	/* Start task_main */
	TaskMainHandle = osThreadCreate(osThread(TASK_MAIN), NULL);
  
	/* task_can definition */
	osThreadDef(TASK_CAN, task_can, osPriorityNormal, 0, configMINIMAL_STACK_SIZE * 2);
  
	/* Start task_can */
	TaskCanHandle = osThreadCreate(osThread(TASK_CAN), NULL);
  
	/* Start scheduler */
	osKernelStart();

	  /* We should never get here as control is now taken by the scheduler */
	for (;;)
		;
}

void SysTick_Handler(void)
{
	HAL_IncTick();
	osSystickHandler();
}

/**
  * @brief  Toggle LED1
  * @param  thread not used
  * @retval None
  */
static void LED_Thread1(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
		osDelay(2000);
		
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
		osThreadSuspend(LEDThread2Handle);
		osDelay(2000);
		
		osThreadResume(LEDThread2Handle);
	}
}

/**
  * @brief  Toggle LED2 thread
  * @param  argument not used
  * @retval None
  */
static void LED_Thread2(void const *argument)
{
	uint32_t count;
	(void) argument;
  
	for (;;)
	{
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_9);
		osDelay(200);
	}
}

/**
  * @brief  task_main: 每隔 500ms 通过 
  SPI1 发送一个字节 0xAA
  * @param  argument not used
  * @retval None
  */
static void task_main(void const *argument)
{
	(void) argument;
  
	for (;;)
	{
		SPI2_CS_ENABLE();       /* 拉低 PB12 片选 */
		SPI2_SendByte(0xAA);
		SPI2_CS_DISABLE();      /* 拉高 PB12 片选 */
		
		osDelay(500);           /* 0.5s */
	}
}

/**
  * @brief  task_can: 每隔 0.5s 通过 CAN1 发送一帧 1 字节数据 0xAA, 并读取收到的报文
  * @param  argument not used
  * @retval None
  */
static void task_can(void const *argument)
{
	CAN_RxMsg_t rxMsg;
	uint8_t txData = 0xAA;
	(void) argument;
  
	for (;;)
	{
		/* 发送: 标准 ID = 0x123, 1 字节数据 0xAA */
		CAN_Send(0x123, &txData, 1);
		
		/* 接收: 取出队列中所有报文 */
		while (CAN_Receive(&rxMsg))
		{
			/* 存入滚动缓冲区, debug 时可在 Watch 中观察 rxMsgBuf / rxMsgLast */
			rxMsgBuf[rxMsgWrIdx] = rxMsg;
			rxMsgLast = rxMsg;
			rxMsgLastTick = HAL_GetTick();
			rxMsgTotal++;
			rxMsgWrIdx = (uint8_t)((rxMsgWrIdx + 1U) % CAN_RX_DBG_BUF_SIZE);
			if (rxMsgWrIdx == 0U)
			{
				rxMsgWrapped = 1U;
			}
		}
		
		osDelay(500);           /* 0.5s */
	}
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
	while (1)
	{
	}
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
