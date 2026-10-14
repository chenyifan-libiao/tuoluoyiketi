/**
  ******************************************************************************
  * @file    bsp_spi1.c
  * @brief   SPI1 / SPI2 驱动实现 (STM32F103C8T6, HAL 库)
  ******************************************************************************
  */

#include "bsp_spi1.h"

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

/* SPI 超时时间 (ms) */
#define SPI1_TIMEOUT          100U
#define SPI2_TIMEOUT          100U

/**
  * @brief  SPI MSP 初始化: 使能时钟并配置 GPIO
  *         由 HAL_SPI_Init() 自动调用, 根据 hspi->Instance 区分 SPI1 / SPI2
  */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if (hspi->Instance == SPI1)
	{
		/* 使能 GPIOA 与 SPI1 时钟 */
		__HAL_RCC_GPIOA_CLK_ENABLE();
		__HAL_RCC_SPI1_CLK_ENABLE();

		/* PA4: NSS (软件片选, 普通推挽输出, 默认拉高) */
		HAL_GPIO_WritePin(SPI1_CS_PORT, SPI1_CS_PIN, GPIO_PIN_SET);
		GPIO_InitStruct.Pin = SPI1_CS_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(SPI1_CS_PORT, &GPIO_InitStruct);

		/* PA5: SCK, PA7: MOSI (复用推挽输出) */
		GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* PA6: MISO (浮空输入) */
		GPIO_InitStruct.Pin = GPIO_PIN_6;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	}
	else if (hspi->Instance == SPI2)
	{
		/* 使能 GPIOB 与 SPI2 时钟 */
		__HAL_RCC_GPIOB_CLK_ENABLE();
		__HAL_RCC_SPI2_CLK_ENABLE();

		/* PB12: NSS (软件片选, 普通推挽输出, 默认拉高) */
		HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_SET);
		GPIO_InitStruct.Pin = SPI2_CS_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(SPI2_CS_PORT, &GPIO_InitStruct);

		/* PB13: SCK, PB15: MOSI (复用推挽输出) */
		GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_15;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* PB14: MISO (浮空输入) */
		GPIO_InitStruct.Pin = GPIO_PIN_14;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	}
}

/**
  * @brief  初始化 SPI1
  */
HAL_StatusTypeDef SPI1_Init(void)
{
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;              /* 主机模式 */
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;    /* 全双工 */
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;        /* 8 位数据帧 */
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;      /* CPOL = 0, 空闲低电平 */
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;          /* CPHA = 0, 第一个边沿采样 (SPI 模式 0) */
	hspi1.Init.NSS = SPI_NSS_SOFT;                  /* 软件片选 */
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* APB2 8MHz / 8 = 1MHz */
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;         /* MSB 先行 */
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;

	if (HAL_SPI_Init(&hspi1) != HAL_OK)
	{
		return HAL_ERROR;
	}

	SPI1_CS_DISABLE();
	return HAL_OK;
}

/**
  * @brief  发送一个字节
  */
HAL_StatusTypeDef SPI1_SendByte(uint8_t data)
{
	return HAL_SPI_Transmit(&hspi1, &data, 1, SPI1_TIMEOUT);
}

/**
  * @brief  接收一个字节
  */
uint8_t SPI1_ReceiveByte(void)
{
	uint8_t data = 0;

	HAL_SPI_Receive(&hspi1, &data, 1, SPI1_TIMEOUT);
	return data;
}

/**
  * @brief  全双工收发缓冲区
  */
HAL_StatusTypeDef SPI1_TransmitReceive(uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
	uint8_t dummyTx = 0xFF;
	uint8_t dummyRx;
	HAL_StatusTypeDef status;

	if (pTxData != NULL && pRxData != NULL)
	{
		status = HAL_SPI_TransmitReceive(&hspi1, pTxData, pRxData, Size, SPI1_TIMEOUT);
	}
	else if (pTxData != NULL)
	{
		status = HAL_SPI_Transmit(&hspi1, pTxData, Size, SPI1_TIMEOUT);
	}
	else if (pRxData != NULL)
	{
		status = HAL_SPI_Receive(&hspi1, pRxData, Size, SPI1_TIMEOUT);
	}
	else
	{
		status = HAL_SPI_TransmitReceive(&hspi1, &dummyTx, &dummyRx, 1, SPI1_TIMEOUT);
	}

	return status;
}

/**
  * @brief  初始化 SPI2
  */
HAL_StatusTypeDef SPI2_Init(void)
{
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;              /* 主机模式 */
	hspi2.Init.Direction = SPI_DIRECTION_2LINES;    /* 全双工 */
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;        /* 8 位数据帧 */
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;      /* CPOL = 0, 空闲低电平 */
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;          /* CPHA = 0, 第一个边沿采样 (SPI 模式 0) */
	hspi2.Init.NSS = SPI_NSS_SOFT;                  /* 软件片选 */
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; /* APB1 8MHz / 8 = 1MHz */
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;         /* MSB 先行 */
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 10;

	if (HAL_SPI_Init(&hspi2) != HAL_OK)
	{
		return HAL_ERROR;
	}

	SPI2_CS_DISABLE();
	return HAL_OK;
}

/**
  * @brief  向 SPI2 发送一个字节
  */
HAL_StatusTypeDef SPI2_SendByte(uint8_t data)
{
	return HAL_SPI_Transmit(&hspi2, &data, 1, SPI2_TIMEOUT);
}

/**
  * @brief  从 SPI2 接收一个字节
  */
uint8_t SPI2_ReceiveByte(void)
{
	uint8_t data = 0;

	HAL_SPI_Receive(&hspi2, &data, 1, SPI2_TIMEOUT);
	return data;
}

/**
  * @brief  SPI2 全双工收发缓冲区
  */
HAL_StatusTypeDef SPI2_TransmitReceive(uint8_t *pTxData, uint8_t *pRxData, uint16_t Size)
{
	uint8_t dummyTx = 0xFF;
	uint8_t dummyRx;
	HAL_StatusTypeDef status;

	if (pTxData != NULL && pRxData != NULL)
	{
		status = HAL_SPI_TransmitReceive(&hspi2, pTxData, pRxData, Size, SPI2_TIMEOUT);
	}
	else if (pTxData != NULL)
	{
		status = HAL_SPI_Transmit(&hspi2, pTxData, Size, SPI2_TIMEOUT);
	}
	else if (pRxData != NULL)
	{
		status = HAL_SPI_Receive(&hspi2, pRxData, Size, SPI2_TIMEOUT);
	}
	else
	{
		status = HAL_SPI_TransmitReceive(&hspi2, &dummyTx, &dummyRx, 1, SPI2_TIMEOUT);
	}

	return status;
}
