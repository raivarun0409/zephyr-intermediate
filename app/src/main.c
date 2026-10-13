#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

#define STACK_SIZE            2048
#define CONTROL_PRIORITY         7
#define MAINTENANCE_PRIORITY     8
#define EVENT_PERIOD_MS        250
#define MAINTENANCE_LOAD_US  45000
#define CONTROL_DEADLINE_US   10000

struct control_event {
    uint32_t seq;
    uint32_t ready_cycles;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);
static atomic_t deadline_misses;

/* ================================================================== */
/*  Timer expiry: creates one control event                           */
/* ================================================================== */

static void event_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    static uint32_t seq;
    struct control_event event = {
        .seq = seq++,
        .ready_cycles = k_cycle_get_32(),
    };

    /* Timer expiry runs in interrupt context, so never wait here. */
    int ret = k_msgq_put(&control_queue, &event, K_NO_WAIT);

    if (ret != 0) {
        atomic_inc(&deadline_misses);
        sys_trace_named_event("control_drop", event.seq,
                              atomic_get(&deadline_misses));
        LOG_WRN_RATELIMIT("[PRODUCER] dropped seq=%u deadline_misses=%d",
                          event.seq, atomic_get(&deadline_misses));
        return;
    }

    LOG_INF("[PRODUCER] released seq=%u ready_cycles=%u",
            event.seq, event.ready_cycles);
    sys_trace_named_event("control_release", event.seq, event.ready_cycles);

    /* Both threads become ready when the timer interrupt returns. */
    k_sem_give(&maintenance_start);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* ================================================================== */
/*  Control thread                                                   */
/* ================================================================== */

static void control_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (true) {
        struct control_event event;
        int ret = k_msgq_get(&control_queue, &event, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[CONTROL] receive failed: %d", ret);
            continue;
        }

        uint32_t response_cycles = k_cycle_get_32() - event.ready_cycles;
        uint32_t response_us = k_cyc_to_us_ceil32(response_cycles);
        bool missed_deadline =
            response_cycles > k_us_to_cyc_floor32(CONTROL_DEADLINE_US);
        atomic_val_t misses = atomic_get(&deadline_misses);

        if (missed_deadline) {
            misses = atomic_inc(&deadline_misses) + 1;
            LOG_WRN_RATELIMIT("[CONTROL] deadline miss seq=%u response_us=%u "
                              "deadline_us=%u total_misses=%d",
                              event.seq, response_us, CONTROL_DEADLINE_US, misses);
        }

        sys_trace_named_event("control_start", event.seq, response_us);
        LOG_INF("[CONTROL] started seq=%u response_us=%u deadline_us=%u result=%s "
                "deadline_misses=%d",
                event.seq, response_us, CONTROL_DEADLINE_US,
                missed_deadline ? "MISS" : "PASS", misses);
    }
}

/* ================================================================== */
/*  Background maintenance thread                                    */
/* ================================================================== */

static void maintenance_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (true) {
        k_sem_take(&maintenance_start, K_FOREVER);

        /* This work is important, but it has no short deadline. */
        k_busy_wait(MAINTENANCE_LOAD_US);
    }
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn,
                NULL, NULL, NULL, CONTROL_PRIORITY, 0, 0);

K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn,
                NULL, NULL, NULL, MAINTENANCE_PRIORITY, 0, 0);

int main(void)
{
    k_thread_name_set(control, "control");
    k_thread_name_set(maintenance, "maintenance");

    LOG_INF("=== L6 Homework: Runtime Investigation ===");
    LOG_INF("Control start response-time guarantee: <= %u us", CONTROL_DEADLINE_US);
    LOG_INF("Priorities: control=%d maintenance=%d (lower number is higher)",
            CONTROL_PRIORITY, MAINTENANCE_PRIORITY);

    k_timer_start(&event_timer, K_MSEC(500), K_MSEC(EVENT_PERIOD_MS));

    return 0;
}
