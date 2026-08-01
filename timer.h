/**
 * @file timer.h
 * @brief Timer abstraction for sdl-player
 *
 * Provides a callback-based timer mechanism built on SDL_GetTicks().
 * The Timer object allows users to register a periodic callback that
 * is invoked at a specified interval (in milliseconds).
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>
#include "player_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===================================================================
 *  Type Definitions
 * =================================================================== */

/**
 * @brief Timer callback function type
 *
 * A callback registered with a Timer will be called whenever the
 * timer elapses. The callback receives an opaque user-data pointer.
 *
 * @param user_data  Opaque pointer supplied at callback registration
 */
typedef void (*TimerCallback)(void *user_data);

/**
 * @brief Timer state enumeration
 */
typedef enum {
    TIMER_STOPPED = 0,  /**< Timer is not running    */
    TIMER_RUNNING       /**< Timer is actively ticking */
} TimerState;

/* ===================================================================
 *  Timer Structure
 * =================================================================== */

/**
 * @brief Timer object
 *
 * Manages an interval timer backed by SDL_GetTicks(). The timer
 * calls the registered callback each time the interval elapses.
 *
 * Fields are publicly visible for transparency, but should be
 * modified only through the exported functions.
 */
typedef struct {
    uint32_t       interval_ms;   /**< Timer period in milliseconds        */
    uint32_t       last_tick;     /**< Timestamp (ms) of last trigger      */
    TimerState     state;         /**< Running / stopped                   */
    TimerCallback  callback;      /**< Function to call on each tick       */
    void          *user_data;     /**< User context passed to callback     */
} Timer;

/* ===================================================================
 *  Timer Functions
 * =================================================================== */

/**
 * @brief Initialise a Timer object
 *
 * Sets the interval and stores the callback / user data.
 * The timer does NOT start running; call Timer_start() to begin.
 *
 * @param timer        Pointer to a Timer object
 * @param interval_ms  Interval in milliseconds
 * @param callback     Callback function (may be NULL)
 * @param user_data    Opaque pointer passed to callback (may be NULL)
 */
void Timer_init(Timer *timer,
                uint32_t interval_ms,
                TimerCallback callback,
                void *user_data);

/**
 * @brief Start (or restart) the timer
 *
 * Resets the last-tick timestamp to the current SDL_GetTicks() value,
 * so the first callback will occur after a full interval.
 *
 * @param timer  Pointer to a Timer object
 */
void Timer_start(Timer *timer);

/**
 * @brief Stop the timer
 *
 * The callback will no longer be invoked until Timer_start() is called again.
 *
 * @param timer  Pointer to a Timer object
 */
void Timer_stop(Timer *timer);

/**
 * @brief Reset the timer
 *
 * Resets the last-tick timestamp to now, effectively restarting the
 * interval. The timer state is unchanged.
 *
 * @param timer  Pointer to a Timer object
 */
void Timer_reset(Timer *timer);

/**
 * @brief Update / tick the timer
 *
 * Should be called once per frame (or periodically). If the timer is
 * running and the interval has elapsed, the callback is invoked.
 * The user_data pointer is passed to the callback.
 *
 * @param timer  Pointer to a Timer object
 */
void Timer_tick(Timer *timer);

/**
 * @brief Get the elapsed time since the last timer tick
 *
 * @param timer  Pointer to a Timer object
 * @return       Milliseconds since last trigger
 */
uint32_t Timer_getElapsed(const Timer *timer);

/**
 * @brief Check whether the timer is currently running
 *
 * @param timer  Pointer to a Timer object
 * @return       true if running, false otherwise
 */
bool Timer_isRunning(const Timer *timer);

/**
 * @brief Get a high-resolution timestamp in microseconds
 *
 * Convenience wrapper around platform-specific high-res timer.
 * Uses SDL_GetPerformanceCounter() internally.
 *
 * @return  Timestamp in microseconds
 */
uint64_t Timer_getMicroseconds(void);

/**
 * @brief Busy-wait (spin) for a specified number of milliseconds
 *
 * @param ms  Milliseconds to delay
 */
void Timer_delay(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H */
