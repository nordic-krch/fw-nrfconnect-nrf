/*$$$LICENCE_NORDIC_STANDARD<2017>$$$*/

#include "sdk_common.h"
#include "nrf_sdh.h"

#include <stdint.h>

#include "nrf_sdm.h"
#include "nrf_nvic.h"
#include "app_error.h"
#include "app_util_platform.h"


#define NRF_LOG_MODULE_NAME nrf_sdh
#if NRF_SDH_LOG_ENABLED
    #define NRF_LOG_LEVEL       NRF_SDH_LOG_LEVEL
    #define NRF_LOG_INFO_COLOR  NRF_SDH_INFO_COLOR
    #define NRF_LOG_DEBUG_COLOR NRF_SDH_DEBUG_COLOR
#else
    #define NRF_LOG_LEVEL       0
#endif // NRF_SDH_LOG_ENABLED
#include "nrf_log.h"
NRF_LOG_MODULE_REGISTER();


// Validate configuration options.

#ifdef CONFIG_NRF_SDH_DISPATCH_MODEL_APPSH
    #if (!APP_SCHEDULER_ENABLED)
        #error app_scheduler is required when CONFIG_NRF_SDH_DISPATCH_MODEL is set to NRF_SDH_DISPATCH_MODEL_APPSH
    #endif
    #include "app_scheduler.h"
#endif

#if (   (CONFIG_NRF_SDH_CLOCK_LF_SRC      == NRF_CLOCK_LF_SRC_RC)          \
     && (CONFIG_NRF_SDH_CLOCK_LF_ACCURACY != NRF_CLOCK_LF_ACCURACY_500_PPM))
    #warning Please select NRF_CLOCK_LF_ACCURACY_500_PPM when using NRF_CLOCK_LF_SRC_RC
#endif


// Create section "sdh_req_observers".
NRF_SECTION_SET_DEF(sdh_req_observers, nrf_sdh_req_observer_t,
		CONFIG_NRF_SDH_REQ_OBSERVER_PRIO_LEVELS);

// Create section "sdh_state_observers".
NRF_SECTION_SET_DEF(sdh_state_observers, nrf_sdh_state_observer_t,
		CONFIG_NRF_SDH_STATE_OBSERVER_PRIO_LEVELS);

// Create section "sdh_stack_observers".
NRF_SECTION_SET_DEF(sdh_stack_observers, nrf_sdh_stack_observer_t,
		CONFIG_NRF_SDH_STACK_OBSERVER_PRIO_LEVELS);


static bool m_nrf_sdh_enabled;   /**< Variable to indicate whether the SoftDevice is enabled. */
static bool m_nrf_sdh_suspended; /**< Variable to indicate whether this module is suspended. */
static bool m_nrf_sdh_continue;  /**< Variable to indicate whether enable/disable process was started. */


/**@brief   Function for notifying request observers.
 *
 * @param[in]   evt     Type of request event.
 */
static ret_code_t sdh_request_observer_notify(nrf_sdh_req_evt_t req)
{
    NRF_LOG_DEBUG("State request: 0x%08X", req);

    TYPE_SECTION_FOREACH(nrf_sdh_req_observer_t, sdh_req_observers, p_observer)
    {
        if (p_observer->handler(req, p_observer->p_context))
        {
            NRF_LOG_DEBUG("Notify observer %p => ready", p_observer);
        }
        else
        {
            // Process is stopped.
            NRF_LOG_DEBUG("Notify observer %p => blocking", p_observer);
            return NRF_ERROR_BUSY;
        }
    }
    return NRF_SUCCESS;
}


/**@brief   Function for stage request observers.
 *
 * @param[in]   evt Type of stage event.
 */
static void sdh_state_observer_notify(nrf_sdh_state_evt_t evt)
{
    NRF_LOG_DEBUG("State change: 0x%08X", evt);

    TYPE_SECTION_FOREACH(nrf_sdh_state_observer_t, sdh_state_observers, p_observer)
    {
        p_observer->handler(evt, p_observer->p_context);
    }
}


static void softdevices_evt_irq_enable(void)
{
#ifdef CONFIG_SOFTDEVICE
    ret_code_t ret_code = sd_nvic_EnableIRQ((IRQn_Type)SD_EVT_IRQn);
    APP_ERROR_CHECK(ret_code);
#else
    // In case of serialization, NVIC must be accessed directly.
    NVIC_EnableIRQ(SD_EVT_IRQn);
#endif
}


static void softdevice_evt_irq_disable(void)
{
#ifdef CONFIG_SOFTDEVICE
    ret_code_t ret_code = sd_nvic_DisableIRQ((IRQn_Type)SD_EVT_IRQn);
    APP_ERROR_CHECK(ret_code);
#else
    // In case of serialization, NVIC must be accessed directly.
    NVIC_DisableIRQ(SD_EVT_IRQn);
#endif
}


ret_code_t nrf_sdh_enable_request(void)
{
    ret_code_t ret_code;

    if (m_nrf_sdh_enabled)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    m_nrf_sdh_continue = true;

    // Notify observers about SoftDevice enable request.
    if (sdh_request_observer_notify(NRF_SDH_EVT_ENABLE_REQUEST) == NRF_ERROR_BUSY)
    {
        // Enable process was stopped.
        return NRF_SUCCESS;
    }

    // Notify observers about starting SoftDevice enable process.
    sdh_state_observer_notify(NRF_SDH_EVT_STATE_ENABLE_PREPARE);

    nrf_clock_lf_cfg_t const clock_lf_cfg =
    {
        .source       = CONFIG_NRF_SDH_CLOCK_LF_SRC,
        .rc_ctiv      = CONFIG_NRF_SDH_CLOCK_LF_RC_CTIV,
        .rc_temp_ctiv = CONFIG_NRF_SDH_CLOCK_LF_RC_TEMP_CTIV,
        .accuracy     = CONFIG_NRF_SDH_CLOCK_LF_ACCURACY
    };

    /*CRITICAL_REGION_ENTER();*/
#ifdef ANT_LICENSE_KEY
    ret_code = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler, ANT_LICENSE_KEY);
#else
    ret_code = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler);
#endif
    m_nrf_sdh_enabled = (ret_code == NRF_SUCCESS);
    /*CRITICAL_REGION_EXIT();*/

    if (ret_code != NRF_SUCCESS)
    {
        return ret_code;
    }

    m_nrf_sdh_continue  = false;
    m_nrf_sdh_suspended = false;

    // Enable event interrupt.
    // Interrupt priority has already been set by the stack.
    softdevices_evt_irq_enable();

    // Notify observers about a finished SoftDevice enable process.
    sdh_state_observer_notify(NRF_SDH_EVT_STATE_ENABLED);

    return NRF_SUCCESS;
}


ret_code_t nrf_sdh_disable_request(void)
{
    ret_code_t ret_code;

    if (!m_nrf_sdh_enabled)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    m_nrf_sdh_continue = true;

    // Notify observers about SoftDevice disable request.
    if (sdh_request_observer_notify(NRF_SDH_EVT_DISABLE_REQUEST) == NRF_ERROR_BUSY)
    {
        // Disable process was stopped.
        return NRF_SUCCESS;
    }

    // Notify observers about starting SoftDevice disable process.
    sdh_state_observer_notify(NRF_SDH_EVT_STATE_DISABLE_PREPARE);

    CRITICAL_REGION_ENTER();
    ret_code          = sd_softdevice_disable();
    m_nrf_sdh_enabled = false;
    CRITICAL_REGION_EXIT();

    if (ret_code != NRF_SUCCESS)
    {
        return ret_code;
    }

    m_nrf_sdh_continue = false;

    softdevice_evt_irq_disable();

    // Notify observers about a finished SoftDevice enable process.
    sdh_state_observer_notify(NRF_SDH_EVT_STATE_DISABLED);

    return NRF_SUCCESS;
}


ret_code_t nrf_sdh_request_continue(void)
{
    if (!m_nrf_sdh_continue)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    if (m_nrf_sdh_enabled)
    {
        return nrf_sdh_disable_request();
    }
    else
    {
        return nrf_sdh_enable_request();
    }
}


bool nrf_sdh_is_enabled(void)
{
    return m_nrf_sdh_enabled;
}


void nrf_sdh_suspend(void)
{
    if (!m_nrf_sdh_enabled)
    {
        return;
    }

    softdevice_evt_irq_disable();
    m_nrf_sdh_suspended = true;
}


void nrf_sdh_resume(void)
{
    if ((!m_nrf_sdh_suspended) || (!m_nrf_sdh_enabled))
    {
        return;
    }

    // Force calling ISR again to make sure that events not previously pulled have been processed.
#ifdef CONFIG_SOFTDEVICE
    ret_code_t ret_code = sd_nvic_SetPendingIRQ((IRQn_Type)SD_EVT_IRQn);
    APP_ERROR_CHECK(ret_code);
#else
    NVIC_SetPendingIRQ((IRQn_Type)SD_EVT_IRQn);
#endif

    softdevices_evt_irq_enable();

    m_nrf_sdh_suspended = false;
}


bool nrf_sdh_is_suspended(void)
{
    return (!m_nrf_sdh_enabled) || (m_nrf_sdh_suspended);
}


void nrf_sdh_evts_poll(void)
{
    // Notify observers about pending SoftDevice event.
    TYPE_SECTION_FOREACH(nrf_sdh_stack_observer_t, sdh_stack_observers, p_observer)
    {
        p_observer->handler(p_observer->p_context);
    }
}


#if defined(CONFIG_NRF_SDH_DISPATCH_MODEL_INTERRUPT)

void SD_EVT_IRQHandler(void)
{
    nrf_sdh_evts_poll();
}

#elif defined(CONFIG_NRF_SDH_DISPATCH_MODEL_APPSH)

/**@brief   Function for polling SoftDevice events.
 *
 * @note    This function is compatible with @ref app_sched_event_handler_t.
 *
 * @param[in]   p_event_data Pointer to the event data.
 * @param[in]   event_size   Size of the event data.
 */
static void appsh_events_poll(void * p_event_data, uint16_t event_size)
{
    UNUSED_PARAMETER(p_event_data);
    UNUSED_PARAMETER(event_size);

    nrf_sdh_evts_poll();
}


void SD_EVT_IRQHandler(void)
{
    ret_code_t ret_code = app_sched_event_put(NULL, 0, appsh_events_poll);
    APP_ERROR_CHECK(ret_code);
}


#elif defined(CONFIG_NRF_SDH_DISPATCH_MODEL_POLLING)

#else

#error "Unknown SoftDevice handler dispatch model."

#endif // NRF_SDH_DISPATCH_MODEL

static void isr_handler(const void *arg)
{
	ARG_UNUSED(arg);
	SD_EVT_IRQHandler();
}

static int sd_irq_init(void)
{
	IRQ_CONNECT(SD_EVT_IRQn, 1, isr_handler, NULL, 0);
	irq_enable(SD_EVT_IRQn);
	return 0;
}

SYS_INIT(sd_irq_init, POST_KERNEL, 0);
