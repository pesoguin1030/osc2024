#include "dtb.h"
#include "uart.h"
#include "exception.h"
#include "cpio.h"
#include "list.h"
#include "timer.h"
#include "irqtask.h"
#include "scheduler.h"
#include "mmu.h"
#include "mbox.h"
#include "filesystem/vfs.h"

extern char *dtb_base;



void main(char *arg)
{
    dtb_base = PHYS_TO_VIRT(arg);
    // init list
    init_idx();
    uart_init();
    task_list_init();
    fdt_traverse(initramfs_callback);
    init_allocator();
    // lab7
    init_rootfs();
    timer_list_init();
   
    enable_interrupt();
    enable_mini_uart_interrupt();
   
    init_thread_sched();
    core_timer_enable();
  

    uart_printf("Lab7 :\n");

    execfile("vfs1.img");
    //shell();
    //test_tmpfs_operations();
}