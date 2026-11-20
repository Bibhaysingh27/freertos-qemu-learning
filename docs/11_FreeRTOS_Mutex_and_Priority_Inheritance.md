# 11. FreeRTOS Mutex and Priority Inheritance

## 1. Objective

Understand how FreeRTOS mutexes provide mutual exclusion and how priority inheritance helps prevent unbounded priority inversion.

This experiment uses three tasks with different priorities to demonstrate:

- Mutex creation and acquisition.
- Task synchronization using task notifications.
- Priority inversion.
- Priority inheritance.
- Priority restoration after mutex release.
- Runtime verification using QEMU and GDB.

## 2. Important Concepts

### 2.1 Race Condition

A race condition occurs when multiple tasks access shared data concurrently and the final result depends on their execution order.

Example:

Suppose `sharedcounter = 10`.

Two tasks execute:

```c
sharedcounter++;
```

Possible execution:

1. Task A reads 10.
2. Task B reads 10.
3. Task A writes 11.
4. Task B writes 11.

Expected result: 12.

Actual result: 11.

This is a lost update caused by concurrent access to shared data.

### 2.2 Mutex

A mutex (Mutual Exclusion) protects a shared resource by allowing only one task to own it at a time.

Basic usage:

```c
if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
{
    // Access shared resource

    xSemaphoreGive(mutex);
}
```

Important characteristics:

- A mutex has ownership.
- Only the task that owns the mutex should release it.
- A task can block while owning a mutex.
- FreeRTOS mutexes support priority inheritance.
- Mutex APIs must not be called from an ISR.

### 2.3 Priority Inversion

Priority inversion occurs when a high-priority task is indirectly delayed by a lower-priority task.

Example:

| Task | Priority | Role |
|---|---:|---|
| Task_A | 1 | Low-priority task owning mutex |
| Task_B | 2 | Medium-priority task |
| Task_C | 3 | High-priority task waiting for mutex |

Without priority inheritance:

1. Task_A acquires the mutex.
2. Task_C attempts to acquire the same mutex and blocks.
3. Task_B becomes Ready.
4. Task_B preempts Task_A because its priority is higher.
5. Task_A cannot release the mutex.
6. Task_C remains blocked.

The medium-priority task indirectly delays the high-priority task.

### 2.4 Priority Inheritance

FreeRTOS mutexes implement priority inheritance.

When a high-priority task blocks while waiting for a mutex owned by a lower-priority task, the mutex owner temporarily inherits the higher priority.

In our example:

- Task_A base priority = 1.
- Task_C priority = 3.
- Task_A temporarily inherits priority 3.
- Task_B has priority 2 and cannot preempt Task_A while Task_A is Ready.
- When Task_A releases the mutex, its priority returns to its original value, provided no other mutex ownership requires inheritance.

This helps prevent medium-priority tasks from unnecessarily delaying the mutex owner.

---

## 3. Experiment Architecture

We implemented three tasks:

| Task | Base priority | Responsibility |
|---|---:|---|
| Task_A | 1 | Acquires and owns the mutex |
| Task_B | 2 | Medium-priority task |
| Task_C | 3 | Waits for the mutex |

Global variables:

```c
uint32_t sharedcounter = 0;
uint32_t taskBExecutionCount = 0;
uint32_t taskAEffectivePriority = 0;

volatile uint32_t debugHold = 0;

SemaphoreHandle_t mutex;

TaskHandle_t taskAhandle;
TaskHandle_t taskBhandle;
TaskHandle_t taskChandle;
```

Task notifications are used to coordinate the execution sequence.

The mutex provides mutual exclusion; task notifications provide synchronization.

## 4. Implementation

### 4.1 Create the mutex

```c
mutex = xSemaphoreCreateMutex();

if (mutex == NULL)
{
    while (1) {}
}
```

`xSemaphoreCreateMutex()` creates a mutex and returns its handle.

If creation fails, it returns `NULL`.

### 4.2 Task_A — Low-priority task

```c
void Task_A(void *pvParameters)
{
    (void)pvParameters;

    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS)
    {
        sharedcounter++;

        xTaskNotifyGive(taskChandle);

        xTaskNotifyGive(taskBhandle);

        taskAEffectivePriority = uxTaskPriorityGet(NULL);

        debugHold = 1;

        while (debugHold == 1)
        {
            /* Hold Task_A for GDB inspection */
        }

        xSemaphoreGive(mutex);
    }

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

Explanation:

1. Task_A acquires the mutex.
2. It increments `sharedcounter`.
3. It notifies Task_C.
4. Task_C wakes and attempts to acquire the mutex.
5. Since Task_A owns the mutex, Task_C blocks.
6. Task_A inherits priority 3.
7. Task_A notifies Task_B.
8. Task_B becomes Ready, but cannot preempt Task_A.
9. Task_A waits in `debugHold` while retaining the mutex.
10. GDB can inspect the priority inheritance state.
11. After releasing `debugHold`, Task_A releases the mutex.

### 4.3 Task_B — Medium-priority task

```c
void Task_B(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        taskBExecutionCount++;
    }
}
```

Task_B initially blocks while waiting for a notification.

When Task_A notifies it, Task_B becomes Ready.

However, Task_A has inherited priority 3, so Task_B at priority 2 cannot preempt it.

### 4.4 Task_C — High-priority task

```c
void Task_C(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
            {
                sharedcounter++;

                xSemaphoreGive(mutex);
            }
        }
    }
}
```

Task_C waits for a notification from Task_A.

Once notified:

1. Task_C wakes.
2. It attempts to acquire the mutex.
3. The mutex is already owned by Task_A.
4. Task_C blocks.
5. FreeRTOS raises Task_A's effective priority to 3.

When Task_A releases the mutex, Task_C can acquire it and increment the shared counter.

## 5. Task Creation

Tasks are created with the following priorities:

```c
xTaskCreate(Task_A, "taskA", 256, NULL, 1, &taskAhandle);

xTaskCreate(Task_B, "taskB", 256, NULL, 2, &taskBhandle);

xTaskCreate(Task_C, "taskC", 256, NULL, 3, &taskChandle);
```

The scheduler is started using:

```c
vTaskStartScheduler();
```

Note: FreeRTOS stack depth is specified in `StackType_t` words, not bytes. On our 32-bit Cortex-M4 target, 256 words represents 1024 bytes.

## 6. Complete Execution Flow

The experiment follows this sequence:

1. Task_C starts and waits for a notification.
2. Task_B starts and waits for a notification.
3. Task_A starts and acquires the mutex.
4. Task_A increments `sharedcounter`.
5. Task_A notifies Task_C.
6. Task_C wakes and attempts to acquire the mutex.
7. Task_C blocks because Task_A owns the mutex.
8. Task_A inherits priority 3.
9. Task_A notifies Task_B.
10. Task_B becomes Ready at priority 2.
11. Task_A continues running because its effective priority is 3.
12. Task_A waits in `debugHold`.
13. GDB verifies the inherited priority.
14. GDB clears `debugHold`.
15. Task_A releases the mutex.
16. Task_A returns to its original priority 1.
17. Task_C acquires the mutex and increments `sharedcounter`.
18. Task_B executes after receiving its notification.

## 7. FreeRTOS Configuration

Two configuration settings were required to enable the APIs used in this experiment:

```c
#define configUSE_MUTEXES         1
#define INCLUDE_uxTaskPriorityGet 1
```

Without these settings, mutex creation and priority inspection APIs may not be available.

## 8. Build and QEMU Execution

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

Connect using GDB in another terminal:

```bash
gdb-multiarch firmware.elf
```

Connect to QEMU:

```gdb
target remote :1234
```

Set a breakpoint:

```gdb
break Task_A
continue
```

Continue execution until Task_A reaches the `debugHold` loop.

Press Ctrl+C to interrupt execution while Task_A is held in the loop.

## 9. GDB Validation

### 9.1 During Priority Inheritance

Inspect:

```gdb
p taskAEffectivePriority
p taskBExecutionCount
p sharedcounter
p debugHold
p uxTaskPriorityGet(taskAhandle)
```

Expected observations:

| Variable | Expected value |
|---|---:|
| `taskAEffectivePriority` | 3 |
| `taskBExecutionCount` | 0 |
| `sharedcounter` | 1 |
| `debugHold` | 1 |
| Task_A effective priority | 3 |

These observations confirm:

- Task_A owns the mutex.
- Task_C is blocked waiting for the mutex.
- Task_A has inherited priority 3.
- Task_B has not executed yet.

### 9.2 Release Task_A

From GDB:

```gdb
set var debugHold = 0
continue
```

Task_A exits the hold loop and releases the mutex.

Inspect:

```gdb
p sharedcounter
p taskBExecutionCount
p uxTaskPriorityGet(taskAhandle)
```

Actual observed values:

| Variable | Actual value |
|---|---:|
| `sharedcounter` | 2 |
| `taskBExecutionCount` | 1 |
| Task_A effective priority | 1 |

### 9.3 Interpretation

`sharedcounter = 2`:

- Task_A incremented it once.
- Task_C incremented it once after acquiring the mutex.

`taskBExecutionCount = 1`:

- Task_B executed after receiving its notification.

Task_A priority = 1:

- Task_A returned to its original priority after releasing the mutex.

The experiment successfully demonstrated priority inheritance and priority restoration.

## 10. Important Learnings

1. A mutex provides mutual exclusion for shared resources.
2. A mutex has ownership; a binary semaphore does not provide mutex ownership semantics.
3. A task can be Blocked while still owning a mutex.
4. Priority inheritance changes effective priority, not task state.
5. A Blocked task cannot execute, regardless of its priority.
6. FreeRTOS temporarily raises a mutex owner's priority when a higher-priority task blocks on that mutex.
7. The mutex owner can return to its original priority after releasing the mutex.
8. Task notifications can be used for efficient task-to-task synchronization.
9. GDB is useful for inspecting task execution and verifying scheduler behavior.

## 11. Interview Questions

**Q1. What is priority inversion?**

When a high-priority task is indirectly delayed because a lower-priority task holds a resource it needs, while medium-priority tasks prevent the lower-priority task from running.

**Q2. How does priority inheritance solve priority inversion?**

The lower-priority mutex owner temporarily inherits the priority of the highest-priority task waiting for that mutex.

**Q3. Can a task be Blocked while holding a mutex?**

Yes. A task can block or delay while holding a mutex, although doing so can create unnecessary delays for other tasks.

**Q4. Does priority inheritance change the task state?**

No. It changes the task's effective priority, not its state.

**Q5. Can a mutex be used inside an ISR?**

No. FreeRTOS mutex APIs must not be used from an ISR.

**Q6. What happens after the mutex owner releases the mutex?**

Priority inheritance is removed as appropriate, and the task's effective priority can return to its original priority.

**Q7. Why is a mutex different from a binary semaphore?**

A mutex provides ownership and priority inheritance. A binary semaphore is primarily used for synchronization and event signalling and does not provide those mutex semantics.

---

## 12. Conclusion

Successfully implemented and validated a three-task FreeRTOS mutex priority-inheritance experiment on the Cortex-M4 QEMU platform.

Verified that:

- Task_C blocks while waiting for Task_A's mutex.
- Task_A inherits priority 3 from Task_C.
- Task_B cannot preempt Task_A while Task_A is Ready at priority 3.
- Task_A returns to priority 1 after releasing the mutex.
- Task_C subsequently acquires the mutex.

**Environment:** Cortex-M4, QEMU `mps2-an386`, FreeRTOS V10.4.3, ARM GCC, GDB.

**Next experiment:** Compare mutex priority inheritance against a binary semaphore, which does not provide priority inheritance.
