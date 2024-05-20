#include "irqtask.h"
#include "exception.h"
#include "memory.h"
#include "uart1.h"

int curr_task_priority = 9999;   // Small number has higher priority

struct list_head *task_list;
void irqtask_list_init()
{
    task_list = s_allocator(sizeof(list_head_t));
    INIT_LIST_HEAD(task_list);
}


void irqtask_add(void *task_function,unsigned long long priority){
    irqtask_t *the_task = s_allocator(sizeof(irqtask_t)); // free by irq_tasl_run_preemptive()

    // store all the related information into irqtask node
    // manually copy the device's buffer
    the_task->priority = priority;
    the_task->task_function = task_function;
    INIT_LIST_HEAD(&the_task->listhead);

    // add the timer_event into timer_event_list (sorted)
    // if the priorities are the same -> FIFO
    struct list_head *curr;

    // mask the device's interrupt line
    lock();
    // enqueue the processing task to the event queue with sorting.
    list_for_each(curr, task_list)
    {
        if (((irqtask_t *)curr)->priority > the_task->priority)
        {
            list_add(&the_task->listhead, curr->prev);
            break;
        }
    }
    // if the priority is lowest
    if (list_is_head(curr, task_list))
    {
        list_add_tail(&the_task->listhead, task_list);
    }
    // unmask the interrupt line
    unlock();
}

void irqtask_run_preemptive(){
    while (!list_empty(task_list))
    {
        // critical section protects new coming node
        lock();
        irqtask_t *the_task = (irqtask_t *)task_list->next;
        // Run new task (early return) if its priority is lower than the scheduled task.
        if (curr_task_priority <= the_task->priority)
        {
            unlock();
            break;
        }
        // get the scheduled task and run it.
        list_del_entry((struct list_head *)the_task);
        int prev_task_priority = curr_task_priority;
        curr_task_priority = the_task->priority;

        unlock();
        irqtask_run(the_task);
        lock();

        curr_task_priority = prev_task_priority;
        unlock();
        s_free(the_task);
    }
}

void irqtask_run(irqtask_t* the_task)
{
    ((void (*)())the_task->task_function)();
}