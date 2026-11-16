# FreeRTOS Scheduler Startup and QEMU Debugging

## 1. Objective

The objective of this milestone was to build and execute the FreeRTOS kernel on the ARM Cortex-M4F processor using QEMU.

The previous milestone covered:
- FreeRTOS kernel integration
- Cortex-M4F processor port
- Startup code and vector table
- Linker script
- FreeRTOSConfig.h

In this milestone, the focus was on:
- Building the complete FreeRTOS firmware.
- Verifying the generated ELF.
- Debugging the startup sequence using GDB.
- Understanding scheduler startup.
- Verifying Producer and Consumer task execution.
- Verifying actual queue communication.
- Understanding task states and scheduling behaviour.

## 2. FreeRTOS Makefile

A separate Makefile was created inside `02_freertos/`.

### 2.1 Compiler configuration

The project uses the ARM GCC cross-compiler:

```makefile
CC = arm-none-eabi-gcc

CPU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
```

Compiler flags:

```makefile
CFLAGS += $(CPU)
CFLAGS += -Wall -Wextra
CFLAGS += -g -O0
CFLAGS += -ffreestanding
CFLAGS += -fno-builtin
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
```

Purpose of important flags:

| Flag | Purpose |
|---|---|
| `-mcpu=cortex-m4` | Generates Cortex-M4 instructions |
| `-mthumb` | Generates Thumb instructions |
| `-mfpu=fpv4-sp-d16` | Selects the Cortex-M4F floating-point unit |
| `-mfloat-abi=softfp` | Uses the soft-float calling convention while allowing FPU instructions |
| `-Wall -Wextra` | Enables compiler warnings |
| `-g` | Generates debugging information |
| `-O0` | Disables optimization for easier debugging |
| `-ffreestanding` | Builds without assuming a hosted C environment |
| `-fno-builtin` | Prevents built-in substitutions for standard library functions |
| `-ffunction-sections` | Places functions into separate sections |
| `-fdata-sections` | Places data into separate sections |

### 2.2 Include paths

```makefile
CFLAGS += -I.
CFLAGS += -I../third_party/FreeRTOS-Kernel/include
CFLAGS += -I../third_party/FreeRTOS-Kernel/portable/GCC/ARM_CM4F
```

These paths allow the compiler to locate:
- Application configuration: `FreeRTOSConfig.h`
- FreeRTOS API headers: `FreeRTOS.h`, `task.h`, `queue.h`
- Cortex-M4F port headers: `portmacro.h`

### 2.3 Source files

```makefile
KERNEL_DIR = ../third_party/FreeRTOS-Kernel

SRC = \
    startup.c \
    main.c \
    $(KERNEL_DIR)/tasks.c \
    $(KERNEL_DIR)/queue.c \
    $(KERNEL_DIR)/list.c \
    $(KERNEL_DIR)/portable/GCC/ARM_CM4F/port.c \
    $(KERNEL_DIR)/portable/MemMang/heap_4.c
```

The initial application uses seven source files.

| Source file | Responsibility |
|---|---|
| `startup.c` | Reset handler and vector table |
| `main.c` | Application and task implementation |
| `tasks.c` | Task management and scheduler |
| `queue.c` | Queue implementation |
| `list.c` | Internal linked-list management |
| `port.c` | Cortex-M4F-specific processor port |
| `heap_4.c` | Dynamic memory allocation |

### 2.4 Linker configuration

```makefile
TARGET = firmware

LDFLAGS = $(CPU)
LDFLAGS += -Tlinker.ld
LDFLAGS += -Wl,-Map=$(TARGET).map
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -nostartfiles
LDFLAGS += -Wl,-e,Reset_Handler
```

Important options:

- `-Tlinker.ld`: Uses the custom linker script.
- `-Wl,-Map=firmware.map`: Generates a linker map file.
- `-Wl,--gc-sections`: Allows unused sections to be removed.
- `-nostartfiles`: Does not link the standard startup files.
- `-Wl,-e,Reset_Handler`: Explicitly specifies the firmware entry point.

### 2.5 Build rules

```makefile
OBJ = $(SRC:.c=.o)

all: $(TARGET).elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).map

.PHONY: all clean
```

Important Makefile concepts:

| Symbol | Meaning |
|---|---|
| `$<` | First prerequisite, normally the source file |
| `$@` | Target being generated |
| `$(SRC:.c=.o)` | Converts `.c` source filenames into `.o` filenames |
| `-c` | Compiles without linking |

## 3. FreeRTOS Configuration Issues During Build

The first compilation identified missing configuration definitions.

### 3.1 Idle and Tick hooks

The following definitions were added:

```c
#define configUSE_IDLE_HOOK    0
#define configUSE_TICK_HOOK    0
```

These disable the optional application-defined Idle and Tick hook functions.

### 3.2 Task delay API

The following definition was added:

```c
#define INCLUDE_vTaskDelay    1
```

This enables the `vTaskDelay()` API implementation.

### 3.3 Static allocation

Initially, both static and dynamic allocation were enabled.

For this initial application, static allocation was disabled:

```c
#define configSUPPORT_STATIC_ALLOCATION       0
#define configSUPPORT_DYNAMIC_ALLOCATION      1
```

Dynamic allocation is used for creating the queue and tasks.

### 3.4 Stack overflow hook

Stack overflow checking was enabled:

```c
#define configCHECK_FOR_STACK_OVERFLOW 2
```

The application provides the required callback:

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();

    while (1)
    {
    }
}
```

If FreeRTOS detects a stack overflow, execution stops in this hook.

## 4. First Successful FreeRTOS Build

The following commands were used:

```bash
make clean
make
```

The build successfully compiled all seven source files and linked them into:

```text
firmware.elf
```

The linker also generated:

```text
firmware.map
```

The initial firmware build completed without compiler or linker errors.

## 5. ELF Verification

Before running QEMU, the generated ELF was inspected.

### 5.1 Verify firmware file

```bash
ls -lh firmware.elf
```

The generated ELF file was approximately 64 KB.

### 5.2 Verify FreeRTOS symbols

```bash
arm-none-eabi-nm firmware.elf | grep -E "vTaskDelay|vPortSVCHandler|xPortPendSVHandler|xPortSysTickHandler"
```

The following symbols were found:

| Symbol | Address |
|---|---:|
| `vTaskDelay` | `0x00000758` |
| `vPortSVCHandler` | `0x00001AE8` |
| `xPortPendSVHandler` | `0x00001D40` |
| `xPortSysTickHandler` | `0x00001DA4` |

This confirmed that the required API and exception-handler implementations were linked into the firmware.

### 5.3 Verify ELF sections

```bash
arm-none-eabi-readelf -S firmware.elf
```

Important sections:

| Section | Address |
|---|---:|
| `.isr_vector` | `0x00000000` |
| `.data` | `0x20000000` |
| `.bss` | `0x20000004` |

The vector table was located at the beginning of the firmware image.

### 5.4 Verify entry point

```bash
arm-none-eabi-readelf -h firmware.elf | grep Entry
```

The ELF entry point was:

```text
0x319
```

The odd address is expected for a Cortex-M Thumb function because bit 0 indicates Thumb execution state.

## 6. Vector Table Verification

The vector table was inspected using:

```bash
arm-none-eabi-objdump -s -j .isr_vector firmware.elf
```

The output confirmed the following entries:

| Vector index | Offset | Decoded address | Handler |
|---:|---:|---:|---|
| 0 | `0x00` | `0x20400000` | Initial MSP |
| 1 | `0x04` | `0x00000319` | Reset_Handler |
| 2 | `0x08` | `0x0000032D` | Default_Handler |
| 11 | `0x2C` | `0x00001AE9` | vPortSVCHandler |
| 14 | `0x38` | `0x00001D41` | xPortPendSVHandler |
| 15 | `0x3C` | `0x00001DA5` | xPortSysTickHandler |

### Important observations

- Each vector table entry occupies 4 bytes.
- The vector table starts at address zero.
- The initial stack pointer is `0x20400000`.
- The three FreeRTOS exception handlers are connected to the correct vector indexes.
- Handler addresses are odd because the Cortex-M processor executes Thumb instructions.

The vector table contains 48 entries: 16 core exception entries and 32 external IRQ entries.

## 7. Running FreeRTOS in QEMU

QEMU was started in debug mode:

```bash
qemu-system-arm -M mps2-an386 \
    -kernel firmware.elf \
    -nographic -monitor none \
    -S -gdb tcp::1234
```

Important options:

| Option | Purpose |
|---|---|
| `-M mps2-an386` | Selects the Cortex-M4 QEMU machine |
| `-kernel firmware.elf` | Loads the firmware |
| `-nographic` | Runs without graphical display |
| `-monitor none` | Disables the QEMU monitor |
| `-S` | Starts QEMU paused |
| `-gdb tcp::1234` | Opens the GDB debugging connection |

In a second terminal:

```bash
gdb-multiarch firmware.elf
```

GDB was connected using:

```gdb
set architecture arm
target remote :1234
```

## 8. Debugging the Startup Sequence

### 8.1 Reset_Handler

A breakpoint was set:

```gdb
break Reset_Handler
continue
```

GDB stopped inside `Reset_Handler`.

Observed registers:

```text
PC = 0x31C
SP = 0x203FFFF8
```

The initial stack pointer was:

```text
0x20400000
```

The stack pointer became `0x203FFFF8` because the function prologue pushed two 32-bit registers onto the stack.

This confirmed that the startup handler was executing with the expected initial stack region.

### 8.2 main()

A breakpoint was set:

```gdb
break main
continue
```

The processor successfully reached `main()` after completing startup initialization.

This confirmed that the firmware could execute the startup sequence and enter the application.

### 8.3 Scheduler startup

The application called:

```c
vTaskStartScheduler();
```

FreeRTOS initialized the scheduler and created its Idle task.

The Cortex-M4F port uses the SVC exception to start the first task.

The first task's initial context is prepared before execution begins.

### 8.4 Observing the Idle task

During GDB inspection, the processor was observed executing:

```text
prvIdleTask()
```

The register output showed:

```text
PC = 0x10D8
SP = 0x20000C70
LR = 0x10D9
```

The stack address was inside `ucHeap`, which is expected because FreeRTOS dynamically allocates task stacks from its heap.

The GDB backtrace also displayed an unusual stack involving `pxPortInitialiseStack()`.

This needs to be interpreted carefully: FreeRTOS constructs an initial task stack frame artificially, rather than through an ordinary C function call chain. Therefore, a normal backtrace may not accurately represent the task's execution history.

## 9. Producer and Consumer Task Execution

The application creates two tasks:

```c
xTaskCreate(Producer_Task, "Producer", 256, NULL, 2, &producerHandle);

xTaskCreate(Consumer_Task, "Consumer", 256, NULL, 2, &consumerHandle);
```

Both tasks have priority 2.

The Idle task has the lowest priority.

### 9.1 Producer task

A GDB breakpoint was set:

```gdb
break Producer_Task
continue
```

GDB successfully stopped inside:

```c
void Producer_Task(void *pvParameters)
```

The local sensor structure contained:

```c
sensor.sensor_id = 1;
sensor.timestamp = 100;
sensor.temperature = 25;
```

This confirmed that the Producer task was created and scheduled successfully.

### 9.2 Consumer task

A breakpoint was set:

```gdb
break Consumer_Task
continue
```

GDB successfully stopped inside:

```c
void Consumer_Task(void *pvParameters)
```

The Consumer received the sensor structure through:

```c
xQueueReceive(sensorQueue, &receivedsensor, portMAX_DELAY);
```

The received structure was inspected in GDB.

The observed temperature was:

```text
temperature = 27
```

The Consumer's processing logic had added 2 to the received temperature.

This confirmed that the Producer and Consumer tasks were executing and that the queue was transferring sensor data successfully.

## 10. Producer–Consumer Queue Data Flow

The application uses a FreeRTOS queue to transfer a structure between tasks.

The structure is:

```c
typedef struct
{
    uint16_t sensor_id;
    uint32_t timestamp;
    int16_t temperature;
} SensorData;
```

The queue is created using:

```c
sensorQueue = xQueueCreate(10, sizeof(SensorData));
```

### Producer

The Producer creates sensor data and sends it:

```c
xQueueSend(sensorQueue, &sensor, pdMS_TO_TICKS(100));
```

FreeRTOS copies the structure into the queue's internal storage.

### Consumer

The Consumer receives the data:

```c
xQueueReceive(sensorQueue, &receivedsensor, portMAX_DELAY);
```

FreeRTOS copies the queued structure into the Consumer's local variable.

### Important concept: Queue copies

A FreeRTOS queue copies the item data.

Therefore:

- Modifying the original Producer variable after sending does not modify the queued copy.
- Modifying the Consumer's received structure does not modify the original Producer variable.
- If a structure contains a pointer, the queue copies the pointer value, not the memory pointed to by it.

## 11. Task States and Blocking

FreeRTOS tasks can move between several states:

- Running
- Ready
- Blocked
- Suspended

### Consumer blocking

The Consumer calls:

```c
xQueueReceive(sensorQueue, &receivedsensor, portMAX_DELAY);
```

When the queue is empty, the Consumer enters the Blocked state.

This means:

1. The Consumer stops executing.
2. FreeRTOS removes it from the Ready list.
3. The scheduler selects another Ready task.
4. When queue data becomes available, the Consumer becomes Ready again.

Blocking avoids continuously polling the queue and wasting CPU cycles.

### Idle task

The Idle task executes when no application task is Ready to run.

For example, if both Producer and Consumer are Blocked and there are no other Ready application tasks, the Idle task can execute.

The Idle task has the lowest priority.

It also performs kernel housekeeping, including reclaiming memory associated with deleted tasks.

## 12. Task Priorities

FreeRTOS uses higher numerical values to represent higher task priorities.

For example:

| Task | Priority |
|---|---:|
| Consumer | 3 |
| Producer | 2 |
| Idle | 0 |

If a higher-priority task becomes Ready while a lower-priority task is running, the higher-priority task can preempt it when preemption is enabled.

### Equal-priority tasks

If Producer and Consumer both have priority 2:

- Both tasks have equal scheduling priority.
- Time slicing can share CPU execution between Ready tasks of equal priority.
- A task can also be switched out when it blocks or yields.
- A newly unblocked equal-priority task does not necessarily cause an immediate switch merely because it became Ready; the precise behaviour depends on the kernel scheduling and yield path.

### Preemption versus time slicing

| Feature | Purpose |
|---|---|
| Preemption | Allows a higher-priority Ready task to take the CPU |
| Time slicing | Shares CPU execution between equal-priority Ready tasks |

The configuration used is:

```c
#define configUSE_PREEMPTION  1
#define configUSE_TIME_SLICING 1
```

## 13. Current Project Milestone

The following have been completed and verified:

| Milestone | Status |
|---|---|
| FreeRTOS kernel integration | Completed |
| Cortex-M4F port compilation | Completed |
| FreeRTOS startup and linker integration | Completed |
| FreeRTOS Makefile | Completed |
| Firmware ELF generation | Successful |
| FreeRTOS symbol verification | Successful |
| Vector table verification | Successful |
| QEMU startup | Successful |
| Reset_Handler execution | Verified |
| main() execution | Verified |
| Scheduler startup | Verified |
| Producer task execution | Verified |
| Consumer task execution | Verified |
| Queue data transfer | Verified |
| Task blocking and Idle task concepts | Studied |
| Task priorities and time slicing | Studied |

## 14. Next Learning Milestone

The next topic is:

**FreeRTOS Semaphores**

Planned topics:

1. Why semaphores are required.
2. Binary semaphore.
3. `xSemaphoreCreateBinary()`.
4. `xSemaphoreTake()`.
5. `xSemaphoreGive()`.
6. Binary semaphore versus queue.
7. ISR-to-task signaling using `xSemaphoreGiveFromISR()`.

The implementation will follow the same approach: understand the concept, write pseudocode, implement in C, build, run in QEMU and debug using GDB.