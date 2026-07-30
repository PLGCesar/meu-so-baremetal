#include "../include/task.h"
#include "../include/serial.h"

static task_t tasks[MAX_TASKS];
static int current_task = 0;
static int num_tasks = 0;
static uint32_t system_ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

void timer_init(void) {
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(11931 & 0xFF));
    outb(0x40, (uint8_t)((11931 >> 8) & 0xFF));
    serial_write("[TASK SCHEDULER] Timer PIT 10ms (100Hz) Inicializado!\n");
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

    uint32_t* stk = (uint32_t*)&t->stack[1024];

    *(--stk) = 0x0202;
    *(--stk) = 0x10;
    *(--stk) = (uint32_t)entry;

    *(--stk) = 0; // EAX
    *(--stk) = 0; // ECX
    *(--stk) = 0; // EDX
    *(--stk) = 0; // EBX
    *(--stk) = 0; // ESP
    *(--stk) = 0; // EBP
    *(--stk) = 0; // ESI
    *(--stk) = 0; // EDI

    *(--stk) = 0x18; // DS
    *(--stk) = 0x18; // ES
    *(--stk) = 0x18; // FS
    *(--stk) = 0x18; // GS

    t->esp = (uint32_t)stk;
    serial_write("[TASK SCHEDULER] Processo criado no Escalonador!\n");
    return id;
}

uint32_t schedule(uint32_t current_esp) {
    outb(0x20, 0x20);
    system_ticks++;

    if (num_tasks == 0) return current_esp;

    tasks[current_task].esp = current_esp;
    tasks[current_task].time_ticks++;

    if (tasks[current_task].ticks_remaining > 1) {
        tasks[current_task].ticks_remaining--;
        return current_esp;
    }

    int next_task = (current_task + 1) % num_tasks;
    while (tasks[next_task].state != TASK_STATE_READY && tasks[next_task].state != TASK_STATE_RUNNING) {
        next_task = (next_task + 1) % num_tasks;
    }

    tasks[current_task].state = TASK_STATE_READY;
    current_task = next_task;
    tasks[current_task].state = TASK_STATE_RUNNING;
    tasks[current_task].ticks_remaining = (4 - tasks[current_task].priority);

    return tasks[current_task].esp;
}

uint32_t get_system_ticks(void) { return system_ticks; }
int get_num_tasks(void) { return num_tasks; }
task_t* get_task(int index) { return &tasks[index]; }
