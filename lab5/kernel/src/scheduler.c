#include "scheduler.h"
#include "uart1.h"
#include "exception.h"
#include "memory.h"
#include "timer.h"
#include "signal.h"

list_head_t *run_queue;

thread_t threads[PIDMAX + 1];
thread_t *curr_thread;

int pid_history = 0;

void init_thread_scheduler()
{
    //lock();
    // init thread freelist and run_queue
    run_queue = kmalloc(sizeof(list_head_t));
    INIT_LIST_HEAD(run_queue);

    //init pids
    for (int i = 0; i <= PIDMAX; i++)
    {
        threads[i].isused = 0;
        threads[i].pid = i;
        threads[i].iszombie = 0;
    }

    asm volatile("msr tpidr_el1, %0" ::"r"(kmalloc(sizeof(thread_t)))); // Don't let thread structure NULL as we enable the functionality

    thread_t* idlethread = thread_create(idle);
    curr_thread = idlethread;
    //unlock();
}

void idle(){
    while(1)
    {
        kill_zombies();   //reclaim threads marked as DEAD
        // uart_sendline("Idle thread\n");
        schedule();       //switch to next thread in run queue
    }
}

void kill_zombies(){
    lock();
    list_head_t *curr;
    list_for_each(curr,run_queue)
    {
        if (((thread_t *)curr)->iszombie)
        {
            list_del_entry(curr);
            kfree(((thread_t *)curr)->stack_alloced_ptr);        // free stack
            kfree(((thread_t *)curr)->kernel_stack_alloced_ptr); // free stack
            //kfree(((thread_t *)curr)->data);                   // Don't free data because children may use data
            ((thread_t *)curr)->iszombie = 0;
            ((thread_t *)curr)->isused = 0;
        }
    }
    unlock();
}

void schedule(){
    lock();
    do {
        curr_thread = (thread_t *)curr_thread->listhead.next;
    } while (list_is_head(&curr_thread->listhead, run_queue) || ((thread_t*)curr_thread)->iszombie); // find a runnable thread
    // int a = 10;
    // int b = 20;
    // int c = 30;
    // int d = 40;
    // int e = 50;
    // int f = 60;
    // int g = 70;
    // int h = 80;
    // int i = 90;
    // int j = 100;
    // int sum = a + b + c + d + e + f + g + h + i + j;
    switch_to(get_current(), &curr_thread->context); //把tpidr_el1的值存到x0, curr_thread->context(下一個thread)的context存到x1
    unlock();
}


thread_t *thread_create(void *start)
{
    lock();
    thread_t *r;
    // find usable PID, don't use the previous one
    // if( pid_history > PIDMAX ) return 0;
    // if (!threads[pid_history].isused){
    //     r = &threads[pid_history];
    //     pid_history += 1;
    // }
    // else return 0;

    for (int i = 0; i <= PIDMAX; i++)
    {
        if (!threads[i].isused)
        {
            r = &threads[i];
            break;
        }
    }

    r->iszombie = 0;
    r->isused = 1;
    r->context.lr = (unsigned long long)start;
    r->stack_alloced_ptr = kmalloc(USTACK_SIZE);
    r->kernel_stack_alloced_ptr = kmalloc(KSTACK_SIZE);
    r->context.sp = (unsigned long long )r->stack_alloced_ptr + USTACK_SIZE;
    r->context.fp = r->context.sp; // frame pointer for local variable, which is also in stack.

    r->signal_running = 0;
    //initial all signal handler with signal_default_handler (kill thread)
    for (int i = 0; i < SIGNAL_MAX;i++)
    {
        r->signal_handler[i] = signal_default_handler; //64種signal的function array
        r->sigcount[i] = 0; //之後若有signal，則將其設為1
    }
    list_add(&r->listhead, run_queue);
    unlock();
    return r;
}


int exec_thread(char *data, unsigned int filesize)
{
    thread_t *t = thread_create(data); //thread_test
    t->data = kmalloc(filesize);
    t->datasize = filesize;
    t->context.lr = (unsigned long)t->data; // set return address to program if function call completes
    // copy file into data
    for (int i = 0; i < filesize;i++)
    {
        t->data[i] = data[i];
    }

    curr_thread = t;
    add_timer(schedule_timer, 1, "", 0); // start scheduler
    // eret to exception level 0
    asm("msr tpidr_el1, %0\n\t" // Hold the "kernel(el1)" thread structure information
        "msr elr_el1, %1\n\t"   // When el0 -> el1, store return address for el1 -> el0
        "msr spsr_el1, xzr\n\t" // Enable interrupt in EL0 -> Used for thread scheduler, the CPU state of user mode
        "msr sp_el0, %2\n\t"    // el0 stack pointer for el1 process
        "mov sp, %3\n\t"        // sp is reference for the same el process. For example, el2 cannot use sp_el2, it has to use sp to find its own stack.
        "eret\n\t" ::"r"(&t->context),"r"(t->context.lr), "r"(t->context.sp), "r"(t->kernel_stack_alloced_ptr + KSTACK_SIZE));

    return 0;
}

void thread_exit(){
    // thread cannot deallocate the stack while still using it, wait for someone to recycle it.
    // In this lab, idle thread handles this task, instead of parent thread.
    lock();
    curr_thread->iszombie = 1;
    unlock();
    schedule();
}

void schedule_timer(char* notuse){
    unsigned long long cntfrq_el0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0));
    add_timer(schedule_timer, cntfrq_el0 >> 5, "", 1); //e.g.3200000(tick/s)/32=100000(tick)
    // 32 * default timer -> trigger next schedule timer
}

void foo(){
    // Lab5 Basic 1 Test function
    for (int i = 0; i < 10; ++i)
    {
        uart_sendline("Thread id: %d %d\n", curr_thread->pid, i);
        int r = 1000000;
        while (r--) { asm volatile("nop"); }
        schedule();
    }
    thread_exit();
}
