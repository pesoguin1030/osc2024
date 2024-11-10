#include "filesystem/vfs.h"
#include "filesystem/tmpfs.h"
#include "malloc.h"
#include "string.h"
#include "filesystem/initramfs.h"
#include "filesystem/dev_uart.h"
#include "filesystem/dev_framebuffer.h"

struct mount* rootfs;
struct filesystem reg_fs[MAX_FS_REG];
struct file_operations reg_dev[MAX_DEV_REG];

int register_filesystem(struct filesystem* fs)
{
    for (int i = 0; i < MAX_FS_REG; i++)
    {
        if (!reg_fs[i].name)
        {
            reg_fs[i].name = fs->name;
            reg_fs[i].setup_mount = fs->setup_mount;
            return i;
        }
    }
    return -1;
}

int register_dev(struct file_operations* fo)
{
    // register device's file operations
    for (int i = 0; i < MAX_FS_REG; i++)
    {
        //if (!reg_dev[i].open) 確保只有在所有必需操作都未註冊的情況下，才會註冊新的設備操作。
        if (!reg_dev[i].open && !reg_dev[i].read && !reg_dev[i].write &&!reg_dev[i].close)
        {
            reg_dev[i] = *fo;
            return i;
        }
    }
    return -1;
}

struct filesystem* find_filesystem(const char* fs_name)
{
    for (int i = 0; i < MAX_FS_REG; i++)
    {
        if (strcmp(reg_fs[i].name, fs_name) == 0)
            return &reg_fs[i];
    }
    return 0;
}

int vfs_open(const char* pathname, int flags, struct file** target)
{
    // 1. Lookup pathname
    // 3. Create a new file if O_CREAT is specified in flags and vnode not found
    struct vnode* node;
    if (vfs_lookup(pathname, &node) != 0 && (flags & O_CREAT))
    {
        int last_slash_idx = 0;
        // get last slash
        for (int i = 0; i < strlen(pathname); i++)
            if (pathname[i] == '/')
                last_slash_idx = i;

        // dir name
        char dirname[MAX_PATH_NAME + 1];
        strcpy(dirname, pathname);
        dirname[last_slash_idx] = 0;
        // find parent dir
        if (vfs_lookup(dirname, &node) != 0)
        {
            uart_printf("cannot ocreate no dir name\n");
            return -1;
        }
        // create file by calling create operation
        node->v_ops->create(node, &node, pathname + last_slash_idx + 1);
        // create file handler
        *target = malloc(sizeof(struct file));
        node->f_ops->open(node, target);
        (*target)->flags = flags;
        return 0;
    } else // 2. Create a new file handle for this vnode if found.
    {
        // file exists
        *target = malloc(sizeof(struct file));
        node->f_ops->open(node, target);
        (*target)->flags = flags;
        return 0;//如果找到了對應的節點，vfs_lookup會返回0
    }

    // lookup error code shows if file exist or not or other error occurs
    // 4. Return error code if fails
    return -1;
}

int vfs_close(struct file* file)
{
    // 1. release the file handle
    // 2. Return error code if fails
    file->f_ops->close(file);
    return 0;
}

int vfs_write(struct file* file, const void* buf, size_t len)
{
    // 1. write len byte from buf to the opened file.
    // 2. return written size or error code if an error occurs.
    return file->f_ops->write(file, buf, len);
}

int vfs_read(struct file* file, void* buf, size_t len)
{
    // 1. read min(len, readable size) byte to buf from the opened file.
    // 2. block if nothing to read for FIFO type
    // 2. return read size or error code if an error occurs.
    return file->f_ops->read(file, buf, len);
}

int vfs_mkdir(const char* pathname)
{
    // e.g : /a/b/c
    // dirname : /a/b
    // newdirname : c
    char dirname[MAX_PATH_NAME] = {};
    char newdirname[MAX_PATH_NAME] = {};

    // split pathname
    int last_slash_idx = 0;
    for (int i = 0; i < strlen(pathname); i++)
        if (pathname[i] == '/') //透過迴圈找出路徑名稱中最後一個斜線（/）的位置，並將其索引值儲存到 last_slash_idx
            last_slash_idx = i;

    memcpy(dirname, pathname, last_slash_idx);//使用 memcpy 函數將路徑名稱中最後一個斜線之前的部分複製到 dirname
    strcpy(newdirname, pathname + last_slash_idx + 1);//strcpy 函數將路徑名稱中最後一個斜線之後的部分複製到 newdirname

    struct vnode* node;
    // find corresponding vnode and call mkdir
    if (vfs_lookup(dirname, &node) == 0) //使用 vfs_lookup 函數尋找 dirname 對應的 vnode。如果找到，則呼叫該 vnode 的 mkdir 方法來建立新目錄，並回傳 0 表示成功
    {
        node->v_ops->mkdir(node, &node, newdirname);
        return 0;
    }

    uart_printf("vfs_mkdir cannot find pathname");//如果 vfs_lookup 函數找不到對應的 vnode，則輸出錯誤訊息並回傳 -1 表示失敗。
    return -1;
}

int vfs_mount(const char* target, const char* filesystem)
{
    struct vnode* dirnode;
    // find register filesystem

    struct filesystem* fs = find_filesystem(filesystem);
    if (!fs)
    {
        uart_printf("vfs_mount cannot find filesystem\n");
        return -1;
    }
    // find mount point
    if (vfs_lookup(target, &dirnode) == -1)
    {
        uart_printf("vfs_mount cannot find dir\n");
        return -1;
    } else
    {
        /* notice :
            setup_mount will new a vnode (target duplication) with different file_operation
            struct mount of taget's vnode will point to it
            when lookup switch filesystem will jump to new vnode
        */
        dirnode->mount = malloc(sizeof(struct mount));
        // filesystem setup mount
        fs->setup_mount(fs, dirnode->mount);
    }
    return 0;
}

int vfs_lookup(const char* pathname, struct vnode** target)
{
    if (strlen(pathname) == 0) //如果路徑名稱為空，則將目標 vnode 設定為根檔案系統的根節點，並回傳 0
    {
        *target = rootfs->root;
        return 0;
    }

    struct vnode* dirnode = rootfs->root; //初始化一個指向根節點的 vnode 指標 dirnode，以及一個用於存儲路徑組件名稱的字元陣列 component_name
    char component_name[FILE_NAME_MAX + 1] = {};
    int c_idx = 0;
    // iterate through directory
    // e.g: lookup "a/b/c"
    /*
        different vnode may have different vnode operation
        use rootfs->lookup find vnode of a
        use a->lookup find vnode of b
        use b->lookup find vnode of c
    */
    for (int i = 1; i < strlen(pathname); i++)
    {
        if (pathname[i] == '/') //透過迴圈處理路徑名稱的每一個字元。如果遇到斜線（/），則認為已經找到一個完整的組件名稱。
        {
            component_name[c_idx++] = 0;
            // use parent vnode lookup to file vnode
            if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0)//使用當前目錄節點的 lookup 函數查找該組件名稱對應的 vnode。如果找不到，則回傳 -1。
                return -1;

            // redirect to new mounted filesystem
            if (dirnode->mount) //如果當前節點有掛載的檔案系統，則將 dirnode 設定為該檔案系統的根節點。
                dirnode = dirnode->mount->root;

            c_idx = 0;
        } else
        {
            component_name[c_idx++] = pathname[i];
        }
    }

    //在迴圈結束後，對最後一個組件名稱進行查找。
    component_name[c_idx++] = 0;
    if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0)
        return -1;
    // redirect to new mounted filesystem
    while (dirnode->mount)
        dirnode = dirnode->mount->root;

    //將找到的 vnode 存入 target，並回傳 0。
    // pass vnode
    *target = dirnode;

    return 0;
}

int vfs_mknod(char* pathname, int id)
{
    // device file according to device id
    struct file* f = malloc(sizeof(struct file));
    // create file
    vfs_open(pathname, O_CREAT, &f);
    // assign registered file operation to vnode file operation
    // according register device id
    f->vnode->f_ops = &reg_dev[id];
    //f->vnode->type = DEV;
    vfs_close(f);
    return 0;
}

long vfs_lseek64(struct file* file, long offset, int whence)
{
    // assign file position to get offset
    if (whence == SEEK_SET)
    {
        file->f_pos = offset;
        return file->f_pos;
    }
    return -1;
}

int vfs_create(const char* pathname)
{
    struct file* f;
    return vfs_open(pathname, O_CREAT, &f);
}


// void parse_directory_structure(struct vnode *node, int level) {
//   if (!node)
//     return;
//   for (int i = 0; i < level; ++i) {
//     uart_printf("|   ");
//   }
//   if (node->type == TMP)
//   {
//       uart_printf("11111111111111\n");
//     struct tmpfs_inode *inode = (struct tmpfs_inode *)node->internal;
//     uart_printf("|-- %s", inode->name);
//     if (inode->type == dir_t) {
//       uart_printf(" {\n");
//       if (node->mount && node->mount->root) {
//         if (node->mount->root->type == TMP) {
//           for (int i = 0; i <= MAX_DIR_ENTRY; ++i) {
//             struct vnode *child =
//                 ((struct tmpfs_inode *)rootfs->root->internal)->entry[i];
//             if (child) {
//               parse_directory_structure(child, level + 1);
//             }
//           }
//         } else {
//           for (int i = 0; i < INITRAMFS_MAX_DIR_ENTRY; ++i) {
//             struct vnode *child =
//                 ((struct initramfs_inode *)node->mount->root->internal)->entry[i];
//             if (child) {
//               parse_directory_structure(child, level + 1);
//             }
//           }
//         }
//       } else {
//         for (int i = 0; i <= MAX_DIR_ENTRY; ++i) {
//           struct vnode *child = inode->entry[i];
//           if (child) {
//             parse_directory_structure(child, level + 1);
//           }
//         }
//       }
//       for (int i = 0; i <= level; ++i) {
//         uart_printf("|   ");
//       }
//       uart_printf("}\n");
//     } else {
//       uart_printf("\n");
//     }
//   } else if (node->type == INITRAM)
//   {
//       uart_printf("222222222222222\n");
//     struct initramfs_inode *inode = (struct initramfs_inode *)node->internal;
//     uart_printf("|-- %s", inode->name);
//     if (inode->type == dir_t) {
//       uart_printf(" {\n");
//       for (int i = 0; i < INITRAMFS_MAX_DIR_ENTRY; ++i) {
//         struct vnode *child = inode->entry[i];
//         if (child) {
//           parse_directory_structure(child, level + 1);
//         }
//       }
//       for (int i = 0; i <= level; ++i) {
//         uart_printf("|   ");
//       }
//       uart_printf("}\n");
//     } else {
//       uart_printf("\n");
//     }
// //   } else if (node->type == DEV)
// //   {
// //       struct initramfs_inode *inode = (struct initramfs_inode *)node->internal;
// //     uart_printf("|-- %s", inode->name);
// //     if (inode->type == dir_t) {
// //       uart_printf(" {\n");
// //       for (int i = 0; i < INITRAMFS_MAX_DIR_ENTRY; ++i) {
// //         struct vnode *child = inode->entry[i];
// //         if (child) {
// //           parse_directory_structure(child, level + 1);
// //         }
// //       }
// //       for (int i = 0; i <= level; ++i) {
// //         uart_printf("|   ");
// //       }
// //       uart_printf("}\n");
// //     } else {
// //       uart_printf("\n");
// //     }
//   }else
//   {
//       uart_printf("[parse_directory_structure] Unknown vnode type.\n");
//   }
// }

// void parse_rootfs()
// {
//   uart_printf("/ {\n");
//   for (int i = 0; i <= MAX_DIR_ENTRY; ++i) {
//     struct vnode *child = ((struct tmpfs_inode *)rootfs->root->internal)->entry[i];
//     if (child) {
//       parse_directory_structure(child, 0);
//     }
//   }
//   uart_printf("}\n");
// }

void init_rootfs()
{

    int idx = register_tmpfs();
    // init rootfs
    rootfs = malloc(sizeof(struct mount));
    reg_fs[idx].setup_mount(&reg_fs[idx], rootfs);
    //註冊完成後，init_rootfs 函數會調用 tmpfs_setup_mount 來設置掛載點並創建根目錄的 vnode。這一步的主要目的是實際掛載 tmpfs 文件系統，並創建一個空的根目錄 vnode 作為文件系統的入口。

    // for initramfs
    vfs_mkdir("/initramfs");
    // assign filename and setup mount callback
    register_initramfs();
    vfs_mount("/initramfs", "initramfs");

    // for dev
    vfs_mkdir("/dev");
    // mknod according devive id
    // register dev uart
    int uart_id = init_dev_uart();
    vfs_mknod("/dev/uart", uart_id);

    // setting mailbox
    //int framebuffer_id = init_dev_framebuffer();
    //vfs_mknod("/dev/framebuffer", framebuffer_id);

    // vfs_mkdir("/home");
    // vfs_mkdir("/home/user");
    // vfs_mkdir("/home/user/docs");
    // vfs_create("/home/user/docs/file1");
    // vfs_create("/home/user/docs/file2");
    // vfs_create("/home/user/file3");
    // vfs_create("/home/file4");
    //while(parse_dir(rootfs,rootfs->root)!=1);
    //while(parse_dir(rootfs->root)!=1);
    //parse_rootfs();
}

// syscall filepath is relative path
// we need to use relative path and cwd to get absolute path
char* path_to_absolute(char* path, char* cwd)
{
    // relative path
    // append to cwd
    if (path[0] != '/')//檢查傳入的路徑 path 是否以斜線（/）開頭。如果不是，則認為這是一個相對路徑->如果 path 是相對路徑，則將當前工作目錄 cwd 和 path 組合成一個絕對路徑。
    {
        char tmp[MAX_PATH_NAME];
        strcpy(tmp, cwd);
        if (strcmp(cwd, "/") != 0)//如果 cwd 不是根目錄（/），則在 cwd 和 path 之間插入一個斜線。
            strcat(tmp, "/");
        strcat(tmp, path);
        strcpy(path, tmp);//將組合後的絕對路徑複製回 path 參數。
    }

    char absolute_path[MAX_PATH_NAME + 1] = {};//初始化一個用於存儲處理後的絕對路徑的字元陣列 absolute_path，以及一個用於追蹤 absolute_path 的當前位置的索引 idx。
    int idx = 0;
    // trim ".." and "."
    //
    for (int i = 0; i < strlen(path); i++)
    {
        // meet /..
        // return upper level
        if (path[i] == '/' && path[i + 1] == '.' && path[i + 2] == '.')
        {
            for (int j = idx; j >= 0; j--)
            {
                if (absolute_path[j] == '/')
                {
                    absolute_path[j] = 0;
                    idx = j;
                }
            }
            i += 2;
            continue;
        }

        // ignore /.
        if (path[i] == '/' && path[i + 1] == '.')
        {
            i++;
            continue;
        }

        absolute_path[idx++] = path[i];
    }

    absolute_path[idx] = 0;


    return strcpy(path, absolute_path);
}

int op_deny()
{
    return -1;
}