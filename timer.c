/**
 * @file timer.c
 * @brief Timer implementation using SDL_GetTicks()
 */

#include "timer.h"
#include <SDL.h>

/* ===================================================================
 *  Timer Implementation
 * =================================================================== */

void Timer_init(Timer *timer,
                uint32_t interval_ms,
                TimerCallback callback,
                void *user_data)
{
    if (timer == NULL) {
        LOG_ERR("Timer_init: timer is NULL\n");
        return;
    }

    timer->interval_ms = interval_ms;
    timer->callback    = callback;
    timer->user_data   = user_data;
    timer->state       = TIMER_STOPPED;
    timer->last_tick   = 0;

    LOG_DEBUG("Timer initialised: interval=%u ms, callback=%p, user_data=%p\n",
              interval_ms, (void *)callback, user_data);
}

void Timer_start(Timer *timer)
{
    if (timer == NULL) return;

    timer->last_tick = SDL_GetTicks();
    timer->state     = TIMER_RUNNING;

    LOG_DEBUG("Timer started (interval=%u ms)\n", timer->interval_ms);
}

void Timer_stop(Timer *timer)
{
    if (timer == NULL) return;

    timer->state = TIMER_STOPPED;

    LOG_DEBUG("Timer stopped\n");
}

void Timer_reset(Timer *timer)
{
    if (timer == NULL) return;

    timer->last_tick = SDL_GetTicks();

    LOG_DEBUG("Timer reset\n");
}

void Timer_tick(Timer *timer)
{
    if (timer == NULL) return;
    if (timer->state != TIMER_RUNNING) return;

    uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - timer->last_tick;

    if (elapsed >= timer->interval_ms) {
        /* Update last_tick — keep remainder to avoid drift */
        timer->last_tick = now - (elapsed % timer->interval_ms);

        /* Invoke the callback if one is registered */
        if (timer->callback != NULL) {
            timer->callback(timer->user_data);
        }
    }
}

uint32_t Timer_getElapsed(const Timer *timer)
{
    if (timer == NULL) return 0;

    return SDL_GetTicks() - timer->last_tick;
}

bool Timer_isRunning(const Timer *timer)
{
    return (timer != NULL) && (timer->state == TIMER_RUNNING);
}

uint64_t Timer_getMicroseconds(void)
{
    return SDL_GetPerformanceCounter() * 1000000ULL / SDL_GetPerformanceFrequency();
}

void Timer_delay(uint32_t ms)
{
    SDL_Delay(ms);
}
