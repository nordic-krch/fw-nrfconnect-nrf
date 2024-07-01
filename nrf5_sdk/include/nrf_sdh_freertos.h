/*$$$LICENCE_NORDIC_STANDARD<2017>$$$*/

#ifndef NRF_SDH_FREERTOS_H__
#define NRF_SDH_FREERTOS_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name FreeRTOS implementation of SoftDevice Handler
 * @{
 * @ingroup  nrf_sdh
 */

typedef void (*nrf_sdh_freertos_task_hook_t)(void * p_context);

/**@brief   Function for creating a task to retrieve SoftDevice events.
 * @param[in]   hook        Function to run in the SoftDevice FreeRTOS task,
 *                          before entering the task loop.
 * @param[in]   p_context   Parameter for the function @p hook.
 */
void nrf_sdh_freertos_init(nrf_sdh_freertos_task_hook_t hook, void * p_context);

/**
 * @}
 */


#ifdef __cplusplus
}
#endif

#endif /* NRF_SDH_FREERTOS_H__ */
