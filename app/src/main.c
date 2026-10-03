#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

#define PRIO_COOP -1
#define PRIO_HIGH 3
#define PRIO_MED 5
#define PRIO_LOW 7

void t_low_fn(void *p1, void *p2, void *p3)
{
    int count = 0;
    while (1) {
        LOG_INF("T_LOW running %d", count);
        count++;
        k_msleep(300);
    }
}

void t_med_fn(void *p1, void *p2, void *p3)
{
    int count = 0;
    while (1) {
        LOG_INF("T_MED running %d", count);
        count++;
        k_msleep(200);
    }
}

void t_high_fn(void *p1, void *p2, void *p3)
{
    int count = 0;
    while (1) {
        LOG_INF("T_HIGH running %d", count);
        count++;
        k_msleep(100);
    }
}

void t_coop_fn(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < 5; i = i + 1)
    {
        LOG_INF("Doing busy work");
    }
    k_yield();
}


K_THREAD_DEFINE(thread_low, STACK_SIZE, t_low_fn,
                NULL, NULL, NULL, PRIO_LOW, 0, 0);
K_THREAD_DEFINE(thread_med, STACK_SIZE, t_med_fn,
                NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(thread_high, STACK_SIZE, t_high_fn, // runs the most because of priority
                NULL, NULL, NULL, PRIO_HIGH, 0, 0);

K_THREAD_DEFINE(thread_coop, STACK_SIZE, t_coop_fn,
                NULL, NULL, NULL, PRIO_COOP, 0, 0);

int main(void)
{
    return 0;
}
