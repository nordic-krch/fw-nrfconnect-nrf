#include <app_timer.h>

static void app_timer_handler(struct k_timer *timer)
{
        app_timer_t *app_timer = CONTAINER_OF(timer, struct k_app_timer, timer);
        void *p_context = k_timer_user_data_get(timer);

        app_timer->handler(p_context);
}

ret_code_t app_timer_create(app_timer_id_t const *      p_timer_id,
                            app_timer_mode_t            mode,
                            app_timer_timeout_handler_t timeout_handler)
{
    (*p_timer_id)->mode = mode;
    (*p_timer_id)->handler = timeout_handler;
    k_timer_init(&(*p_timer_id)->timer, app_timer_handler, NULL);

    return NRF_SUCCESS;
}

ret_code_t app_timer_start(app_timer_id_t timer_id, uint32_t timeout_ticks, void * p_context)
{
    k_timeout_t t = { .ticks = timeout_ticks };
    k_timeout_t repeat = (timer_id->mode == APP_TIMER_MODE_SINGLE_SHOT) ? K_NO_WAIT : t;

    k_timer_user_data_set(&timer_id->timer, p_context);
    k_timer_start(&timer_id->timer, t, repeat);

    return NRF_SUCCESS;
}

ret_code_t app_timer_stop(app_timer_id_t timer_id)
{
        k_timer_stop(&timer_id->timer);
        return NRF_SUCCESS;
}
