#include "bcm2837/rpi_uart1.h"
#include "uart1.h"
#include "exception.h"
#include "timer.h"
#include "irqtask.h"
#include "memory.h"
#include "syscall.h"
#include "scheduler.h"
#include "signal.h"

extern list_head_t *run_queue;
// extern thread_t* curr_thread;
extern char* sp;
void sync_64_router(trapframe_t *tpf) { 
    // uart_sendline("sp addr: %x\n", sp);

    // Basic #3 - Based on System Call Format in Video Player’s Test Program

    el1_interrupt_enable();   // Allow UART input during exception
    unsigned long long syscall_no = tpf->x8;

    //uart_sendline("syscall_no: %d\n", syscall_no);
    // uart_sendline("current pid: %d\n", curr_thread->pid);

    if (syscall_no == 0)       { getpid(tpf);                                                                 }
    else if(syscall_no == 1)   { uartread(tpf,(char *) tpf->x0, tpf->x1);                                     }
    else if (syscall_no == 2)  { uartwrite(tpf,(char *) tpf->x0, tpf->x1);                                    }
    else if (syscall_no == 3)  { exec(tpf,(char *) tpf->x0, (char **)tpf->x1);                                }
    else if (syscall_no == 4)  { fork(tpf);                                                                   }
    else if (syscall_no == 5)  { exit(tpf,tpf->x0);                                                           }
    else if (syscall_no == 6)  { syscall_mbox_call(tpf,(unsigned char)tpf->x0, (unsigned int *)tpf->x1);      }
    else if (syscall_no == 7)  { kill(tpf, (int)tpf->x0);                                                     }
    else if (syscall_no == 8)  { signal_register(tpf->x0, (void (*)())tpf->x1);                               }
    else if (syscall_no == 9)  { signal_kill(tpf->x0, tpf->x1);                                               }
    else if (syscall_no == 50) { sigreturn(tpf);                                                              }
}

void irq_router(trapframe_t *tpf){ //有可能是el0被中斷或是el1被中斷,所以合併處理
    // decouple the handler into irqtask queue
    // (1) https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf - Pg.113
    // (2) https://datasheets.raspberrypi.com/bcm2836/bcm2836-peripherals.pdf - Pg.16
    if(*IRQ_PENDING_1 & IRQ_PENDING_1_AUX_INT && *CORE0_INTERRUPT_SOURCE & INTERRUPT_SOURCE_GPU) // from aux && from GPU0 -> uart exception
    {
        if (*AUX_MU_IIR_REG & (0b01 << 1))
        {
            *AUX_MU_IER_REG &= ~(2);  // disable write interrupt
            irqtask_add(uart_w_irq_handler, UART_IRQ_PRIORITY);
            irqtask_run_preemptive();
        }
        else if (*AUX_MU_IIR_REG & (0b10 << 1))
        {
            *AUX_MU_IER_REG &= ~(1);  // disable read interrupt
            irqtask_add(uart_r_irq_handler, UART_IRQ_PRIORITY);
            irqtask_run_preemptive();
        }
    }
    else if(*CORE0_INTERRUPT_SOURCE & INTERRUPT_SOURCE_CNTPNSIRQ)  //from CNTPNS (core_timer) // A1 - setTimeout run in el1
    {
        core_timer_disable();
        irqtask_add(core_timer_handler, TIMER_IRQ_PRIORITY);
        irqtask_run_preemptive();
        core_timer_enable();

        //at least two thread running -> schedule for any timer irq
        if (run_queue->next->next != run_queue) schedule();
    }

    //only do signal handler when return to user mode
    if ((tpf->spsr_el1 & 0b1100) == 0) { check_signal(tpf); } //當 M[3:2] 等於 00 時，異常是從 EL0 處理器模式被取的，並且處理器在返回時將返回到 EL0 模式
}


void invalid_exception_router(unsigned long long x0){
    // TBD
    //uart_sendline("invalid exception : 0x%x\r\n",x0);
    //while(1);
}

static unsigned long long lock_count = 0;
void lock()
{
    el1_interrupt_disable();
    lock_count++;
}

void unlock()
{
    lock_count--;
    if (lock_count == 0)
        el1_interrupt_enable();
}