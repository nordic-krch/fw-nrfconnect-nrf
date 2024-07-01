/*$$$LICENCE_NORDIC_STANDARD<2017>$$$*/

#include "sdk_common.h"
#include "nrf_sdh_soc.h"

#include "nrf_sdh.h"
#include "nrf_soc.h"
#include "app_error.h"


#define NRF_LOG_MODULE_NAME nrf_sdh_soc
#if NRF_SDH_SOC_LOG_ENABLED
    #define NRF_LOG_LEVEL       NRF_SDH_SOC_LOG_LEVEL
    #define NRF_LOG_INFO_COLOR  NRF_SDH_SOC_INFO_COLOR
    #define NRF_LOG_DEBUG_COLOR NRF_SDH_SOC_DEBUG_COLOR
#else
    #define NRF_LOG_LEVEL       0
#endif // NRF_SDH_SOC_LOG_ENABLED
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


// Create section set "sdh_soc_observers".
NRF_SECTION_SET_DEF(sdh_soc_observers, nrf_sdh_soc_evt_observer_t, CONFIG_NRF_SDH_SOC_OBSERVER_PRIO_LEVELS);


/**@brief   Function for polling SoC events.
 *
 * @param[in]   p_context   Context of the observer.
 */
static void nrf_sdh_soc_evts_poll(void * p_context)
{
    ret_code_t ret_code;

    UNUSED_VARIABLE(p_context);

    while (true)
    {
        uint32_t evt_id;

        ret_code = sd_evt_get(&evt_id);
        if (ret_code != NRF_SUCCESS)
        {
            break;
        }

        NRF_LOG_DEBUG("SoC event: 0x%x.", evt_id);

        // Forward the event to SoC observers.
        TYPE_SECTION_FOREACH(nrf_sdh_soc_evt_observer_t, sdh_soc_observers, p_observer)
        {
            p_observer->handler(evt_id, p_observer->p_context);
        }
    }

    if (ret_code != NRF_ERROR_NOT_FOUND)
    {
        APP_ERROR_HANDLER(ret_code);
    }
}


NRF_SDH_STACK_OBSERVER(m_nrf_sdh_soc_evts_poll, CONFIG_NRF_SDH_SOC_STACK_OBSERVER_PRIO) =
{
    .handler   = nrf_sdh_soc_evts_poll,
    .p_context = NULL,
};
