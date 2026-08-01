#include "../include/task.h"
#include "../include/serial.h"

static task_t tasks[MAX_TASKS];
static int current_task = 0;
static int num_tasks = 0;
static uint64_t system_ticks = 0; // CONTADOR DE 64-BITS DE ALTA PRECISÃO

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init(void) {
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(11931 & 0xFF));
    outb(0x40, (uint8_t)((11931 >> 8) & 0xFF));
    serial_write("[TASK SCHEDULER] Timer PIT 10ms (100Hz) Inicializado em 64-bits!\n");
}

void task_init(void) {
    num_tasks = 0;
    current_task = 0;
    system_ticks = 0;

    task_t* t = &tasks[0];
    t->pid = 1;
    t->state = TASK_STATE_RUNNING;
    t->priority = 1;
    t->ticks_remaining = 3;
    t->time_ticks = 0;

    const char* name = "Kernel_Main_64b";
    int i = 0;
    while (name[i] != '\0' && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    num_tasks = 1;
    timer_init();
}

int task_create(void (*entry)(void), const char* name, int priority) {
    if (num_tasks >= MAX_TASKS) return -1;

    int id = num_tasks++;
    task_t* t = &tasks[id];
    t->pid = id + 1;
    t->state = TASK_STATE_READY;
    t->priority = priority;
    t->ticks_remaining = (4 - priority);
    t->time_ticks = 0;

    int i = 0;
    while (name[i] != '\0' && i < 31) {
        t->name[i] = name[i];
        i++;
    }
    t->name[i] = '\0';

    uint64_t* stk = &t->stack[1024];

    *(--stk) = 0x10;
    uint64_t rsp_val = (uint64_t)stk;
    *(--stk) = rsp_val;
    *(--stk) = 0x0202;
    *(--stk) = 0x08;
    *(--stk) = (uint64_t)entry;

    for (int k = 0; k < 15; k++) *(--stk) = 0;

    t->rsp = (uint64_t)stk;
    serial_write("[TASK SCHEDULER] Processo de 64-bits registrado!\n");
    return id;
}

uint64_t schedule(uint64_t current_rsp) {
    outb(0x20, 0x20);
    system_ticks++;

    if (num_tasks == 0) return current_rsp;

    tasks[current_task].rsp = current_rsp;
    tasks[current_task].time_ticks++;

    if (tasks[current_task].ticks_remaining > 1) {
        tasks[current_task].ticks_remaining--;
        return current_rsp;
    }

    int next_task = (current_task + 1) % num_tasks;
    while (tasks[next_task].state != TASK_STATE_READY && tasks[next_task].state != TASK_STATE_RUNNING) {
        next_task = (next_task + 1) % num_tasks;
    }

    tasks[current_task].state = TASK_STATE_READY;
    current_task = next_task;
    tasks[current_task].state = TASK_STATE_RUNNING;
    tasks[current_task].ticks_remaining = (4 - tasks[current_task].priority);

    return tasks[current_task].rsp;
}

uint64_t get_system_ticks(void) { return system_ticks; }
int get_num_tasks(void) { return num_tasks; }
task_t* get_task(int index) { return &tasks[index]; }
