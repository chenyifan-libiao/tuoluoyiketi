/**
  ******************************************************************************
  * @file    bsp_spi1.h
  * @brief   SPI1 / SPI2 驱动头文件 (STM32F103C8T6, HAL 库)
  *
  * SPI1 引脚分配 (APB2, 8MHz):
  *   PA4  - NSS  (软件片选, 普通推挽输出)
  *   PA5  - SCK  (复用推挽输出)
  *   PA6  - MISO (浮空输入)
  *   PA7  - MOSI (复用推挽输出)
  *
  * SPI2 引脚分配 (APB1, 8MHz):
  *   PB12 - NSS  (软件片选, 普通推挽输出)
  *   PB13 - SCK  (复用推挽输出)
  *   PB14 - MISO (浮空输入)
  *   PB15 - MOSI (复用推挽输出)
  ******************************************************************************
  */

#ifndef __BSP_SPI1_H
#define __BSP_SPI1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stm32f1xx_hal.h>

/* SPI1 片选引脚定义 (软件 NSS) */
#define SPI1_CS_PORT            GPIOA
#define SPI1_CS_PIN             GPIO_PIN_4
#define SPI1_CS_ENABLE()        HAL_GPIO_WritePin(SPI1_CS_PORT, SPI1_CS_PIN, GPIO_PIN_RESET)
#define SPI1_CS_DISABLE()       HAL_GPIO_WritePin(SPI1_CS_PORT, SPI1_CS_PIN, GPIO_PIN_SET)

/* SPI2 片选引脚定义 (软件 NSS) */
#define SPI2_CS_PORT            GPIOB
#define SPI2_CS_PIN             GPIO_PIN_12
#define SPI2_CS_ENABLE()        HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_RESET)
#define SPI2_CS_DISABLE()       HAL_GPIO_WritePin(SPI2_CS_PORT, SPI2_CS_PIN, GPIO_PIN_SET)

/* 全局 SPI 句柄 */
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

/**
  * @brief  初始化 SPI1 (主模式, 全双工, 8bit, CPOL=0/CPHA=1Edge, MSB first)
  *         当前系统时钟为 HSI 8MHz, APB2=8MHz, 8 分频后 SCK = 1MHz
  * @retval HAL_OK 成功, 其他值失败
  */
HAL_StatusTypeDef SPI1_Init(void);

/**
  * @brief  向 SPI1 发送一个字节
  * @param  data 待发送数据
  * @retval HAL_OK 成功
  */
HAL_StatusTypeDef SPI1_SendByte(uint8_t data);

/**
  * @brief  从 SPI1 接收一个字节 (发送 0xFF 产生时钟)
  * @retval 接收到的数据
  */
uint8_t SPI1_ReceiveByte(void);

/**
  * @brief  SPI1 全双工收发缓冲区
  * @param  pTxData 发送缓冲区指针 (可为 NULL, 则发送 0xFF)
  * @param  pRxData 接收缓冲区指针 (可为 NULL, 则丢弃接收数据)
  * @param  Size    数据长度 (字节)
  * @retval HAL_OK 成功
  */
HAL_StatusTypeDef SPI1_TransmitReceive(uint8_t *pTxData, uint8_t *pRxData, uint16_t Size);

/**
  * @brief  初始化 SPI2 (主模式, 全双工, 8bit, CPOL=0/CPHA=1Edge, MSB first)
  *         SPI2 挂在 APB1, 当前时钟 8MHz, 8 分频后 SCK = 1MHz
  * @retval HAL_OK 成功, 其他值失败
  */
HAL_StatusTypeDef SPI2_Init(void);

/**
  * @brief  向 SPI2 发送一个字节
  * @param  data 待发送数据
  * @retval HAL_OK 成功
  */
HAL_StatusTypeDef SPI2_SendByte(uint8_t data);

/**
  * @brief  从 SPI2 接收一个字节 (发送 0xFF 产生时钟)
  * @retval 接收到的数据
  */
uint8_t SPI2_ReceiveByte(void);

/**
  * @brief  SPI2 全双工收发缓冲区
  * @param  pTxData 发送缓冲区指针 (可为 NULL, 则发送 0xFF)
  * @param  pRxData 接收缓冲区指针 (可为 NULL, 则丢弃接收数据)
  * @param  Size    数据长度 (字节)
  * @retval HAL_OK 成功
  */
HAL_StatusTypeDef SPI2_TransmitReceive(uint8_t *pTxData, uint8_t *pRxData, uint16_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SPI1_H */
