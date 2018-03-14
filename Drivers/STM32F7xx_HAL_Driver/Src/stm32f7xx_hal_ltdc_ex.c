#include "stm32f7xx_hal.h"

//{{{
HAL_StatusTypeDef HAL_LTDC_StructInitFromVideoConfig(LTDC_HandleTypeDef* hltdc, DSI_VidCfgTypeDef *VidCfg)
{
	/* Retrieve signal polarities from DSI */

	/* The following polarity is inverted:
										 LTDC_DEPOLARITY_AL <-> LTDC_DEPOLARITY_AH */

	/* Note 1 : Code in line w/ Current LTDC specification */
	hltdc->Init.DEPolarity = (VidCfg->DEPolarity == DSI_DATA_ENABLE_ACTIVE_HIGH) ? LTDC_DEPOLARITY_AL : LTDC_DEPOLARITY_AH;
	hltdc->Init.VSPolarity = (VidCfg->VSPolarity == DSI_VSYNC_ACTIVE_HIGH) ? LTDC_VSPOLARITY_AH : LTDC_VSPOLARITY_AL;
	hltdc->Init.HSPolarity = (VidCfg->HSPolarity == DSI_HSYNC_ACTIVE_HIGH) ? LTDC_HSPOLARITY_AH : LTDC_HSPOLARITY_AL;

	/* Note 2: Code to be used in case LTDC polarities inversion updated in the specification */
	/* hltdc->Init.DEPolarity = VidCfg->DEPolarity << 29;
		 hltdc->Init.VSPolarity = VidCfg->VSPolarity << 29;
		 hltdc->Init.HSPolarity = VidCfg->HSPolarity << 29; */

	/* Retrieve vertical timing parameters from DSI */
	hltdc->Init.VerticalSync       = VidCfg->VerticalSyncActive - 1;
	hltdc->Init.AccumulatedVBP     = VidCfg->VerticalSyncActive + VidCfg->VerticalBackPorch - 1;
	hltdc->Init.AccumulatedActiveH = VidCfg->VerticalSyncActive + VidCfg->VerticalBackPorch + VidCfg->VerticalActive - 1;
	hltdc->Init.TotalHeigh         = VidCfg->VerticalSyncActive + VidCfg->VerticalBackPorch + VidCfg->VerticalActive + VidCfg->VerticalFrontPorch - 1;

	return HAL_OK;
}
//}}}
//{{{
HAL_StatusTypeDef HAL_LTDC_StructInitFromAdaptedCommandConfig(LTDC_HandleTypeDef* hltdc, DSI_CmdCfgTypeDef *CmdCfg)
{
	/* Retrieve signal polarities from DSI */

	/* The following polarities are inverted:
										 LTDC_DEPOLARITY_AL <-> LTDC_DEPOLARITY_AH
										 LTDC_VSPOLARITY_AL <-> LTDC_VSPOLARITY_AH
										 LTDC_HSPOLARITY_AL <-> LTDC_HSPOLARITY_AH)*/

	/* Note 1 : Code in line w/ Current LTDC specification */
	hltdc->Init.DEPolarity = (CmdCfg->DEPolarity == DSI_DATA_ENABLE_ACTIVE_HIGH) ? LTDC_DEPOLARITY_AL : LTDC_DEPOLARITY_AH;
	hltdc->Init.VSPolarity = (CmdCfg->VSPolarity == DSI_VSYNC_ACTIVE_HIGH) ? LTDC_VSPOLARITY_AL : LTDC_VSPOLARITY_AH;
	hltdc->Init.HSPolarity = (CmdCfg->HSPolarity == DSI_HSYNC_ACTIVE_HIGH) ? LTDC_HSPOLARITY_AL : LTDC_HSPOLARITY_AH;

	/* Note 2: Code to be used in case LTDC polarities inversion updated in the specification */
	/* hltdc->Init.DEPolarity = CmdCfg->DEPolarity << 29;
		 hltdc->Init.VSPolarity = CmdCfg->VSPolarity << 29;
		 hltdc->Init.HSPolarity = CmdCfg->HSPolarity << 29; */

	return HAL_OK;
}
//}}}
