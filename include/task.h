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
    uint32_t esp;
    uint32_t stack[1024];
    task_state_t state;
    int priority;           // 1 = Alta, 2 = Media, 3 = Baixa
    uint32_t time_ticks;
    uint32_t ticks_remaining;
} task_t;

void timer_init(void);
int task_create(void (*entry)(void), const char* name, int priority);
uint32_t schedule(uint32_t current_esp);

uint32_t get_system_ticks(void);
int get_num_tasks(void);
task_t* get_task(int index);

#endif
