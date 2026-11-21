# 12. FreeRTOS Binary Semaphore vs Mutex — Priority Inversion Experiment

## 1. Objective

Understand the differences between FreeRTOS binary semaphores and mutexes, particularly their behavior when multiple tasks with different priorities access a shared resource.

This experiment demonstrates:

- Binary semaphore creation and initialization.
- Task synchronization using task notifications.
- Shared resource protection using a binary semaphore.
- Priority inversion.
- Absence of priority inheritance with binary semaphores.
- Comparison with mutex priority inheritance.
- Runtime verification using QEMU and GDB.

## 2. Important Concepts

### 2.1 Binary Semaphore

A binary semaphore is a synchronization mechanism that can hold one of two states:

- Available (1)
- Unavailable (0)

It is commonly used for event signalling between tasks and ISRs.

Creation:

```c
SemaphoreHandle_t binarySem;

binarySem = xSemaphoreCreateBinary();

if (binarySem == NULL)
{
    while (1) {}
}
```

A newly created binary semaphore is initially empty.

Therefore, if we want a task to acquire it immediately, we must give it once:

```c
xSemaphoreGive(binarySem);
```

Taking the semaphore:

```c
if (xSemaphoreTake(binarySem, portMAX_DELAY) == pdTRUE)
{
    // Access shared resource

    xSemaphoreGive(binarySem);
}
```

Important characteristics:

- Binary semaphores can be used for synchronization.
- They can also be used as basic locks.
- They do not provide mutex ownership semantics.
- They do not support priority inheritance.
- They can be given from an ISR using `xSemaphoreGiveFromISR()`.

### 2.2 Mutex

A mutex (Mutual Exclusion) is designed to protect shared resources.

Important characteristics:

- A mutex has ownership.
- Only its owning task should release it.
- It supports priority inheritance.
- It cannot be used from an ISR.

### 2.3 Priority Inheritance

Priority inheritance is a mechanism used by FreeRTOS mutexes to reduce priority inversion.

When a high-priority task blocks while waiting for a mutex owned by a lower-priority task, the mutex owner temporarily inherits the higher priority.

Example:

| Task | Priority |
|---|---:|
| Task_A | 1 |
| Task_B | 2 |
| Task_C | 3 |

If Task_A owns a mutex and Task_C blocks waiting for it:

- Task_A temporarily inherits priority 3.
- Task_B cannot preempt Task_A while Task_A is Ready.
- When Task_A releases the mutex, its priority can return to 1.

**A binary semaphore does not implement this priority inheritance mechanism.**

## 3. Experiment Architecture

We implemented three FreeRTOS tasks.

| Task | Priority | Responsibility |
|---|---:|---|
| Task_A | 1 | Acquires and holds binary semaphore |
| Task_B | 2 | Medium-priority task |
| Task_C | 3 | Attempts to acquire binary semaphore |

Global variables:

```c
SemaphoreHandle_t binarySem;

TaskHandle_t taskAHandle;
TaskHandle_t taskBHandle;
TaskHandle_t taskCHandle;

uint32_t sharedCounter = 0;
uint32_t taskBexecutioncounter = 0;
uint32_t debugHold = 0;
uint32_t taskAEffectivePriority = 0;
```

Task notifications coordinate the execution sequence.

The binary semaphore protects the shared resource, while task notifications synchronize the tasks.

## 4. Implementation

### 4.1 Binary Semaphore Initialization

```c
binarySem = xSemaphoreCreateBinary();

if (binarySem == NULL)
{
    while (1) {}
}

xSemaphoreGive(binarySem);
```

The initial `xSemaphoreGive()` makes the semaphore available for Task_A.

### 4.2 Stack Overflow Hook

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();

    while (1) {}
}
```

### 4.3 Task_A — Low-priority task

```c
void Task_A(void *pvParameters)
{
    (void)pvParameters;

    if (xSemaphoreTake(binarySem, portMAX_DELAY) == pdTRUE)
    {
        sharedCounter++;

        xTaskNotifyGive(taskCHandle);
        xTaskNotifyGive(taskBHandle);

        taskAEffectivePriority = uxTaskPriorityGet(NULL);

        debugHold = 1;

        while (debugHold == 1)
        {
            /* Keep Task_A available for GDB inspection */
        }

        xSemaphoreGive(binarySem);
    }

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

Explanation:

1. Task_A acquires the binary semaphore.
2. It increments `sharedCounter`.
3. It notifies Task_C.
4. Task_C wakes and attempts to acquire the binary semaphore.
5. Task_C blocks because Task_A already holds it.
6. Task_A continues with its original priority of 1.
7. Task_A notifies Task_B.
8. Task_B becomes Ready and preempts Task_A because its priority is 2.
9. Task_A resumes after Task_B blocks again.
10. Task_A enters `debugHold` and waits for GDB.
11. After GDB clears `debugHold`, Task_A gives the binary semaphore.

Unlike the mutex experiment, Task_A does not inherit priority 3.

### 4.4 Task_B — Medium-priority task

```c
void Task_B(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        taskBexecutioncounter++;

        for (volatile uint32_t i = 0; i < 1000000; i++)
        {
            /* Controlled CPU work */
        }
    }
}
```

Task_B initially blocks while waiting for a notification.

After receiving a notification:

1. Task_B becomes Ready.
2. Since its priority is 2 and Task_A's priority is 1, Task_B preempts Task_A.
3. Task_B increments `taskBexecutioncounter`.
4. It performs controlled CPU work.
5. It blocks again, waiting for another notification.

This demonstrates that a medium-priority task can delay a low-priority task holding a binary semaphore.

### 4.5 Task_C — High-priority task

```c
void Task_C(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (xSemaphoreTake(binarySem, portMAX_DELAY) == pdTRUE)
        {
            sharedCounter++;

            xSemaphoreGive(binarySem);
        }
    }
}
```

Task_C waits for a notification from Task_A.

After receiving the notification:

1. Task_C wakes.
2. It attempts to acquire the binary semaphore.
3. The semaphore is already held by Task_A.
4. Task_C becomes Blocked.
5. Task_A's priority remains 1 because binary semaphores do not provide priority inheritance.

When Task_A releases the semaphore, Task_C can acquire it and increment the shared counter.

## 5. Task Creation

The three tasks are created with different priorities:

```c
xTaskCreate(Task_A, "taskA", 256, NULL, 1, &taskAHandle);

xTaskCreate(Task_B, "taskB", 256, NULL, 2, &taskBHandle);

xTaskCreate(Task_C, "taskC", 256, NULL, 3, &taskCHandle);
```

Start the scheduler:

```c
vTaskStartScheduler();
```

Note: FreeRTOS stack depth is specified in `StackType_t` words, not bytes. On our 32-bit Cortex-M4 target, 256 words represents 1024 bytes.

## 6. Complete Execution Flow

The experiment follows this sequence:

1. Task_C starts and waits for a notification.
2. Task_B starts and waits for a notification.
3. Task_A starts and acquires the binary semaphore.
4. Task_A increments `sharedCounter`.
5. Task_A notifies Task_C.
6. Task_C wakes and attempts to acquire the binary semaphore.
7. Task_C blocks because Task_A holds the semaphore.
8. Task_A continues with priority 1.
9. Task_A notifies Task_B.
10. Task_B becomes Ready at priority 2.
11. Task_B preempts Task_A.
12. Task_B increments `taskBexecutioncounter`.
13. Task_B completes its busy loop and blocks again.
14. Task_A resumes and enters `debugHold`.
15. GDB verifies that Task_A's priority is still 1.
16. GDB clears `debugHold`.
17. Task_A releases the binary semaphore.
18. Task_C acquires the semaphore and increments `sharedCounter`.

## 7. Build and QEMU Execution

Update the Makefile to select:

```makefile
SRC = startup.c \
      binary_semaphore_vs_mutex.c \
      ...
```

Build:

```bash
make clean && make
```

Start QEMU in debug mode:

```bash
qemu-system-arm -M mps2-an386 \
-kernel firmware.elf \
-nographic -monitor none \
-S -gdb tcp::1234
```

Connect GDB in another terminal:

```bash
gdb-multiarch firmware.elf
```

Connect:

```gdb
target remote :1234
```

Set a breakpoint in Task_A:

```gdb
break Task_A
continue
```

Continue until execution reaches the `debugHold` loop.

## 8. GDB Validation

### 8.1 During Binary Semaphore Hold

Inspect:

```gdb
p taskAEffectivePriority
p taskBexecutioncounter
p sharedCounter
p uxTaskPriorityGet(taskAHandle)
p debugHold
```

Actual observed values:

| Variable | Actual value |
|---|---:|
| `taskAEffectivePriority` | 1 |
| `taskBexecutioncounter` | 1 |
| `sharedCounter` | 1 |
| Task_A effective priority | 1 |
| `debugHold` | 1 |

Interpretation:

- Task_A acquired the binary semaphore.
- Task_C blocked while waiting for it.
- Task_A remained at priority 1.
- Task_B executed once and incremented its counter.
- Task_A resumed after Task_B blocked again.

This confirms that the binary semaphore did not provide priority inheritance.

### 8.2 Release Task_A

From GDB:

```gdb
set var debugHold = 0
continue
```

Task_A exits the hold loop and gives the binary semaphore.

Inspect:

```gdb
p sharedCounter
p taskBexecutioncounter
p uxTaskPriorityGet(taskAHandle)
```

Actual observed values:

| Variable | Actual value |
|---|---:|
| `sharedCounter` | 2 |
| `taskBexecutioncounter` | 1 |
| Task_A effective priority | 1 |

Interpretation:

- Task_A incremented the shared counter once.
- Task_B executed once.
- Task_C subsequently acquired the binary semaphore and incremented the shared counter.
- Task_A retained its original priority of 1.

The experiment completed successfully.

## 9. Comparison: Mutex vs Binary Semaphore

This table compares the actual results from Topic 11 and Topic 12.

| Observation | Mutex | Binary semaphore |
|---|---|---|
| Task_A base priority | 1 | 1 |
| Task_B priority | 2 | 2 |
| Task_C priority | 3 | 3 |
| Task_A owns resource | Yes | Holds semaphore |
| Task_C blocks | Yes | Yes |
| Task_A priority after Task_C blocks | 3 | 1 |
| Task_B executes before Task_A releases resource | No | Yes |
| Priority inheritance | Supported | Not supported |
| Ownership semantics | Yes | No |

### Mutex experiment

Task_A inherited priority 3 when Task_C blocked waiting for the mutex.

Task_B could not preempt Task_A while Task_A was Ready at effective priority 3.

### Binary semaphore experiment

Task_A remained at priority 1 when Task_C blocked waiting for the binary semaphore.

Task_B at priority 2 could preempt Task_A.

Consequently, Task_C experienced additional delay before the semaphore became available.

## 10. Important Learnings

1. A newly created binary semaphore is initially empty.
2. `xSemaphoreGive()` makes the binary semaphore available.
3. Binary semaphores can be used for event signalling and basic resource locking.
4. Binary semaphores do not provide mutex ownership semantics.
5. Binary semaphores do not support priority inheritance.
6. A medium-priority task can preempt a low-priority task holding a binary semaphore.
7. A high-priority task waiting for that semaphore can consequently experience additional delay.
8. Mutexes provide ownership and priority inheritance.
9. Task notifications are useful for efficient task-to-task synchronization.
10. GDB can verify actual task scheduling behavior and effective task priorities.

## 11. Interview Questions

**Q1. What is the difference between a mutex and a binary semaphore?**

A mutex provides ownership and priority inheritance, whereas a binary semaphore is primarily used for synchronization and event signalling.

**Q2. Does a binary semaphore support priority inheritance?**

No. FreeRTOS binary semaphores do not implement priority inheritance.

**Q3. Can a binary semaphore be used to protect a shared resource?**

Yes, it can be used as a basic lock, but it does not provide mutex ownership or priority inheritance. A mutex is generally preferable for protecting resources shared between tasks with different priorities.

**Q4. Why can Task_B preempt Task_A in the binary semaphore experiment?**

Because Task_A remains at priority 1. Task_B has priority 2 and is Ready, so the scheduler selects Task_B.

**Q5. Why does Task_C remain blocked?**

Because Task_A still holds the binary semaphore. Task_C must wait until the semaphore becomes available.

**Q6. Can a binary semaphore be given from an ISR?**

Yes. Use `xSemaphoreGiveFromISR()` and the appropriate ISR yield mechanism.

**Q7. Can a mutex be given from an ISR?**

No. FreeRTOS mutex APIs cannot be used from an ISR.

**Q8. Which mechanism should be used to protect a shared resource between tasks with different priorities?**

A mutex is generally the appropriate choice because it provides ownership and priority inheritance.

## 12. Conclusion

Successfully implemented and validated a FreeRTOS binary semaphore versus mutex experiment on the Cortex-M4 QEMU platform.

The experiment demonstrated that:

- Task_A acquired and held the binary semaphore.
- Task_C blocked while waiting for the semaphore.
- Task_A remained at its original priority of 1.
- Task_B preempted Task_A and executed once.
- Task_A released the semaphore.
- Task_C subsequently acquired it and completed its operation.

The results were compared against the previous mutex experiment, demonstrating why priority inheritance is important when protecting shared resources between tasks with different priorities.

**Environment:** Cortex-M4, QEMU `mps2-an386`, FreeRTOS V10.4.3, ARM GCC, GDB.

**Next topic:** FreeRTOS Event Groups — synchronization of multiple tasks using event bits.