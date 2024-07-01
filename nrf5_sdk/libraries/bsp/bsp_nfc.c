/*$$$LICENCE_NORDIC_STANDARD<2015>$$$*/
#include "bsp_nfc.h"
#include "bsp.h"
#include "nrf.h"
#include "app_util_platform.h"

#ifndef BSP_SIMPLE
#define BTN_ACTION_SLEEP          BSP_BUTTON_ACTION_RELEASE    /**< Button action used to put the application into sleep mode. */

ret_code_t bsp_nfc_btn_init(uint32_t sleep_button)
{
    uint32_t err_code = bsp_event_to_button_action_assign(sleep_button,
                                                          BTN_ACTION_SLEEP,
                                                          BSP_EVENT_SLEEP);
    return err_code;
}

ret_code_t bsp_nfc_btn_deinit(uint32_t sleep_button)
{
    uint32_t err_code = bsp_event_to_button_action_assign(sleep_button,
                                                          BTN_ACTION_SLEEP,
                                                          BSP_EVENT_DEFAULT);
    return err_code;
}

ret_code_t bsp_nfc_sleep_mode_prepare(void)
{
#if defined(NFCT_PRESENT)
    // Check if peripheral is not used.
    CRITICAL_REGION_ENTER();
#ifdef NRF52832_XXAA
    if ((*(uint32_t *)0x40005410 & 0x07) == 0)
#else
    if ((NRF_NFCT->NFCTAGSTATE & NFCT_NFCTAGSTATE_NFCTAGSTATE_Msk)
        == NFCT_NFCTAGSTATE_NFCTAGSTATE_Disabled)
#endif // NRF52832_XXAA
    {
        NRF_NFCT->TASKS_SENSE = 1;
    }
    CRITICAL_REGION_EXIT();
    return NRF_SUCCESS;
#else
    return NRF_ERROR_NOT_SUPPORTED;
#endif
}
#endif //BSP_SIMPLE
