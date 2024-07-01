/*$$$LICENCE_NORDIC_STANDARD<2011>$$$*/

#include "sdk_common.h"
#include "nrf_strerror.h"

/**
 * @brief Macro for adding an entity to the description array.
 *
 * Macro that helps to create a single entity in the description array.
 */
#define NRF_STRERROR_ENTITY(mnemonic) {.code = mnemonic, .name = #mnemonic}

/**
 * @brief Array entity element that describes an error.
 */
typedef struct
{
    ret_code_t   code; /**< Error code. */
    char const * name; /**< Descriptive name (the same as the internal error mnemonic). */
}nrf_strerror_desc_t;

/**
 * @brief Unknown error code.
 *
 * The constant string used by @ref nrf_strerror_get when the error description was not found.
 */
static char const m_unknown_str[] = "Unknown error code";

/**
 * @brief Array with error codes.
 *
 * Array that describes error codes.
 *
 * @note It is required for this array to have error codes placed in ascending order.
 *       This condition is checked in automatic unit test before the release.
 */
static nrf_strerror_desc_t const nrf_strerror_array[] =
{
    NRF_STRERROR_ENTITY(NRF_SUCCESS),
    NRF_STRERROR_ENTITY(NRF_ERROR_SVC_HANDLER_MISSING),
    NRF_STRERROR_ENTITY(NRF_ERROR_SOFTDEVICE_NOT_ENABLED),
    NRF_STRERROR_ENTITY(NRF_ERROR_INTERNAL),
    NRF_STRERROR_ENTITY(NRF_ERROR_NO_MEM),
    NRF_STRERROR_ENTITY(NRF_ERROR_NOT_FOUND),
    NRF_STRERROR_ENTITY(NRF_ERROR_NOT_SUPPORTED),
    NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_PARAM),
    NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_STATE),
    NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_LENGTH),
    NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_FLAGS),
    NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_DATA),
    NRF_STRERROR_ENTITY(NRF_ERROR_DATA_SIZE),
    NRF_STRERROR_ENTITY(NRF_ERROR_TIMEOUT),
    NRF_STRERROR_ENTITY(NRF_ERROR_NULL),
    NRF_STRERROR_ENTITY(NRF_ERROR_FORBIDDEN),
    NRF_STRERROR_ENTITY(NRF_ERROR_INVALID_ADDR),
    NRF_STRERROR_ENTITY(NRF_ERROR_BUSY),
#ifdef NRF_ERROR_CONN_COUNT
    NRF_STRERROR_ENTITY(NRF_ERROR_CONN_COUNT),
#endif
#ifdef NRF_ERROR_RESOURCES
    NRF_STRERROR_ENTITY(NRF_ERROR_RESOURCES),
#endif

    /* SDK Common errors */
    NRF_STRERROR_ENTITY(NRF_ERROR_MODULE_NOT_INITIALIZED),
    NRF_STRERROR_ENTITY(NRF_ERROR_MUTEX_INIT_FAILED),
    NRF_STRERROR_ENTITY(NRF_ERROR_MUTEX_LOCK_FAILED),
    NRF_STRERROR_ENTITY(NRF_ERROR_MUTEX_UNLOCK_FAILED),
    NRF_STRERROR_ENTITY(NRF_ERROR_MUTEX_COND_INIT_FAILED),
    NRF_STRERROR_ENTITY(NRF_ERROR_MODULE_ALREADY_INITIALIZED),
    NRF_STRERROR_ENTITY(NRF_ERROR_STORAGE_FULL),
    NRF_STRERROR_ENTITY(NRF_ERROR_API_NOT_IMPLEMENTED),
    NRF_STRERROR_ENTITY(NRF_ERROR_FEATURE_NOT_ENABLED),
    NRF_STRERROR_ENTITY(NRF_ERROR_IO_PENDING),

    /* TWI error codes */
    NRF_STRERROR_ENTITY(NRF_ERROR_DRV_TWI_ERR_OVERRUN),
    NRF_STRERROR_ENTITY(NRF_ERROR_DRV_TWI_ERR_ANACK),
    NRF_STRERROR_ENTITY(NRF_ERROR_DRV_TWI_ERR_DNACK),

    /* IPSP error codes */
    NRF_STRERROR_ENTITY(NRF_ERROR_BLE_IPSP_RX_PKT_TRUNCATED),
    NRF_STRERROR_ENTITY(NRF_ERROR_BLE_IPSP_CHANNEL_ALREADY_EXISTS),
    NRF_STRERROR_ENTITY(NRF_ERROR_BLE_IPSP_LINK_DISCONNECTED),
    NRF_STRERROR_ENTITY(NRF_ERROR_BLE_IPSP_PEER_REJECTED)
};


char const * nrf_strerror_get(ret_code_t code)
{
    char const * p_ret = nrf_strerror_find(code);
    return (p_ret == NULL) ? m_unknown_str : p_ret;
}

char const * nrf_strerror_find(ret_code_t code)
{
    nrf_strerror_desc_t const * p_start;
    nrf_strerror_desc_t const * p_end;
    p_start = nrf_strerror_array;
    p_end   = nrf_strerror_array + ARRAY_SIZE(nrf_strerror_array);

    while (p_start < p_end)
    {
        nrf_strerror_desc_t const * p_mid = p_start + ((p_end - p_start) / 2);
        ret_code_t mid_c = p_mid->code;
        if (mid_c > code)
        {
            p_end = p_mid;
        }
        else if (mid_c < code)
        {
            p_start = p_mid + 1;
        }
        else
        {
            return p_mid->name;
        }
    }
    return NULL;
}
