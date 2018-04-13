//{{{
/**
  ******************************************************************************
  * @file    stm32746g_discovery_sd.c
  * @author  MCD Application Team
  * @brief   This file includes the uSD card driver mounted on STM32746G-Discovery
  *          board.
  @verbatim
   1. How To use this driver:
   --------------------------
      - This driver is used to drive the micro SD external card mounted on STM32746G-Discovery
        board.
      - This driver does not need a specific component driver for the micro SD device
        to be included with.

   2. Driver description:
   ---------------------
     + Initialization steps:
        o Initialize the micro SD card using the BSP_SD_Init() function. This
          function includes the MSP layer hardware resources initialization and the
          SDIO interface configuration to interface with the external micro SD. It
          also includes the micro SD initialization sequence.
        o To check the SD card presence you can use the function BSP_SD_IsDetected() which
          returns the detection status
        o If SD presence detection interrupt mode is desired, you must configure the
          SD detection interrupt mode by calling the function BSP_SD_ITConfig(). The interrupt
          is generated as an external interrupt whenever the micro SD card is
          plugged/unplugged in/from the board.
        o The function BSP_SD_GetCardInfo() is used to get the micro SD card information
          which is stored in the structure "HAL_SD_CardInfoTypedef".

     + Micro SD card operations
        o The micro SD card can be accessed with read/write block(s) operations once
          it is ready for access. The access can be performed whether using the polling
          mode by calling the functions BSP_SD_ReadBlocks()/BSP_SD_WriteBlocks(), or by DMA
          transfer using the functions BSP_SD_ReadBlocks_DMA()/BSP_SD_WriteBlocks_DMA()
        o The DMA transfer complete is used with interrupt mode. Once the SD transfer
          is complete, the SD interrupt is handled using the function BSP_SD_IRQHandler(),
          the DMA Tx/Rx transfer complete are handled using the functions
          BSP_SD_DMA_Tx_IRQHandler()/BSP_SD_DMA_Rx_IRQHandler(). The corresponding user callbacks
          are implemented by the user at application level.
        o The SD erase block(s) is performed using the function BSP_SD_Erase() with specifying
          the number of blocks to erase.
        o The SD runtime status is returned when calling the function BSP_SD_GetCardState().

  @endverbatim
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
//}}}
#include "stm32746g_discovery_sd.h"

SD_HandleTypeDef uSdHandle;
static DMA_HandleTypeDef dma_rx_handle;
static DMA_HandleTypeDef dma_tx_handle;

void BSP_SDMMC_IRQHandler() { HAL_SD_IRQHandler (&uSdHandle); }
void BSP_SDMMC_DMA_Tx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmatx); }
void BSP_SDMMC_DMA_Rx_IRQHandler() { HAL_DMA_IRQHandler (uSdHandle.hdmarx); }

//{{{
uint8_t BSP_SD_Init()
{
  uint8_t sd_state = MSD_OK;

  /* uSD device interface configuration */
  uSdHandle.Instance = SDMMC1;
  uSdHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
  uSdHandle.Init.ClockBypass         = SDMMC_CLOCK_BYPASS_DISABLE;
  uSdHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  uSdHandle.Init.BusWide             = SDMMC_BUS_WIDE_1B;
  uSdHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  uSdHandle.Init.ClockDiv            = SDMMC_TRANSFER_CLK_DIV;
  BSP_SD_Detect_MspInit (&uSdHandle, NULL);

  if (BSP_SD_IsDetected() != SD_PRESENT)
    return MSD_ERROR_SD_NOT_PRESENT;

  /* Msp SD initialization */
  BSP_SD_MspInit (&uSdHandle, NULL);

  /* HAL SD initialization */
  if (HAL_SD_Init (&uSdHandle) != HAL_OK)
    sd_state = MSD_ERROR;

  /* Configure SD Bus width */
  if (sd_state == MSD_OK) {
    /* Enable wide operation */
    if (HAL_SD_ConfigWideBusOperation (&uSdHandle, SDMMC_BUS_WIDE_4B) != HAL_OK)
      sd_state = MSD_ERROR;
    else
      sd_state = MSD_OK;
    }

  return  sd_state;
  }
//}}}
//{{{
uint8_t BSP_SD_DeInit()
{
  uint8_t sd_state = MSD_OK;

  uSdHandle.Instance = SDMMC1;

  /* HAL SD deinitialization */
  if (HAL_SD_DeInit(&uSdHandle) != HAL_OK)
    sd_state = MSD_ERROR;

  /* Msp SD deinitialization */
  uSdHandle.Instance = SDMMC1;
  BSP_SD_MspDeInit (&uSdHandle, NULL);

  return  sd_state;
}
//}}}

//{{{
uint8_t BSP_SD_ITConfig() {

  GPIO_InitTypeDef gpio_init_structure;

  /* Configure Interrupt mode for SD detection pin */
  gpio_init_structure.Pin = SD_DETECT_PIN;
  gpio_init_structure.Pull = GPIO_PULLUP;
  gpio_init_structure.Speed = GPIO_SPEED_FAST;
  gpio_init_structure.Mode = GPIO_MODE_IT_RISING_FALLING;
  HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init_structure);

  /* Enable and set SD detect EXTI Interrupt to the lowest priority */
  HAL_NVIC_SetPriority((IRQn_Type)(SD_DETECT_EXTI_IRQn), 0x0F, 0x00);
  HAL_NVIC_EnableIRQ((IRQn_Type)(SD_DETECT_EXTI_IRQn));

  return MSD_OK;
  }
//}}}
//{{{
uint8_t BSP_SD_IsDetected() {

  __IO uint8_t status = SD_PRESENT;

  /* Check SD card detect pin */
  if (HAL_GPIO_ReadPin(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) == GPIO_PIN_SET)
    status = SD_NOT_PRESENT;

  return status;
  }
//}}}

//{{{
uint8_t BSP_SD_ReadBlocks (uint32_t* pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout) {

  if (HAL_SD_ReadBlocks(&uSdHandle, (uint8_t*)pData, ReadAddr, NumOfBlocks, Timeout) != HAL_OK)
    return MSD_ERROR;
  else
    return MSD_OK;
  }
//}}}
//{{{
uint8_t BSP_SD_WriteBlocks (uint32_t* pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout) {

  if (HAL_SD_WriteBlocks (&uSdHandle, (uint8_t*)pData, WriteAddr, NumOfBlocks, Timeout) != HAL_OK)
    return MSD_ERROR;
  else
    return MSD_OK;
  }
//}}}
//{{{
uint8_t BSP_SD_ReadBlocks_DMA (uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks) {

  /* Read block(s) in DMA transfer mode */
  if (HAL_SD_ReadBlocks_DMA(&uSdHandle, (uint8_t *)pData, ReadAddr, NumOfBlocks) != HAL_OK)
    return MSD_ERROR;
  else
    return MSD_OK;
  }
//}}}
//{{{
uint8_t BSP_SD_WriteBlocks_DMA (uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks) {

  /* Write block(s) in DMA transfer mode */
  if (HAL_SD_WriteBlocks_DMA (&uSdHandle, (uint8_t*)pData, WriteAddr, NumOfBlocks) != HAL_OK)
    return MSD_ERROR;
  else
    return MSD_OK;
  }
//}}}
//{{{
uint8_t BSP_SD_Erase (uint32_t StartAddr, uint32_t EndAddr) {

  if (HAL_SD_Erase (&uSdHandle, StartAddr, EndAddr) != HAL_OK)
    return MSD_ERROR;
  else
    return MSD_OK;
  }
//}}}

//{{{
__weak void BSP_SD_MspInit (SD_HandleTypeDef *hsd, void *Params) {

  GPIO_InitTypeDef gpio_init_structure;

  /* Enable SDIO clock */
  __HAL_RCC_SDMMC1_CLK_ENABLE();

  /* Enable DMA2 clocks */
  __DMAx_TxRx_CLK_ENABLE();

  /* Enable GPIOs clock */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* Common GPIO configuration */
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  gpio_init_structure.Alternate = GPIO_AF12_SDMMC1;

  /* GPIOC configuration */
  gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  HAL_GPIO_Init(GPIOC, &gpio_init_structure);

  /* GPIOD configuration */
  gpio_init_structure.Pin = GPIO_PIN_2;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);

  /* NVIC configuration for SDIO interrupts */
  HAL_NVIC_SetPriority(SDMMC1_IRQn, 0x0E, 0);
  HAL_NVIC_EnableIRQ(SDMMC1_IRQn);

  /* Configure DMA Rx parameters */
  dma_rx_handle.Init.Channel             = SD_DMAx_Rx_CHANNEL;
  dma_rx_handle.Init.Direction           = DMA_PERIPH_TO_MEMORY;
  dma_rx_handle.Init.PeriphInc           = DMA_PINC_DISABLE;
  dma_rx_handle.Init.MemInc              = DMA_MINC_ENABLE;
  dma_rx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_rx_handle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  dma_rx_handle.Init.Mode                = DMA_PFCTRL;
  dma_rx_handle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  dma_rx_handle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  dma_rx_handle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  dma_rx_handle.Init.MemBurst            = DMA_MBURST_INC4;
  dma_rx_handle.Init.PeriphBurst         = DMA_PBURST_INC4;

  dma_rx_handle.Instance = SD_DMAx_Rx_STREAM;

  /* Associate the DMA handle */
  __HAL_LINKDMA(hsd, hdmarx, dma_rx_handle);

  /* Deinitialize the stream for new transfer */
  HAL_DMA_DeInit(&dma_rx_handle);

  /* Configure the DMA stream */
  HAL_DMA_Init(&dma_rx_handle);

  /* Configure DMA Tx parameters */
  dma_tx_handle.Init.Channel             = SD_DMAx_Tx_CHANNEL;
  dma_tx_handle.Init.Direction           = DMA_MEMORY_TO_PERIPH;
  dma_tx_handle.Init.PeriphInc           = DMA_PINC_DISABLE;
  dma_tx_handle.Init.MemInc              = DMA_MINC_ENABLE;
  dma_tx_handle.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  dma_tx_handle.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
  dma_tx_handle.Init.Mode                = DMA_PFCTRL;
  dma_tx_handle.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
  dma_tx_handle.Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
  dma_tx_handle.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
  dma_tx_handle.Init.MemBurst            = DMA_MBURST_INC4;
  dma_tx_handle.Init.PeriphBurst         = DMA_PBURST_INC4;

  dma_tx_handle.Instance = SD_DMAx_Tx_STREAM;

  /* Associate the DMA handle */
  __HAL_LINKDMA(hsd, hdmatx, dma_tx_handle);

  /* Deinitialize the stream for new transfer */
  HAL_DMA_DeInit(&dma_tx_handle);

  /* Configure the DMA stream */
  HAL_DMA_Init(&dma_tx_handle);

  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority(SD_DMAx_Rx_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(SD_DMAx_Rx_IRQn);

  /* NVIC configuration for DMA transfer complete interrupt */
  HAL_NVIC_SetPriority(SD_DMAx_Tx_IRQn, 0x0F, 0);
  HAL_NVIC_EnableIRQ(SD_DMAx_Tx_IRQn);
}
//}}}
//{{{
__weak void BSP_SD_Detect_MspInit (SD_HandleTypeDef *hsd, void *Params) {
  GPIO_InitTypeDef  gpio_init_structure;
  SD_DETECT_GPIO_CLK_ENABLE();

  /* GPIO configuration in input for uSD_Detect signal */

  gpio_init_structure.Pin       = SD_DETECT_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_INPUT;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  HAL_GPIO_Init (SD_DETECT_GPIO_PORT, &gpio_init_structure);
  }
//}}}
//{{{
__weak void BSP_SD_MspDeInit (SD_HandleTypeDef *hsd, void *Params) {

  /* Disable NVIC for DMA transfer complete interrupts */
  HAL_NVIC_DisableIRQ (SD_DMAx_Rx_IRQn);
  HAL_NVIC_DisableIRQ (SD_DMAx_Tx_IRQn);

  /* Deinitialize the stream for new transfer */
  dma_rx_handle.Instance = SD_DMAx_Rx_STREAM;
  HAL_DMA_DeInit (&dma_rx_handle);

  /* Deinitialize the stream for new transfer */
  dma_tx_handle.Instance = SD_DMAx_Tx_STREAM;
  HAL_DMA_DeInit (&dma_tx_handle);

  /* Disable NVIC for SDIO interrupts */
  HAL_NVIC_DisableIRQ (SDMMC1_IRQn);

  /* DeInit GPIO pins can be done in the application
     (by surcharging this __weak function) */

  /* Disable SDMMC1 clock */
  __HAL_RCC_SDMMC1_CLK_DISABLE();

  /* GPIO pins clock and DMA clocks can be shut down in the application
     by surcharging this __weak function */
  }
//}}}
//{{{
uint8_t BSP_SD_GetCardState() {

  return((HAL_SD_GetCardState(&uSdHandle) == HAL_SD_CARD_TRANSFER ) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
  }
//}}}

void BSP_SD_GetCardInfo (HAL_SD_CardInfoTypeDef *CardInfo) { HAL_SD_GetCardInfo(&uSdHandle, CardInfo); }
void HAL_SD_AbortCallback (SD_HandleTypeDef *hsd) { BSP_SD_AbortCallback(); }

void HAL_SD_TxCpltCallback (SD_HandleTypeDef *hsd) { BSP_SD_WriteCpltCallback(); }
void HAL_SD_RxCpltCallback (SD_HandleTypeDef *hsd) { BSP_SD_ReadCpltCallback(); }

__weak void BSP_SD_AbortCallback() {}
__weak void BSP_SD_WriteCpltCallback() {}
__weak void BSP_SD_ReadCpltCallback() {}
