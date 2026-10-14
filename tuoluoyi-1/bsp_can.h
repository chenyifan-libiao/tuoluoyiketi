/**
  ******************************************************************************
  * @file    bsp_can.h
  * @brief   CAN1 驱动头文件 (STM32F103C8T6, HAL 库)
  *
  * 引脚分配 (CAN1 默认映射):
  *   PA11 - CAN_RX (浮空输入)
  *   PA12 - CAN_TX (复用推挽输出)
  *
  * 说明: STM32F103C8T6 的 CAN1 另一组重映射引脚为 PB8(RX)/PB13(TX),
  *       但 PB13 已被 SPI2 的 SCK 占用, 因此只能使用 PA11/PA12。
  *
  * 波特率: 1 Mbps  (APB1=8MHz, Prescaler=1, BS1=5TQ, BS2=2TQ, 采样点 75%)
  * 工作模式: 正常模式 (Normal) —— 需外接 CAN 收发器芯片才能与总线通信
  ******************************************************************************
  */

#ifndef __BSP_CAN_H
#define __BSP_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stm32f1xx_hal.h>

/* CAN 接收缓冲队列深度 (FIFO 有 3 级, 队列用于缓存供任务读取) */
#define CAN_RX_QUEUE_SIZE       8U

/* 发送等待空闲邮箱的超时时间 (ms) */
#define CAN_TX_TIMEOUT_MS       10U

/**
  * @brief  CAN 接收报文数据结构
  */
typedef struct
{
	uint32_t StdId;         /* 标准标识符 (11 位, 0~0x7FF) */
	uint32_t ExtId;         /* 扩展标识符 (29 位) */
	uint32_t IDE;           /* CAN_ID_STD 或 CAN_ID_EXT */
	uint32_t RTR;           /* CAN_RTR_DATA 数据帧 或 CAN_RTR_REMOTE 远程帧 */
	uint32_t DLC;           /* 数据长度 (0~8) */
	uint8_t  Data[8];       /* 数据 */
} CAN_RxMsg_t;

/* 全局 CAN 句柄 */
extern CAN_HandleTypeDef hcan1;

/**
  * @brief  初始化并启动 CAN1 (正常模式, 1Mbps, 接收全部报文, FIFO0 中断接收)
  * @retval HAL_OK 成功, 其他值失败
  */
HAL_StatusTypeDef CAN_Init(void);

/**
  * @brief  发送一帧标准 ID 数据报文 (阻塞等待空闲邮箱)
  * @param  StdId 标准标识符 (0~0x7FF)
  * @param  pData 数据缓冲区指针
  * @param  Len   数据长度 (0~8)
  * @retval HAL_OK 成功; HAL_TIMEOUT 无空闲邮箱; HAL_ERROR 参数错误
  * @note   正常模式下总线上必须有其他节点应答(ACK), 否则发送会一直重传直至超时
  */
HAL_StatusTypeDef CAN_Send(uint32_t StdId, uint8_t *pData, uint8_t Len);

/**
  * @brief  从接收队列中取出一帧报文 (非阻塞)
  * @param  pMsg 用于存放报文的指针
  * @retval 1 成功取到一帧; 0 队列为空
  */
uint8_t CAN_Receive(CAN_RxMsg_t *pMsg);

/**
  * @brief  查询接收队列中待读取的报文数量
  * @retval 报文数量 (0 ~ CAN_RX_QUEUE_SIZE)
  */
uint8_t CAN_GetRxCount(void);

/**
  * @brief  清空接收队列
  */
void CAN_ClearRxBuffer(void);

/**
  * @brief  获取最近一次 CAN 错误码 (HAL CAN Error Code)
  * @retval 错误码, 0 表示无错误
  */
uint32_t CAN_GetLastError(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CAN_H */
