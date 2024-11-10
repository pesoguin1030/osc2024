TOOLCHAIN_PREFIX = aarch64-linux-gnu-
#TOOLCHAIN_PREFIX = aarch64-none-elf-
CC = $(TOOLCHAIN_PREFIX)gcc
LD = $(TOOLCHAIN_PREFIX)ld
OBJCPY = $(TOOLCHAIN_PREFIX)objcopy

SRC_DIR = src
BUILD_DIR = build
INCLUDE = include

LINKER_FILE = $(SRC_DIR)/linker.ld

# Find all .c and .S files in SRC_DIR excluding bootloader directory
SRCS = $(shell find $(SRC_DIR) -type f -name '*.c' ! -path '$(SRC_DIR)/bootloader/*')
ASMS = $(shell find $(SRC_DIR) -type f -name '*.S' ! -path '$(SRC_DIR)/bootloader/*')

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
OBJS += $(ASMS:$(SRC_DIR)/%.S=$(BUILD_DIR)/%.o)

$(info SRCS: $(SRCS))
$(info OBJS: $(OBJS))

CFLAGS = -c -Wall -O2 -I$(INCLUDE) -nostdinc -nostdlib -nostartfiles -ffreestanding -fno-stack-protector

ifeq ($(DEBUG),1)
CFLAGS += -DDEBUG
endif

.PHONY: all clean asm run debug dirs

all: dirs kernel8.img

dirs: 
	if [ ! -d "$(BUILD_DIR)" ]; then mkdir "$(BUILD_DIR)"; fi

cpio:
	cd rootfs; find . | cpio -o -H newc > ../initramfs.cpio; cd ..
	
kernel8.img: $(OBJS) 
	$(LD) $(OBJS) -T $(LINKER_FILE) -o kernel8.elf
	$(OBJCPY) -O binary kernel8.elf kernel8.img

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $< -o $@

asm: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -d in_asm -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -drive if=sd,file=sfn_nctuos.img,format=raw

run: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -serial null -serial stdio -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -drive if=sd,file=sfn_nctuos.img,format=raw

run_with_display: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -serial null -serial stdio -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -drive if=sd,file=sfn_nctuos.img,format=raw

debug: all
	qemu-system-aarch64 -M raspi3b -kernel kernel8.img -display none -S -s -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -drive if=sd,file=sfn_nctuos.img,format=raw

clean:
	rm -f $(BUILD_DIR)/* kernel8.*
