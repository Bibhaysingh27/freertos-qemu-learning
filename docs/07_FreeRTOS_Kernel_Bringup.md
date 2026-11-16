# FreeRTOS Kernel Bring-up on ARM Cortex-M4F

## 1. Objective

Integrate the FreeRTOS kernel into the existing bare-metal Cortex-M4 learning project using QEMU's `mps2-an386` machine.

The objective is to understand how FreeRTOS interacts with the Cortex-M4 processor, including:

- FreeRTOS kernel architecture
- Cortex-M4F processor port
- SVC, PendSV and SysTick exceptions
- FreeRTOS configuration
- Startup code and vector table integration
- Linker script configuration

The existing bare-metal project is preserved separately.

## 2. Project Structure

The project is organized as follows:

```text
freertos-qemu-learning/
│
├── 01_bare_metal_basics/
│   ├── main.c
│   ├── startup.c
│   ├── linker.ld
│   └── Makefile
│
├── 02_freertos/
│   ├── main.c
│   ├── startup.c
│   ├── FreeRTOSConfig.h
│   ├── linker.ld
│   └── Makefile
│
├── third_party/
│   └── FreeRTOS-Kernel/
│
└── docs/
```

FreeRTOS is added as a Git submodule:

```bash
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git third_party/FreeRTOS-Kernel
```

The kernel version used in this project is V10.4.3.

## 3. FreeRTOS Kernel Architecture

The following source files are important for the initial integration.

| Source file | Responsibility |
|---|---|
| `tasks.c` | Task management and scheduler |
| `queue.c` | Queues, semaphores and mutexes |
| `list.c` | Internal linked-list management |
| `portable/GCC/ARM_CM4F/port.c` | Cortex-M4F-specific processor port |
| `portable/MemMang/heap_4.c` | Dynamic memory allocation |

### Kernel versus processor port

FreeRTOS kernel code contains the generic scheduling and task-management logic.

The processor-specific port handles operations that depend on the CPU architecture, including:

- Saving and restoring task context
- Starting the first task
- Managing processor exceptions
- Configuring interrupt priorities
- Managing the Cortex-M4F stack pointers and FPU context

## 4. Cortex-M4F Exception Handling in FreeRTOS

FreeRTOS uses three important Cortex-M exceptions.

### 4.1 SVC (Supervisor Call)

SVC is used to start the first FreeRTOS task.

Sequence:

1. `main()` creates the required tasks.
2. `vTaskStartScheduler()` initializes the scheduler.
3. FreeRTOS selects the first task.
4. `prvPortStartFirstTask()` executes the `SVC` instruction.
5. The processor enters `vPortSVCHandler()`.
6. The handler restores the initial context of the first task.
7. Execution begins in the first task.

### 4.2 PendSV (Pending Supervisor Call)

PendSV performs deferred task context switching.

The PendSV exception is assigned the lowest interrupt priority.

Why?

If a context switch is requested while a higher-priority interrupt is executing, PendSV remains pending until higher-priority exceptions have completed.

This reduces interrupt latency and avoids performing the context switch directly inside SysTick.

The PendSV handler:

- Saves the current task context.
- Updates the current task's stack pointer.
- Calls `vTaskSwitchContext()` to select the next task.
- Restores the next task's context.
- Returns to the selected task.

### 4.3 SysTick

SysTick provides the periodic RTOS tick.

Its responsibilities include:

- Incrementing the RTOS tick count.
- Checking delayed tasks and timeouts.
- Making expired blocked tasks Ready.
- Requesting a context switch when necessary.

If a context switch is required, SysTick sets PendSV to pending.

Not every SysTick interrupt results in a context switch.

### 4.4 Exception priority example

Consider the following priorities:

| Exception | Priority |
|---|---:|
| UART interrupt | 2 |
| SysTick | 5 |
| PendSV | 15 |

Lower numerical values represent higher interrupt urgency.

If UART is executing and SysTick requests a context switch, PendSV waits until the processor can service the lower-priority exception.

## 5. FreeRTOS Interrupt Priority Configuration

The configuration used as the starting point is:

```c
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  0x50
```

This is a provisional hardware-encoded priority threshold for a Cortex-M configuration with four implemented NVIC priority bits.

Important concepts:

- Higher-urgency interrupts must not call FreeRTOS APIs.
- Interrupts at permitted priorities can use the appropriate `FromISR` APIs.
- `BASEPRI` masks interrupts at and below the configured threshold in terms of urgency.
- PendSV is configured at the lowest priority.

The exact priority configuration must be validated against the target processor and FreeRTOS port.

## 6. FreeRTOSConfig.h

`FreeRTOSConfig.h` contains application-specific kernel configuration.

Important settings include:

### Scheduler configuration

```c
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configMAX_TASK_NAME_LEN                 16

#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TICKLESS_IDLE                 0
```

### Clock configuration

```c
#define configCPU_CLOCK_HZ        25000000UL
#define configTICK_RATE_HZ         100
```

The CPU clock is configured for the current QEMU target. The SysTick clock-source selection must be verified separately before treating any external reference-clock assumption as confirmed.

### Memory configuration

```c
#define configSUPPORT_STATIC_ALLOCATION       0
#define configSUPPORT_DYNAMIC_ALLOCATION      1

#define configTOTAL_HEAP_SIZE                 (20 * 1024)
#define configAPPLICATION_ALLOCATED_HEAP      0
```

Dynamic allocation is used for the initial application.

The `heap_4.c` allocator is included in the project.

### Runtime checks

```c
#define configCHECK_HANDLER_INSTALLATION 1
#define configCHECK_FOR_STACK_OVERFLOW  2
```

`configCHECK_HANDLER_INSTALLATION` enables validation of the SVC and PendSV handler installation.

Stack overflow checking is enabled at level 2.

### Optional hooks

```c
#define configUSE_IDLE_HOOK  0
#define configUSE_TICK_HOOK  0
```

These optional application callbacks are disabled.

### API inclusion

```c
#define INCLUDE_vTaskDelay  1
```

This enables the `vTaskDelay()` API implementation.

### Assertions

```c
#define configASSERT(x)                 \
    do                                  \
    {                                   \
        if ((x) == 0)                   \
        {                               \
            __asm volatile ("cpsid i"); \
            for (;;)                    \
            {                           \
            }                           \
        }                               \
    } while (0)
```

If an assertion fails, maskable interrupts are disabled and execution remains in an infinite loop.

## 7. FreeRTOS Startup Code

A separate startup file is created:

`02_freertos/startup.c`

The original bare-metal startup code remains unchanged.

### 7.1 Handler declarations

```c
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
```

### 7.2 Vector table mapping

The Cortex-M vector table uses the following exception positions:

| Vector index | Exception | Handler |
|---:|---|---|
| 0 | Initial MSP | `_estack` |
| 1 | Reset | `Reset_Handler` |
| 2 | NMI | `Default_Handler` |
| 3 | HardFault | `Default_Handler` |
| 4 | MemManage | `Default_Handler` |
| 5 | BusFault | `Default_Handler` |
| 6 | UsageFault | `Default_Handler` |
| 7–10 | Reserved | `0` |
| 11 | SVCall | `vPortSVCHandler` |
| 12 | DebugMonitor | `Default_Handler` |
| 13 | Reserved | `0` |
| 14 | PendSV | `xPortPendSVHandler` |
| 15 | SysTick | `xPortSysTickHandler` |
| 16 onward | External IRQs | `Default_Handler` |

The vector table is declared with:

```c
__attribute__((section(".isr_vector"), used, aligned(256)))
const VectorEntry vector_table[] = {
    /* Vector entries */
};
```

The table contains 48 entries in the current learning implementation: 16 core exception entries and 32 external IRQ entries.

### 7.3 Reset_Handler initialization

The reset handler performs the following operations:

1. Copy initialized `.data` values from their load address to RAM.
2. Clear the `.bss` section.
3. Call `main()`.
4. Enter an infinite loop if `main()` returns.

The `.data` and `.bss` initialization is essential because FreeRTOS depends on correctly initialized global and static variables.

## 8. FreeRTOS Linker Script

A separate linker script is created:

`02_freertos/linker.ld`

The QEMU MPS2-AN386 memory regions used are:

| Region | Start address | Size |
|---|---:|---:|
| CODE_RAM | `0x00000000` | 4 MB |
| RAM | `0x20000000` | 4 MB |

The address-zero region is emulated RAM on this QEMU machine, not physical Flash. It is named CODE_RAM to distinguish its firmware-image role.

### Important linker sections

| Section | Purpose |
|---|---|
| `.isr_vector` | Interrupt and exception vector table |
| `.text` | Program instructions |
| `.rodata` | Read-only data |
| `.ARM.extab` | ARM exception-related data |
| `.ARM.exidx` | ARM exception index |
| `.data` | Initialized global/static variables |
| `.bss` | Zero-initialized global/static variables |

### Data section placement

The `.data` section has two addresses:

- VMA: runtime address in RAM.
- LMA: load address in CODE_RAM.

The linker symbol:

```ld
_sidata = LOADADDR(.data);
```

provides the source address used by `startup.c` to initialize `.data`.

### Important linker symbols

```ld
_sdata
_edata
_sidata
_sbss
_ebss
_estack
```

These symbols are referenced by the startup code.

The stack is placed at the top of RAM:

```ld
_estack = ORIGIN(RAM) + LENGTH(RAM);
```

Alignment and RAM boundary checks are included:

```ld
ASSERT((_estack & 7) == 0, "Stack must be 8-byte aligned");
ASSERT(_ebss <= _estack, "RAM overflow");
```

## 9. FreeRTOS Scheduler Startup Flow

The complete startup sequence is:

```text
Processor Reset
      |
      v
Vector Table
      |
      v
Reset_Handler
      |
      v
Initialize .data
      |
      v
Clear .bss
      |
      v
main()
      |
      v
Create Queue
      |
      v
Create Producer Task
      |
      v
Create Consumer Task
      |
      v
vTaskStartScheduler()
      |
      v
prvPortStartFirstTask()
      |
      v
SVC Instruction
      |
      v
vPortSVCHandler()
      |
      v
First FreeRTOS Task Starts
      |
      v
SysTick / PendSV Context Switching
```

Remember:

- SVC starts the first task.
- SysTick maintains the RTOS tick.
- PendSV performs subsequent task context switches.

## 10. Current Milestone

Completed:

- FreeRTOS kernel added as a Git submodule.
- Cortex-M4F port identified.
- FreeRTOS configuration file prepared.
- FreeRTOS-specific startup code created.
- SVC, PendSV and SysTick handlers connected to the vector table.
- Separate FreeRTOS linker script created.
- Memory regions and linker symbols configured.

**Documentation scope ends here.** Makefile creation, compilation, ELF verification and QEMU debugging are documented as the next milestone.