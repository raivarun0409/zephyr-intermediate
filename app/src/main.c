#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_DBG);

#define PRODUCER_PRIORITY   4
#define CONSUMER_PRIORITY   5
#define HEALTH_PRIORITY     6
#define HEALTH_PERIOD       200
#define PRODUCER_PERIOD     150
#define CONSUMER_PERIOD     250
#define STACK_SIZE          1024
#define USAGE_WARN_LEVEL    ((SENSOR_QUEUE_SIZE * 3) / 4)

#define SENSOR_QUEUE_SIZE   12

#define WDT_TIMEOUT_MS      1000
#define THREAD_STUCK_AFTER  20

struct sensor_data {
    int32_t data;
    uint32_t timestamp_ms;
    uint8_t seq;
};


K_MSGQ_DEFINE(sensor_queue, sizeof(struct sensor_data), SENSOR_QUEUE_SIZE, 4);

void wdt_callback(int channel_id, void *user_data)
{
    LOG_WRN("[WDT] THREAD STUCK! Channel: %d, name: %s, queue used: %u/%u",
            channel_id,
            k_thread_name_get((k_tid_t)user_data),
            k_msgq_num_used_get(&sensor_queue),
            SENSOR_QUEUE_SIZE);
}

//  Producer
static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    
    uint32_t seq = 0;
    k_thread_name_set(k_current_get(), "producer");

    while (1) {
        int ret = k_msgq_put(&sensor_queue, &seq, K_NO_WAIT);
        if (ret == 0) {
            LOG_INF("[PROD] seq=%u, used %u/%u",
                    seq,
                    k_msgq_num_used_get(&sensor_queue),
                    SENSOR_QUEUE_SIZE);
        }
        else {
            LOG_WRN("[PROD] queue full -> dropping seq=%u, used %u/%u",
                    seq,
                    k_msgq_num_used_get(&sensor_queue),
                    SENSOR_QUEUE_SIZE); 
        }
        seq++;
        k_msleep(PRODUCER_PERIOD);
    }

    LOG_INF("[PROD] done");
}

//  Consumer
static void consumer_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    uint32_t seq;
    uint32_t count = 0;

    k_thread_name_set(k_current_get(), "consumer");
    
    task_wdt_init(NULL);
    int wdt_chan = task_wdt_add(WDT_TIMEOUT_MS,
                    wdt_callback,
                    (void*)k_current_get()
                );

    if (wdt_chan < 0) {
        LOG_ERR("[CONSUMER] Watchdog channel not registered!");
    }

    while(1) {
        if (k_msgq_get(&sensor_queue, &seq, K_MSEC(250)) == 0) {
            LOG_INF("[CONSUMER] seq=%u", seq);
            count++;

            if ((count % THREAD_STUCK_AFTER) == 0) {
                task_wdt_feed(wdt_chan);
            }
        }
        k_msleep(CONSUMER_PERIOD);
    }

    LOG_INF("[CONSUMER] done, received=%d", count);
    task_wdt_delete(wdt_chan);
}


void health_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while(1) {
        uint32_t used = k_msgq_num_used_get(&sensor_queue);

        if (used >= USAGE_WARN_LEVEL) {
            LOG_WRN("[HEALTH] queue used: %u/%u (>=75%)",
                    used, SENSOR_QUEUE_SIZE);
        }
        else {
            LOG_INF("[HEALTH] queue used: %u/%u",
                    used, SENSOR_QUEUE_SIZE);
        }

        k_msleep(HEALTH_PERIOD);
    }
}


K_THREAD_DEFINE(sensor_thread, STACK_SIZE, sensor_thread_fn,
                NULL, NULL, NULL, PRODUCER_PRIORITY, 0, 0);

K_THREAD_DEFINE(consumer_thread, STACK_SIZE, consumer_thread_fn,
                NULL, NULL, NULL, CONSUMER_PRIORITY, 0, 0);

K_THREAD_DEFINE(health_thread, STACK_SIZE, health_fn,
                NULL, NULL, NULL, HEALTH_PRIORITY, 0, 0);


int main(void)
{
    LOG_INF("=== L5 HW: Resource Constraints and Reliability ===");
    LOG_INF("sensor produces every %dms", PRODUCER_PERIOD);

    return 0;
}
