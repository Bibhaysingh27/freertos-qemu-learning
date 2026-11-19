# 10 — FreeRTOS Counting Semaphore: Implementation & QEMU Validation

## 1. Objective

Implement and validate a FreeRTOS counting semaphore using two tasks:

- `Generator_Task`: Generates events periodically.
- `Processing_Task`: Waits for events and processes them more slowly.

The objective is to understand how counting semaphores accumulate multiple pending events, how their maximum count works, and how they differ from binary semaphores.

---

## 2. Counting Semaphore Concept

A counting semaphore maintains a count of available tokens.

Unlike a binary semaphore, which can hold only one token, a counting semaphore can accumulate multiple tokens up to a configured maximum.

Example:

```c
sem = xSemaphoreCreateCounting(5, 0);
```

Parameters:

- Maximum count = 5
- Initial count = 0

The semaphore starts empty and can hold up to five tokens.

### Counting Semaphore Behavior

| Operation | Semaphore count |
|---|---:|
| Initial state | 0 |
| First successful give | 1 |
| Second successful give | 2 |
| Third successful give | 3 |
| Fourth successful give | 4 |
| Fifth successful give | 5 |
| Additional give | Remains 5; returns `pdFALSE` |
| Successful take | Decreases by 1 |

---

## 3. Application Architecture

The exercise uses two FreeRTOS tasks.

### Generator_Task

Generates an event every 500 milliseconds.

```c
void Generator_Task(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));

        if(xSemaphoreGive(sem) == pdTRUE)
        {
            generatedCount++;
        }
    }
}
```

Responsibilities:

1. Wait for 500 ms.
2. Give one semaphore token.
3. Increment `generatedCount` only when the give succeeds.
4. Repeat continuously.

### Processing_Task

Consumes one token and simulates slow processing.

```c
void Processing_Task(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        if(xSemaphoreTake(sem, portMAX_DELAY) == pdTRUE)
        {
            processedCount++;

            vTaskDelay(pdMS_TO_TICKS(1500));
        }
    }
}
```

Responsibilities:

1. Wait indefinitely for a semaphore token.
2. Take one available token.
3. Increment `processedCount`.
4. Simulate processing for 1500 milliseconds.
5. Repeat continuously.

Because the processing task takes longer than the generator interval, pending events accumulate.

---

## 4. Global Variables

```c
SemaphoreHandle_t sem;

TaskHandle_t generatorhandle;
TaskHandle_t processorhandle;

uint32_t generatedCount = 0;
uint32_t processedCount = 0;
```

The counters are used to observe successful event generation and processing during debugging.

- `generatedCount`: Number of successful semaphore gives.
- `processedCount`: Number of successful semaphore takes.

---

## 5. Main Function

```c
int main(void)
{
    sem = xSemaphoreCreateCounting(5, 0);

    if(sem == NULL)
    {
        while(1)
        {
        }
    }

    if(xTaskCreate(Generator_Task,
                   "Generator",
                   256,
                   NULL,
                   2,
                   &generatorhandle) != pdPASS)
    {
        while(1)
        {
        }
    }

    if(xTaskCreate(Processing_Task,
                   "Processing",
                   256,
                   NULL,
                   2,
                   &processorhandle) != pdPASS)
    {
        while(1)
        {
        }
    }

    vTaskStartScheduler();

    while(1)
    {
        // Scheduler failed to start
    }

    return 0;
}
```

Both tasks use priority 2.

The scheduler starts after successful semaphore and task creation.

---

## 6. FreeRTOS Configuration

Counting semaphore support must be enabled in `FreeRTOSConfig.h`:

```c
#define configUSE_COUNTING_SEMAPHORES 1
```

FreeRTOS counting semaphores use the kernel's queue infrastructure internally.

The application therefore uses the existing FreeRTOS queue implementation.

The application also provides `vApplicationStackOverflowHook()` because stack overflow checking is enabled.

---

## 7. Binary Semaphore vs Counting Semaphore

| Feature | Binary Semaphore | Counting Semaphore |
|---|---|---|
| Maximum count | 1 | Configurable |
| Initial count | Usually 0 | Configurable |
| Multiple pending events | Cannot accumulate beyond 1 | Can accumulate |
| Give operation | Makes token available | Increments count if below maximum |
| Take operation | Consumes one token | Decrements count by one |
| Typical use | Event notification | Multiple pending events |

### Important distinction

A counting semaphore stores the number of pending notifications, not the actual event data.

If every event contains data that must be preserved, a queue or another suitable buffer should be used.

---

## 8. Runtime Timing Configuration

| Parameter | Value |
|---|---:|
| Generator delay | 500 ms |
| Processing delay | 1500 ms |
| Maximum semaphore count | 5 |
| Initial semaphore count | 0 |
| Generator priority | 2 |
| Processing priority | 2 |

The generator produces events approximately three times faster than the processing task consumes them.

Therefore, the semaphore count increases until it reaches its maximum capacity.

---

## 9. QEMU and GDB Validation

The firmware was built using the ARM GCC toolchain and executed on the QEMU MPS2-AN386 Cortex-M4 target.

GDB was used to inspect task execution and monitor application counters.

### Breakpoints

```gdb
break Generator_Task
break Processing_Task
```

These confirmed that both tasks were executing.

The debugger also showed the internal FreeRTOS semaphore take path:

```text
xQueueSemaphoreTake
```

This demonstrated that FreeRTOS implements semaphore operations using its queue infrastructure.

### Watchpoints

The following hardware watchpoints were configured:

```gdb
watch generatedCount
watch processedCount
```

These allowed GDB to stop whenever the application counters changed.

---

## 10. Runtime Observations

During QEMU execution, the following values were observed.

### Observation 1

```text
generatedCount = 2
processedCount = 1
```

Pending events:

```text
2 - 1 = 1
```

One event was waiting to be processed.

### Observation 2

```text
generatedCount = 6
processedCount = 3
```

Pending events:

```text
6 - 3 = 3
```

Three tokens were pending.

### Observation 3 — Maximum Capacity Reached

```text
generatedCount = 9
processedCount = 4
```

Pending events:

```text
9 - 4 = 5
```

The counting semaphore reached its configured maximum capacity of five.

### Summary of Runtime Validation

| Observation | Generated | Processed | Pending tokens |
|---|---:|---:|---:|
| 1 | 2 | 1 | 1 |
| 2 | 6 | 3 | 3 |
| 3 | 9 | 4 | 5 |

The final observation confirmed that the semaphore had accumulated five pending tokens.

**Note:** These are snapshots taken at different debugging times, not a continuous timing trace.

The additional give failure at full capacity was explained from the FreeRTOS API behavior, but was not independently captured as a GDB observation in this experiment.

---

## 11. What Happens When the Semaphore Is Full?

When the semaphore count reaches five:

```c
xSemaphoreGive(sem);
```

returns:

```c
pdFALSE
```

The count remains unchanged.

Since the application increments `generatedCount` only when the give succeeds, the counter does not increase for rejected gives.

When the processing task successfully takes a token:

- Semaphore count decreases from 5 to 4.
- One slot becomes available.
- A later give can succeed again.

This demonstrates the maximum-capacity behavior of a counting semaphore.

---

## 12. Important Engineering Considerations

### Event loss

A counting semaphore can accumulate notifications only up to its maximum count.

If additional events arrive while it is full, those notifications are not stored.

If every event must be preserved, use a queue or another suitable buffering mechanism.

### Semaphore vs data transfer

A semaphore provides synchronization. It does not transfer event payloads.

For example, a DMA completion notification can use a semaphore, while the actual DMA data remains in a memory buffer.

### Counter interpretation

The difference:

```text
generatedCount - processedCount
```

represents pending tokens in this exercise because the generator increments its counter only after a successful give and the processor increments its counter after a successful take.

This relationship assumes no counter wraparound and consistent observation.

---

## 13. Key Learnings

- A counting semaphore can store multiple pending notifications.
- Maximum count and initial count are separate parameters.
- `xSemaphoreCreateCounting()` creates the counting semaphore.
- `xSemaphoreGive()` increments the count if capacity is available.
- `xSemaphoreTake()` consumes one available token.
- `portMAX_DELAY` allows indefinite blocking.
- A give operation fails when the semaphore is already full.
- FreeRTOS implements counting semaphores using queue infrastructure.
- A counting semaphore does not carry application data.
- GDB hardware watchpoints are useful for observing runtime state changes.

---

## 14. Milestone Status

**Completed:**

- Counting semaphore implemented.
- Maximum count configured to 5.
- Initial count configured to 0.
- Generator task implemented.
- Processing task implemented.
- Both tasks created successfully.
- Counting semaphore support enabled in FreeRTOS configuration.
- Firmware compiled and linked.
- Firmware executed in QEMU.
- Both tasks observed through GDB.
- Application counters monitored using hardware watchpoints.
- Semaphore accumulation observed up to five pending tokens.

---

## Next Topic

**FreeRTOS Mutexes and Priority Inversion**

Topics to cover:

- Why mutexes are needed.
- Shared resource protection.
- Mutex vs binary semaphore.
- Mutex ownership.
- Priority inversion.
- Priority inheritance.
- Practical implementation and QEMU debugging.
