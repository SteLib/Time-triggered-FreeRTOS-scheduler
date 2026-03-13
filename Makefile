# --- Toolchain ---
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc

# --- Configuration ---

FREERTOS_DIR = FreeRTOS/FreeRTOS/Source

MACH = cortex-m3

# --- Compiler Flags ---
# -c: Compile only, do not link (equivalent to your previous lab commands)
# -I: Tells the compiler where to look for header files (.h)
CFLAGS = -c -mcpu=$(MACH) -mthumb -g3 -O0 -Wall
CFLAGS += -I. 
CFLAGS += -I$(FREERTOS_DIR)/include 
CFLAGS += -I$(FREERTOS_DIR)/portable/GCC/ARM_CM3

# --- Linker Flags ---
# -T: Specifies your custom linker script
LDFLAGS = -mcpu=$(MACH) -mthumb -T mps2_m3.ld -nostartfiles

# --- Source Files ---
# Your application files
SRCS = main.c startup.c uart.c

# FreeRTOS core kernel files
SRCS += $(FREERTOS_DIR)/tasks.c 
SRCS += $(FREERTOS_DIR)/queue.c 
SRCS += $(FREERTOS_DIR)/list.c 

# FreeRTOS port and heap implementation
SRCS += $(FREERTOS_DIR)/portable/GCC/ARM_CM3/port.c
SRCS += $(FREERTOS_DIR)/portable/MemMang/heap_4.c

# --- Object Files ---
# Automatically replaces the .c extension with .o for all source files
OBJS = $(SRCS:.c=.o)

# Final executable name
TARGET = main.elf

# --- Build Rules ---

all: $(TARGET)

# Rule to compile any .c file into a .o file
%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

# Rule to link all .o files into the final .elf file
$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# Rule to clean up compiled files
clean:
	rm -f $(OBJS) $(TARGET)

# Rule to run the QEMU simulation
run: $(TARGET)
	qemu-system-arm -M mps2-an385 -cpu $(MACH) -m 16M -nographic -kernel $(TARGET)