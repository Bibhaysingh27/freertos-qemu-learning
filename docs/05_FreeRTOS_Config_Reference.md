# FreeRTOS Configuration Reference

## Target Platform

| Component | Configuration |
|---|---|
| Board | QEMU MPS2-AN386 |
| Processor | ARM Cortex-M4 |
| Compiler | ARM GCC |
| FreeRTOS port | GCC ARM_CM4F |
| Emulator | QEMU |
| Debugger | GDB Multiarch + GDB Dashboard |
| Repository | freertos-qemu-learning |

**Documentation status:** Learning reference — configuration values are subject to build-time and runtime verification.

---

# 1. Hardware and Clock Configuration

## 1.1 `configCPU_CLOCK_HZ`

**Purpose:** Defines the CPU clock frequency used by the FreeRTOS port.

```c
#define configCPU_CLOCK_HZ 25000000UL
```

- CPU clock is reported as 25 MHz for the QEMU MPS2-AN386 model.
- Used by the FreeRTOS Cortex-M port for clock-related calculations.

**Important:** Verify the actual clock exposed by the selected QEMU machine before finalizing the application configuration.

## 1.2 `configSYSTICK_CLOCK_HZ`

**Purpose:** Defines the clock frequency supplied to the Cortex-M SysTick timer.

```c
#define configSYSTICK_CLOCK_HZ 1000000UL
```

The Cortex-M4F port contains:

```c
#ifndef configSYSTICK_CLOCK_HZ
    #define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG \
            portNVIC_SYSTICK_CLK_BIT
#else
    #define portNVIC_SYSTICK_CLK_BIT_CONFIG 0
#endif
```

Meaning:

- If undefined, SysTick uses the processor clock.
- If defined, the port selects the external SysTick clock source.
- The configured value must match the actual external clock.

**Current proposed value:** 1 MHz, based on the QEMU reference clock assumption. Verify that this is the actual clock connected to the external SysTick source before using it.

## 1.3 `configTICK_RATE_HZ`

**Purpose:** Defines how many RTOS ticks occur per second.

Proposed configuration:

```c
#define configTICK_RATE_HZ 100
```

Formula:

Tick period = 1 / Tick frequency

Example:

100 Hz = 10 ms per tick.

Another example:

```c
#define configTICK_RATE_HZ 1000
```

This produces a 1 ms tick.

### SysTick reload calculation

The Cortex-M SysTick timer is a 24-bit down-counter.

FreeRTOS calculates:

```c
portNVIC_SYSTICK_LOAD_REG =
    (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ) - 1UL;
```

Example using a 1 MHz SysTick clock and 100 Hz tick:

```text
Reload = (1,000,000 / 100) - 1
       = 9,999
```

The subtraction of one is required because the counter counts down to zero.

Maximum reload value:

```c
#define portMAX_24_BIT_NUMBER 0xFFFFFFUL
```

The calculated reload must fit within the 24-bit counter.

---

# 2. Scheduling Configuration

## 2.1 `configUSE_PREEMPTION`

**Purpose:** Enables preemptive or cooperative scheduling.

```c
#define configUSE_PREEMPTION 1
```

- `1`: Preemptive scheduling.
- `0`: Cooperative scheduling.

Example:

| Task | Priority |
|---|---:|
| Task A | 1 |
| Task B | 3 |

If Task A is executing and Task B becomes Ready, Task B can preempt Task A.

## 2.2 `configUSE_TIME_SLICING`

**Purpose:** Controls time slicing between Ready tasks having equal priority.

```c
#define configUSE_TIME_SLICING 1
```

- `1`: Equal-priority Ready tasks can rotate on tick interrupts.
- `0`: A tick alone does not cause equal-priority time slicing.

Equal-priority tasks can still switch when a task blocks or explicitly yields.

## 2.3 `configMAX_PRIORITIES`

**Purpose:** Defines the number of available task priority levels.

Example:

```c
#define configMAX_PRIORITIES 5
```

Valid priorities:

```text
Priority 4 = Highest
Priority 3
Priority 2
Priority 1
Priority 0 = Lowest
```

Five priority levels means 0 through 4, not 0 through 5.

## 2.4 `configUSE_TICKLESS_IDLE`

**Purpose:** Enables low-power tickless operation.

```c
#define configUSE_TICKLESS_IDLE 0
```

- `0`: Normal periodic RTOS tick.
- `1`: Suppress periodic ticks during suitable idle periods.

We will initially disable tickless idle because our goal is to understand normal scheduling and context switching.

## 2.5 `configIDLE_SHOULD_YIELD`

**Purpose:** Determines whether the Idle task yields to an application task running at Idle priority.

```c
#define configIDLE_SHOULD_YIELD 1
```

---

# 3. Task and Stack Configuration

## 3.1 `configMINIMAL_STACK_SIZE`

**Purpose:** Defines the Idle task stack size in words, not bytes.

Example:

```c
#define configMINIMAL_STACK_SIZE 128
```

For a 32-bit Cortex-M4:

```text
128 words × 4 bytes = 512 bytes
```

This setting does not automatically define the stack size of every application task.

## 3.2 `configMAX_TASK_NAME_LEN`

**Purpose:** Defines the maximum task name length, including the null terminator.

```c
#define configMAX_TASK_NAME_LEN 16
```

Example:

```c
xTaskCreate(sensorTask, "Sensor", ...);
```

Task names are useful when debugging and inspecting task lists.

## 3.3 `configTICK_TYPE_WIDTH_IN_BITS`

**Purpose:** Defines the width of the RTOS tick counter.

Proposed configuration:

```c
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
```

A 32-bit tick counter is suitable for our initial Cortex-M4 learning project.

---

# 4. Memory Configuration

## 4.1 Static allocation

```c
#define configSUPPORT_STATIC_ALLOCATION 1
```

Static allocation means the application provides the memory required by FreeRTOS objects.

Example:

```c
static StaticTask_t taskTCB;
static StackType_t taskStack[128];
```

Advantages:

- Predictable memory usage.
- No runtime heap allocation for the statically created object.
- Useful for systems with strict memory requirements.

## 4.2 Dynamic allocation

```c
#define configSUPPORT_DYNAMIC_ALLOCATION 1
```

Dynamic allocation allows FreeRTOS to allocate memory from its heap.

Example:

```c
xTaskCreate(...);
```

Advantages:

- Convenient task and object creation.
- Flexible runtime allocation.

Considerations:

- Allocation can fail.
- Heap fragmentation depends on the allocator.
- Runtime memory usage must be monitored.

## 4.3 `configTOTAL_HEAP_SIZE`

**Purpose:** Defines the FreeRTOS heap size for heap implementations that use this setting.

Example:

```c
#define configTOTAL_HEAP_SIZE (20 * 1024)
```

This example represents 20 KB.

Important distinction:

- FreeRTOS heap is separate from the standard C library heap.
- Task stacks allocated dynamically consume FreeRTOS heap.
- The Idle task stack and other dynamically allocated kernel objects also consume heap.

## 4.4 Heap implementation

Our planned initial implementation:

```text
heap_4.c
```

`heap_4.c` supports free operations and coalesces adjacent free blocks.

We will inspect and integrate the allocator before finalizing heap size.

---

# 5. Queues, Semaphores, Mutexes and Timers

These configuration settings enable optional kernel functionality.

| Macro | Purpose |
|---|---|
| `configUSE_MUTEXES` | Enable mutex APIs |
| `configUSE_RECURSIVE_MUTEXES` | Enable recursive mutex APIs |
| `configUSE_COUNTING_SEMAPHORES` | Enable counting semaphores |
| `configUSE_QUEUE_SETS` | Enable queue sets |
| `configUSE_TIMERS` | Enable software timers |
| `configUSE_EVENT_GROUPS` | Enable event groups |
| `configUSE_STREAM_BUFFERS` | Enable stream buffers |

## Software timer configuration

```c
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY \
    (configMAX_PRIORITIES - 1)

#define configTIMER_TASK_STACK_DEPTH \
    configMINIMAL_STACK_SIZE

#define configTIMER_QUEUE_LENGTH 10
```

Software timers are managed by a dedicated timer service task.

They do not require one hardware timer per software timer.

---

# 6. Interrupt Configuration — Cortex-M4F

This is one of the most important sections for embedded firmware development.

## 6.1 Cortex-M NVIC priority behaviour

Cortex-M uses numerical priorities where:

- Priority 0 = Highest urgency.
- Larger numerical values = Lower urgency.

Our Cortex-M4 target is expected to implement four NVIC priority bits. This must be verified against the target configuration.

If four priority bits are implemented:

| Logical priority | Hardware-encoded priority |
|---:|---:|
| 0 | 0x00 |
| 1 | 0x10 |
| 2 | 0x20 |
| 3 | 0x30 |
| 4 | 0x40 |
| 5 | 0x50 |
| 6 | 0x60 |
| 7 | 0x70 |
| 8 | 0x80 |
| 9 | 0x90 |
| 10 | 0xA0 |
| 11 | 0xB0 |
| 12 | 0xC0 |
| 13 | 0xD0 |
| 14 | 0xE0 |
| 15 | 0xF0 |

The lower four bits are not implemented.

## 6.2 `configMAX_SYSCALL_INTERRUPT_PRIORITY`

**Purpose:** Defines the interrupt priority threshold used by FreeRTOS when masking interrupts through BASEPRI.

Proposed configuration:

```c
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 0x50
```

This corresponds to logical priority 5 with four implemented NVIC priority bits.

### Important rule

Interrupts with higher urgency than the configured threshold must not call FreeRTOS APIs.

Example:

| Interrupt priority | Can call FreeRTOS FromISR APIs? |
|---:|---|
| 0 | No |
| 1 | No |
| 2 | No |
| 3 | No |
| 4 | No |
| 5 | Yes |
| 6–15 | Yes |

The permitted ISR APIs include:

```c
xQueueSendFromISR();
xSemaphoreGiveFromISR();
xTaskNotifyFromISR();
```

These must only be called from interrupts with valid configured priorities and appropriate interrupt-priority grouping.

## 6.3 BASEPRI

BASEPRI is a Cortex-M special register used to mask interrupts according to a priority threshold.

FreeRTOS uses it in critical sections.

Example from the port:

```asm
msr basepri, r0
```

Setting BASEPRI to zero removes the BASEPRI priority mask.

Important:

- BASEPRI does not mask priority-zero interrupts.
- Higher-urgency interrupts can remain enabled.
- Incorrect priority configuration can cause kernel corruption.

## 6.4 PendSV and SysTick priorities

The FreeRTOS Cortex-M4F port configures both at the lowest interrupt priority.

Relevant definitions:

```c
#define portNVIC_PENDSV_PRI ...
#define portNVIC_SYSTICK_PRI ...
```

Purpose:

- PendSV: Deferred context switching.
- SysTick: RTOS tick processing.

SVC is configured at the highest system-handler priority in this port.

---

# 7. Cortex-M4F Exception Handlers

Three exception handlers are central to FreeRTOS scheduling.

## 7.1 SVC Handler

Function:

```c
void vPortSVCHandler(void);
```

Purpose: Starts the first FreeRTOS task.

Flow:

1. Read `pxCurrentTCB`.
2. Get the selected task's saved stack pointer.
3. Restore registers R4–R11 and LR.
4. Restore PSP.
5. Clear BASEPRI.
6. Execute `BX LR`.
7. Cortex-M performs exception return and starts the first task.

Important instruction:

```asm
bx r14
```

The LR contains an EXC_RETURN value, which controls exception return.

## 7.2 SysTick Handler

Function:

```c
void xPortSysTickHandler(void);
```

Purpose: Maintains the RTOS tick and checks whether scheduling is required.

Important operations:

```c
portDISABLE_INTERRUPTS();

if (xTaskIncrementTick() != pdFALSE)
{
    portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
}

portENABLE_INTERRUPTS();
```

`xTaskIncrementTick()`:

- Increments the RTOS tick count.
- Processes delayed tasks.
- Checks whether tasks should become Ready.
- Indicates whether a context switch is required.

SysTick does not perform the context switch itself. It requests PendSV.

## 7.3 PendSV Handler

Function:

```c
void xPortPendSVHandler(void);
```

Purpose: Performs the actual task context switch.

Main operations:

1. Read PSP.
2. Find the current task TCB.
3. Save R4–R11 and LR.
4. Save the outgoing task's stack pointer.
5. Call `vTaskSwitchContext()`.
6. Get the next task's stack pointer.
7. Restore the next task's registers.
8. Update PSP.
9. Return to the next task.

Important instructions:

| Instruction | Meaning |
|---|---|
| MRS | Read special register |
| MSR | Write special register |
| STMDB | Store multiple registers, decrement before |
| LDMIA | Load multiple registers, increment after |
| ISB | Instruction synchronization barrier |
| DSB | Data synchronization barrier |
| BX | Branch and exchange |

### Quick comparison

| Handler | Main responsibility |
|---|---|
| SVC | Start the first task |
| SysTick | Maintain RTOS tick and request scheduling |
| PendSV | Save and restore task contexts |

---

# 8. Debugging and Safety

## 8.1 Stack overflow checking

```c
#define configCHECK_FOR_STACK_OVERFLOW 2
```

FreeRTOS provides two commonly used checking levels:

- Level 1: Checks the task stack pointer against stack bounds.
- Level 2: Also checks a known pattern at the stack boundary.

This requires an application callback:

```c
void vApplicationStackOverflowHook(
    TaskHandle_t xTask,
    char *pcTaskName
);
```

## 8.2 `configUSE_MALLOC_FAILED_HOOK`

```c
#define configUSE_MALLOC_FAILED_HOOK 1
```

Enables a callback when a FreeRTOS memory allocation fails.

Required callback:

```c
void vApplicationMallocFailedHook(void);
```

## 8.3 `configASSERT`

Assertions help detect incorrect kernel usage and invalid configurations.

For our project, we will implement an assertion handler that stops execution so we can inspect the failure using GDB.

## 8.4 Other useful settings

| Macro | Purpose |
|---|---|
| `configUSE_IDLE_HOOK` | Enable Idle task hook |
| `configUSE_TICK_HOOK` | Enable RTOS tick hook |
| `configQUEUE_REGISTRY_SIZE` | Queue registry capacity |
| `configTASK_NOTIFICATION_ARRAY_ENTRIES` | Notification indexes per task |
| `configNUM_THREAD_LOCAL_STORAGE_POINTERS` | Task-local storage pointer count |

---

# 9. Important Interview Questions

1. What is the difference between static and dynamic allocation?
2. Why is configMINIMAL_STACK_SIZE measured in words?
3. What is the difference between configTICK_RATE_HZ and configCPU_CLOCK_HZ?
4. How is the SysTick reload value calculated?
5. Why does SysTick request PendSV instead of switching tasks directly?
6. What is the role of BASEPRI?
7. Why can't an interrupt with priority 3 call FreeRTOS APIs when the syscall threshold is priority 5?
8. What is the difference between SVC and PendSV?
9. What is stored in a Task Control Block?
10. What happens when a task's stack overflows?
11. What is the difference between a hardware timer and a FreeRTOS software timer?
12. What is the difference between heap_4 and heap_1?

---

# 10. Practical Experiments

1. Change tick frequency and observe the resulting SysTick reload value.
2. Create two equal-priority tasks and observe time slicing.
3. Create tasks with different priorities and observe preemption.
4. Inspect PSP and MSP using GDB.
5. Break at `xPortSysTickHandler()`.
6. Break at `xPortPendSVHandler()`.
7. Inspect task stack contents during context switching.
8. Test dynamic allocation failure.
9. Monitor task stack high-water marks.
10. Test queue communication between two tasks.

---

# 11. Configuration Status

| Setting or feature | Status |
|---|---|
| QEMU MPS2-AN386 target | Selected |
| Cortex-M4F port | Identified |
| SysTick reload formula | Understood |
| SVC implementation | Studied |
| SysTick implementation | Studied |
| PendSV implementation | Studied |
| CPU clock | Requires final verification |
| External SysTick clock | Requires final verification |
| NVIC priority bits | Requires target verification |
| Syscall threshold | Proposed: 0x50 |
| FreeRTOS heap size | Pending |
| FreeRTOSConfig.h | Pending |
| First FreeRTOS build | Pending |

**Documentation rule:** Never treat a proposed configuration as a verified runtime result. Update this table after successful compilation and debugging.

