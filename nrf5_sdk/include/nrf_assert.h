/*$$$LICENCE_NORDIC_STANDARD<2006>$$$*/
/** @file
 * @brief Utilities for verifying program logic
 */

#ifndef NRF_ASSERT_H_
#define NRF_ASSERT_H_

#include <stdint.h>
#include "nrf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Function for handling assertions.
 *
 *
 * @note
 * This function is called when an assertion has triggered.
 *
 * @note
 * This function is deprecated and will be removed in future releases.
 * Use app_error_fault_handler instead.
 *
 *
 * @post
 * All hardware is put into an idle non-emitting state (in particular the radio is highly
 * important to switch off since the radio might be in a state that makes it send
 * packets continiously while a typical final infinit ASSERT loop is executing).
 *
 *
 * @param line_num The line number where the assertion is called
 * @param file_name Pointer to the file name
 */
//lint -save -esym(14, assert_nrf_callback)
void assert_nrf_callback(uint16_t line_num, const uint8_t *file_name);
//lint -restore

#if (defined(DEBUG_NRF) || defined(DEBUG_NRF_USER))
#define NRF_ASSERT_PRESENT 1
#else
#define NRF_ASSERT_PRESENT 0
#endif

//#if defined(DEBUG_NRF) || defined(DEBUG_NRF_USER)

/*lint -emacro(506, ASSERT) */ /* Suppress "Constant value Boolean */
/*lint -emacro(774, ASSERT) */ /* Suppress "Boolean within 'if' always evaluates to True" */ \

/** @brief Function for checking intended for production code.
 *
 * Check passes if "expr" evaluates to true. */

#ifdef _lint
#define ASSERT(expr)                                                          \
if (expr)                                                                     \
{                                                                             \
}                                                                             \
else                                                                          \
{                                                                             \
    while (1);                                                                \
}
#else //_lint
#define ASSERT(expr)                                                          \
if (NRF_ASSERT_PRESENT)                                                       \
{                                                                             \
    if (expr)                                                                 \
    {                                                                         \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        assert_nrf_callback((uint16_t)__LINE__, (uint8_t *)__FILE__);         \
    }                                                                         \
}

#endif


#ifdef __cplusplus
}
#endif

#endif /* NRF_ASSERT_H_ */
