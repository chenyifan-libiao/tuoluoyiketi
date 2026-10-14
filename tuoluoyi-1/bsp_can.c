/**
  ******************************************************************************
  * @file    bsp_can.c
  * @brief   CAN1 驱动实现 (STM32F103C8T6, HAL 库)
  *
  * 位时序: APB1 = 8MHz, Prescaler = 1  -> 1 tq = 125ns
  *         SYNC(1) + BS1(5) + BS2(2) = 8 tq = 1us/bit  ->  1 Mbps
  *         采样点 = (1 + 5) / 8 = 75%
  ******************************************************************************
  */

#include "bsp_can.h"

CAN_HandleTypeDef hcan1;

/* 接收缓冲队列 (单生产者 = RX 中断, 单消费者 = 任务, 无锁环形缓冲) */
static CAN_RxMsg_t sRxQueue[CAN_RX_QUEUE_SIZE];
static volatile uint8_t sRxHead = 0;   /* 由中断写入 */
static volatile uint8_t sRxTail = 0;   /* 由任务写入 */

static volatile uint32_t sLastErrorCode = 0;

/**
  * @brief  CAN MSP 初始化: 使能时钟、配置 GPIO 与 NVIC
  *         由 HAL_CAN_Init() 自动调用
  */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if (hcan->Instance != CAN1)
	{
		return;
	}

	/* 使能 GPIOA 与 CAN1 时钟 (CAN1 挂在 APB1) */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_CAN1_CLK_ENABLE();

	/* PA11: CAN_RX (浮空输入) */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* PA12: CAN_TX (复用推挽输出) */
	GPIO_InitStruct.Pin = GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* NVIC: 优先级数值必须 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY(5),
	   这样中断里将来可以安全调用 FreeRTOS API */
	HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

	HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
}

/**
  * @brief  初始化并启动 CAN1
  */
HAL_StatusTypeDef CAN_Init(void)
{
	CAN_FilterTypeDef filter = {0};

	hcan1.Instance = CAN1;
	hcan1.Init.Prescaler = 1;                       /* APB1 8MHz / 1 = 8MHz tq 时钟 */
	hcan1.Init.Mode = CAN_MODE_NORMAL;              /* 正常模式 (需外接收发器) */
	hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;         /* 同步跳转宽度 1 tq */
	hcan1.Init.TimeSeg1 = CAN_BS1_5TQ;              /* 位段1 = 5 tq */
	hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;              /* 位段2 = 2 tq  -> 共 8 tq = 1Mbps */
	hcan1.Init.TimeTriggeredMode = DISABLE;         /* 关闭时间触发模式 */
	hcan1.Init.AutoBusOff = ENABLE;                 /* 总线关闭后自动恢复 */
	hcan1.Init.AutoWakeUp = ENABLE;                 /* 收到报文自动唤醒 */
	hcan1.Init.AutoRetransmission = ENABLE;         /* 自动重传 (标准 CAN 行为) */
	hcan1.Init.ReceiveFifoLocked = DISABLE;         /* FIFO 满时覆盖旧报文 */
	hcan1.Init.TransmitFifoPriority = DISABLE;      /* 按报文 ID 优先级发送 */

	if (HAL_CAN_Init(&hcan1) != HAL_OK)
	{
		return HAL_ERROR;
	}

	/* 过滤器: 掩码全 0 -> 接收总线上所有报文, 送入 FIFO0 */
	filter.FilterBank = 0;
	filter.FilterMode = CAN_FILTERMODE_IDMASK;
	filter.FilterScale = CAN_FILTERSCALE_32BIT;
	filter.FilterIdHigh = 0x0000;
	filter.FilterIdLow = 0x0000;
	filter.FilterMaskIdHigh = 0x0000;
	filter.FilterMaskIdLow = 0x0000;
	filter.FilterFIFOAssignment = CAN_RX_FIFO0;
	filter.FilterActivation = CAN_FILTER_ENABLE;
	filter.SlaveStartFilterBank = 14;

	if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
	{
		return HAL_ERROR;
	}

	/* 启动 CAN */
	if (HAL_CAN_Start(&hcan1) != HAL_OK)
	{
		return HAL_ERROR;
	}

	/* 使能接收与错误中断 */
	if (HAL_CAN_ActivateNotification(&hcan1,
			CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_BUSOFF | CAN_IT_ERROR) != HAL_OK)
	{
		return HAL_ERROR;
	}

	sRxHead = 0;
	sRxTail = 0;

	return HAL_OK;
}

/**
  * @brief  发送一帧标准 ID 数据报文
  */
HAL_StatusTypeDef CAN_Send(uint32_t StdId, uint8_t *pData, uint8_t Len)
{
	CAN_TxHeaderTypeDef txHeader = {0};
	uint32_t txMailbox = 0;
	uint32_t tickstart = HAL_GetTick();

	if (pData == NULL || Len > 8U)
	{
		return HAL_ERROR;
	}

	txHeader.StdId = StdId;
	txHeader.ExtId = 0;
	txHeader.IDE = CAN_ID_STD;
	txHeader.RTR = CAN_RTR_DATA;
	txHeader.DLC = Len;
	txHeader.TransmitGlobalTime = DISABLE;

	/* 等待空闲发送邮箱 */
	while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
	{
		if ((HAL_GetTick() - tickstart) > CAN_TX_TIMEOUT_MS)
		{
			return HAL_TIMEOUT;
		}
	}

	if (HAL_CAN_AddTxMessage(&hcan1, &txHeader, pData, &txMailbox) != HAL_OK)
	{
		sLastErrorCode = HAL_CAN_GetError(&hcan1);
		return HAL_ERROR;
	}

	return HAL_OK;
}

/**
  * @brief  从接收队列取出一帧报文 (非阻塞)
  */
uint8_t CAN_Receive(CAN_RxMsg_t *pMsg)
{
	uint8_t head;

	if (pMsg == NULL)
	{
		return 0;
	}

	head = sRxHead;
	if (head == sRxTail)
	{
		return 0;   /* 队列为空 */
	}

	*pMsg = sRxQueue[sRxTail];

	/* 先读完数据再移动尾指针, 保证与中断的单生产者/单消费者访问安全 */
	sRxTail = (uint8_t)((sRxTail + 1U) % CAN_RX_QUEUE_SIZE);

	return 1;
}

/**
  * @brief  查询接收队列中的报文数量
  */
uint8_t CAN_GetRxCount(void)
{
	uint8_t head = sRxHead;
	uint8_t tail = sRxTail;

	return (uint8_t)((head - tail + CAN_RX_QUEUE_SIZE) % CAN_RX_QUEUE_SIZE);
}

/**
  * @brief  清空接收队列
  */
void CAN_ClearRxBuffer(void)
{
	sRxTail = sRxHead;
}

/**
  * @brief  获取最近一次 CAN 错误码
  */
uint32_t CAN_GetLastError(void)
{
	return sLastErrorCode;
}

/**
  * @brief  FIFO0 收到报文中断回调: 把报文搬入接收队列
  *         由 HAL_CAN_IRQHandler() 调用
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_RxHeaderTypeDef rxHeader = {0};
	CAN_RxMsg_t msg = {0};
	uint8_t next;
	uint8_t i;

	if (hcan->Instance != CAN1)
	{
		return;
	}

	if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, msg.Data) != HAL_OK)
	{
		sLastErrorCode = HAL_CAN_GetError(hcan);
		return;
	}

	msg.StdId = rxHeader.StdId;
	msg.ExtId = rxHeader.ExtId;
	msg.IDE = rxHeader.IDE;
	msg.RTR = rxHeader.RTR;
	msg.DLC = (rxHeader.DLC > 8U) ? 8U : rxHeader.DLC;

	/* HAL 会无条件填满 Data[0..7], 不看 DLC。
	   当 DLC < 8 时, 后面的字节是寄存器残留值(垃圾数据), 必须清零,
	   否则 debug 观察时会误以为数据收错了 */
	for (i = (uint8_t)msg.DLC; i < 8U; i++)
	{
		msg.Data[i] = 0U;
	}

	next = (uint8_t)((sRxHead + 1U) % CAN_RX_QUEUE_SIZE);
	if (next == sRxTail)
	{
		/* 队列已满, 丢弃最新报文 (任务读取过慢) */
		sLastErrorCode |= 0x80000000U;
		return;
	}

	sRxQueue[sRxHead] = msg;
	sRxHead = next;
}

/**
  * @brief  CAN 错误中断回调
  */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
	if (hcan->Instance != CAN1)
	{
		return;
	}

	sLastErrorCode = HAL_CAN_GetError(hcan);
}

/**
  * @brief  CAN1 FIFO0 接收中断入口
  * @note   向量表中的名字是 USB_LP_CAN1_RX0_IRQHandler (USB 与 CAN_RX0 共用)
  */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}

/**
  * @brief  CAN1 错误/状态变化中断入口
  */
void CAN1_SCE_IRQHandler(void)
{
	HAL_CAN_IRQHandler(&hcan1);
}
