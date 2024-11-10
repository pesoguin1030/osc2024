#include "filesystem/vfs.h"
#include "filesystem/tmpfs.h"
#include "string.h"
#include "scheduler.h"
extern thread_t* curr_thread;
void test_tmpfs_operations()
{
    // First, ensure the UART device is correctly opened and mapped to the standard output file descriptor
    vfs_open("/dev/uart", 0, &curr_thread->fdt[1]);

    // Step 1: Create test directory
    if (vfs_mkdir("/test") != 0)
    {
        vfs_write(curr_thread->fdt[1], "Failed to create /test directory\n", 33);
        return;
    }

    // Step 2: Create test1.txt file in test directory
    struct file* file1;
    if (vfs_open("/test/test1.txt", O_CREAT, &file1) != 0)
    {
        vfs_write(curr_thread->fdt[1], "Failed to create /test/test1.txt file\n", 36);
        return;
    }

    // Step 3: Write initial text to test1.txt
    const char* initial_text = "Hello, this is test1!";
    if (vfs_write(file1, initial_text, strlen(initial_text)) != strlen(initial_text))
    {
        vfs_write(curr_thread->fdt[1], "Failed to write initial text to /test/test1.txt\n", 47);
        vfs_close(file1);
        return;
    }

    // Step 4: Read and print initial text from test1.txt
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    vfs_lseek64(file1, 0, SEEK_SET);
    if (vfs_read(file1, buffer, strlen(initial_text)) != strlen(initial_text))
    {
        vfs_write(curr_thread->fdt[1], "Failed to read initial text from /test/test1.txt\n", 48);
        vfs_close(file1);
        return;
    }
    vfs_write(curr_thread->fdt[1], "Initial text in /test/test1.txt: ", 34);
    vfs_write(curr_thread->fdt[1], buffer, strlen(buffer));
    vfs_write(curr_thread->fdt[1], "\n", 1);

    // Step 5: Close test1.txt
    vfs_close(file1);

    // Step 6: Reopen test1.txt, move pointer to beginning, and write new text
    if (vfs_open("/test/test1.txt", 0, &file1) != 0)
    {
        vfs_write(curr_thread->fdt[1], "Failed to reopen /test/test1.txt file\n", 38);
        return;
    }
    const char* new_text = "New text for test1.";
    vfs_lseek64(file1, 0, SEEK_SET);
    if (vfs_write(file1, new_text, strlen(new_text)) != strlen(new_text))
    {
        vfs_write(curr_thread->fdt[1], "Failed to write new text to /test/test1.txt\n", 44);
        vfs_close(file1);
        return;
    }

    // Step 7: Read and print new text from test1.txt
    memset(buffer, 0, sizeof(buffer));
    vfs_lseek64(file1, 0, SEEK_SET);
    if (vfs_read(file1, buffer, strlen(new_text)) != strlen(new_text))
    {
        vfs_write(curr_thread->fdt[1], "Failed to read new text from /test/test1.txt\n", 44);
        vfs_close(file1);
        return;
    }
    vfs_write(curr_thread->fdt[1], "New text in /test/test1.txt: ", 29);
    vfs_write(curr_thread->fdt[1], buffer, strlen(buffer));
    vfs_write(curr_thread->fdt[1], "\n", 1);

    // Step 8: Close test1.txt
    vfs_close(file1);
}