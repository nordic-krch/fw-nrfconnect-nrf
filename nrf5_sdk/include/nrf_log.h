/*$$$LICENCE_NORDIC_STANDARD<2016>$$$*/
/**@file
 *
 * @defgroup nrf_log Logger module
 * @{
 * @ingroup app_common
 *
 * @brief The nrf_log module interface.
 */

#ifndef NRF_LOG_H_
#define NRF_LOG_H_

#include <zephyr/logging/log.h>


#ifdef __cplusplus
extern "C" {
#endif

/** @brief Severity level for the module.
 *
 * The severity level can be defined in a module to override the default.
 */
#ifndef NRF_LOG_LEVEL
    #define NRF_LOG_LEVEL NRF_LOG_DEFAULT_LEVEL
#endif

/** @brief Initial severity if filtering is enabled.
 */
#ifndef NRF_LOG_INITIAL_LEVEL
    #define NRF_LOG_INITIAL_LEVEL NRF_LOG_LEVEL
#endif


/** @def NRF_LOG_ERROR
 *  @brief Macro for logging error messages. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 */

/** @def NRF_LOG_WARNING
 *  @brief Macro for logging error messages. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes warning logs.
 */

/** @def NRF_LOG_INFO
 *  @brief Macro for logging error messages. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes info logs.
 */

/** @def NRF_LOG_DEBUG
 *  @brief Macro for logging error messages. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes debug logs.
 */

#define NRF_LOG_ERROR(...)                     LOG_ERR(__VA_ARGS__)
#define NRF_LOG_WARNING(...)                   LOG_WRN( __VA_ARGS__)
#define NRF_LOG_INFO(...)                      LOG_INF( __VA_ARGS__)
#define NRF_LOG_DEBUG(...)                     LOG_DBG( __VA_ARGS__)

/** @def NRF_LOG_INST_ERROR
 *  @brief Macro for logging error messages for a given module instance. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @param p_inst Pointer to the instance with logging support.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 */

/** @def NRF_LOG_INST_WARNING
 *  @brief Macro for logging error messages for a given module instance. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @param p_inst Pointer to the instance with logging support.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 */

/** @def NRF_LOG_INST_INFO
 *  @brief Macro for logging error messages for a given module instance. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @param p_inst Pointer to the instance with logging support.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 */

/** @def NRF_LOG_INST_DEBUG
 *  @brief Macro for logging error messages for given module instance. It takes a printf-like, formatted
 *  string with up to seven arguments.
 *
 *  @param p_inst Pointer to the instance with logging support.
 *
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 */
#define NRF_LOG_INST_ERROR(p_inst,...)         LOG_INST_ERR(p_inst,__VA_ARGS__)
#define NRF_LOG_INST_WARNING(p_inst,...)       LOG_INST_WRN(p_inst,__VA_ARGS__)
#define NRF_LOG_INST_INFO(p_inst,...)          LOG_INST_INF(p_inst, __VA_ARGS__)
#define NRF_LOG_INST_DEBUG(p_inst,...)         LOG_INST_DBG(p_inst, __VA_ARGS__)

/**
 * @brief Macro for logging a formatted string without any prefix or timestamp.
 */
#define NRF_LOG_RAW_INFO(...)                  LOG_RAW( __VA_ARGS__)

/** @def NRF_LOG_HEXDUMP_ERROR
 *  @brief Macro for logging raw bytes.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 *
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
/** @def NRF_LOG_HEXDUMP_WARNING
 *  @brief Macro for logging raw bytes.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes warning logs.
 *
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
/** @def NRF_LOG_HEXDUMP_INFO
 *  @brief Macro for logging raw bytes.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes info logs.
 *
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
/** @def NRF_LOG_HEXDUMP_DEBUG
 *  @brief Macro for logging raw bytes.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes debug logs.
 *
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
#define NRF_LOG_HEXDUMP_ERROR(p_data, len)   LOG_HEXDUMP_ERR(p_data, len, "")
#define NRF_LOG_HEXDUMP_WARNING(p_data, len) LOG_HEXDUMP_WRN(p_data, len, "")
#define NRF_LOG_HEXDUMP_INFO(p_data, len)    LOG_HEXDUMP_INF(p_data, len, "")
#define NRF_LOG_HEXDUMP_DEBUG(p_data, len)   LOG_HEXDUMP_DBG(p_data, len, "")

/** @def NRF_LOG_HEXDUMP_INST_ERROR
 *  @brief Macro for logging raw bytes for a specific module instance.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 *
 * @param p_inst     Pointer to the instance with logging support.
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
/** @def NRF_LOG_HEXDUMP_INST_WARNING
 *  @brief Macro for logging raw bytes for a specific module instance.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 *
 * @param p_inst     Pointer to the instance with logging support.
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
/** @def NRF_LOG_HEXDUMP_INST_INFO
 *  @brief Macro for logging raw bytes for a specific module instance.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 *
 * @param p_inst     Pointer to the instance with logging support.
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
/** @def NRF_LOG_HEXDUMP_INST_DEBUG
 *  @brief Macro for logging raw bytes for a specific module instance.
 *  @details This macro is compiled only if @ref NRF_LOG_LEVEL includes error logs.
 *
 * @param p_inst     Pointer to the instance with logging support.
 * @param p_data     Pointer to data.
 * @param len        Data length in bytes.
 */
#define NRF_LOG_HEXDUMP_INST_ERROR(p_inst, p_data, len)   LOG_INST_HEXDUMP_ERR(p_inst, p_data, len, "")
#define NRF_LOG_HEXDUMP_INST_WARNING(p_inst, p_data, len) LOG_INST_HEXDUMP_WRN(p_inst, p_data, len, "")
#define NRF_LOG_HEXDUMP_INST_INFO(p_inst, p_data, len)    LOG_INST_HEXDUMP_INF(p_inst, p_data, len, "")
#define NRF_LOG_HEXDUMP_INST_DEBUG(p_inst, p_data, len)   LOG_INST_HEXDUMP_DBG(p_inst, p_data, len, "")

/**
 * @brief Macro for logging hexdump without any prefix or timestamp.
 */
#define NRF_LOG_RAW_HEXDUMP_INFO(p_data, len)


/**
 * @brief Macro for copying a string to internal logger buffer if logs are deferred.
 *
 * @param _str  String.
 */
#define NRF_LOG_PUSH(_str)

/**
 * @brief Function for copying a string to the internal logger buffer if logs are deferred.
 *
 * Use this function to store a string that is volatile (for example allocated
 * on stack) or that may change before the deferred logs are processed. Such string is copied
 * into the internal logger buffer (see @ref NRF_LOG_STR_PUSH_BUFFER_SIZE).
 *
 * @note String storing is not reliable. It means that string is copied to the buffer but there is
 *       no indication when it was used and could be freed. String may be overwritten by another
 *       @ref nrf_log_push call before being processed. For reliable data dumping use
 *       hexdump macros (e.g. @ref NRF_LOG_HEXDUMP_INFO).
 *
 * @note If the logs are not deferred, then this function returns the input parameter.
 *
 * @param p_str Pointer to the user string.
 *
 * @return Address to the location where the string is stored in the internal logger buffer.
 */

/**
 * @brief Macro to be used in a formatted string to a pass float number to the log.
 *
 * Use this macro in a formatted string instead of the %f specifier together with
 * @ref NRF_LOG_FLOAT macro.
 * Example: NRF_LOG_INFO("My float number" NRF_LOG_FLOAT_MARKER "\r\n", NRF_LOG_FLOAT(f)))
 */
#define NRF_LOG_FLOAT_MARKER "%s%d.%02d"

/**
 * @brief Macro for dissecting a float number into two numbers (integer and residuum).
 */
#define NRF_LOG_FLOAT(val)

/**
 * @brief Macro for registering an independent module.
 *
 * Registration creates set of dynamic (RAM) and constant variables associated with the module.
 */
#define NRF_LOG_MODULE_REGISTER() LOG_MODULE_REGISTER(NRF_LOG_MODULE_NAME, NRF_LOG_LEVEL)


#ifdef __cplusplus
}
#endif

#endif // NRF_LOG_H_

/** @} */
