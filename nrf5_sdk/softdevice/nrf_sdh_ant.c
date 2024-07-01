/*$$$LICENCE_NORDIC_STANDARD<2017>$$$*/

#include "sdk_common.h"
#if NRF_MODULE_ENABLED(NRF_SDH_ANT)

#include "nrf_sdh_ant.h"

#include "nrf_sdh.h"
#include "app_error.h"
#include "nrf_strerror.h"
#include "ant_interface.h"


#define NRF_LOG_MODULE_NAME nrf_sdh_ant
#if NRF_SDH_ANT_LOG_ENABLED
    #define NRF_LOG_LEVEL       NRF_SDH_ANT_LOG_LEVEL
    #define NRF_LOG_INFO_COLOR  NRF_SDH_ANT_INFO_COLOR
    #define NRF_LOG_DEBUG_COLOR NRF_SDH_ANT_DEBUG_COLOR
#else
    #define NRF_LOG_LEVEL       0
#endif // NRF_SDH_ANT_LOG_ENABLED
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


STATIC_ASSERT(NRF_SDH_ANT_TOTAL_CHANNELS_ALLOCATED <= MAX_ANT_CHANNELS);
STATIC_ASSERT(NRF_SDH_ANT_ENCRYPTED_CHANNELS <= NRF_SDH_ANT_TOTAL_CHANNELS_ALLOCATED);

// Create section set "sdh_ant_observers".
NRF_SECTION_SET_DEF(sdh_ant_observers, nrf_sdh_ant_evt_observer_t, NRF_SDH_ANT_OBSERVER_PRIO_LEVELS);

// Memory buffer provided in order to support channel configuration.
__ALIGN(4) static uint8_t m_ant_stack_buffer[NRF_SDH_ANT_BUF_SIZE];


static bool m_stack_is_enabled;


ret_code_t nrf_sdh_ant_enable(void)
{
    ANT_ENABLE ant_enable_cfg =
    {
        .ucTotalNumberOfChannels     = NRF_SDH_ANT_TOTAL_CHANNELS_ALLOCATED,
        .ucNumberOfEncryptedChannels = NRF_SDH_ANT_ENCRYPTED_CHANNELS,
        .usNumberOfEvents            = NRF_SDH_ANT_EVENT_QUEUE_SIZE,
        .pucMemoryBlockStartLocation = m_ant_stack_buffer,
        .usMemoryBlockByteSize       = sizeof(m_ant_stack_buffer),
    };

    ret_code_t ret_code = sd_ant_enable(&ant_enable_cfg);
    if (ret_code == NRF_SUCCESS)
    {
        m_stack_is_enabled = true;
    }
    else
    {
        NRF_LOG_ERROR("sd_ant_enable() returned %s.", nrf_strerror_get(ret_code));
    }

    return ret_code;
}


/**@brief       Function for polling ANT events.
 *
 * @param[in]   p_context   Context of the observer.
 */
static void nrf_sdh_ant_evts_poll(void * p_context)
{
    UNUSED_VARIABLE(p_context);

    ret_code_t ret_code;

#ifndef SER_CONNECTIVITY
    if (!m_stack_is_enabled)
    {
        return;
    }
#else
    UNUSED_VARIABLE(m_stack_is_enabled);
#endif // SER_CONNECTIVITY

    while (true)
    {
        ant_evt_t  ant_evt;

        ret_code = sd_ant_event_get(&ant_evt.channel, &ant_evt.event, ant_evt.message.aucMessage);
        if (ret_code != NRF_SUCCESS)
        {
            break;
        }

        NRF_LOG_DEBUG("ANT Event 0x%02X Channel 0x%02X", ant_evt.event, ant_evt.channel);

        // Forward the event to ANT observers.
        nrf_section_iter_t  iter;
        for (nrf_section_iter_init(&iter, &sdh_ant_observers);
             nrf_section_iter_get(&iter) != NULL;
             nrf_section_iter_next(&iter))
        {
            nrf_sdh_ant_evt_observer_t * p_observer;
            nrf_sdh_ant_evt_handler_t    handler;

            p_observer = (nrf_sdh_ant_evt_observer_t *) nrf_section_iter_get(&iter);
            handler    = p_observer->handler;

            handler(&ant_evt, p_observer->p_context);
        }
    }

    if (ret_code != NRF_ERROR_NOT_FOUND)
    {
        APP_ERROR_HANDLER(ret_code);
    }
}


NRF_SDH_STACK_OBSERVER(m_nrf_sdh_ant_evts_poll, NRF_SDH_ANT_STACK_OBSERVER_PRIO) =
{
    .handler   = nrf_sdh_ant_evts_poll,
    .p_context = NULL,
};

#endif // NRF_MODULE_ENABLED(NRF_SDH_ANT)
