#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stddef.h>

#define MAX_TASKS 8

typedef enum {
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_SLEEPING,
    TASK_STATE_DEAD
} task_state_t;

typedef struct task {
    int pid;
    char name[32];
    uint64_t rsp;
    uint64_t stack[1024];
    task_state_t state;
    int priority;
    uint64_t time_ticks;
    uint32_t ticks_remaining;
    uint64_t wake_ticks;
    int is_user;
} task_t;

void task_init(void);
void timer_init(void);
int task_create(void (*entry)(void), const char* name, int priority);
int task_create_user(void (*entry)(void), const char* name, int priority);
void task_sleep(uint64_t ticks);
uint64_t schedule(uint64_t current_rsp);

uint64_t get_system_ticks(void);
int get_num_tasks(void);
task_t* get_task(int index);

#endif
