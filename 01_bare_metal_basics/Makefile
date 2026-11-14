CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS = -mcpu=cortex-m4 -mthumb -Wall -Wextra -ffreestanding -g -O0
LDFLAGS = -T linker.ld -nostdlib

TARGET = firmware
SOURCES = main.c startup.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET).elf

$(TARGET).elf: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@