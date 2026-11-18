# 09 — FreeRTOS Binary Semaphore: Event Signaling & QEMU Validation

## 1. Objective

Implement and validate a FreeRTOS binary semaphore for synchronizing two tasks:

- `Event_task` periodically generates an event.
- `Uart_task` waits for that event and processes it.

The purpose was to understand task blocking, semaphore signaling, task wake-up, and runtime validation using QEMU and GDB.

---

## 2. Binary Semaphore Concept

A binary semaphore has two states:

- **Available (1):** A task can successfully take it.
- **Unavailable (0):** A task attempting to take it will block.

Unlike a counting semaphore, a binary semaphore does not accumulate multiple tokens. If it is already available, another give operation does not increase its count.

A binary semaphore is useful for signaling an event, such as:

- UART data received
- DMA transfer completed
- GPIO event
- Sensor data ready
- Hardware operation completed

A semaphore signals that an event occurred; it does not carry the actual data.

---

## 3. Application Architecture

### Event_task

The event task waits for 2 seconds and then gives the semaphore.

```c
void Event_task(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));

        if(xSemaphoreGive(uartsemaphore) == pdTRUE)
        {
            eventGenerateCount++;
        }
    }
}
```

### Uart_task

The UART task blocks while waiting for the semaphore.

```c
void Uart_task(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        if(xSemaphoreTake(uartsemaphore, portMAX_DELAY) == pdTRUE)
        {
            uartEventCount++;

            // Process the event
        }
    }
}
```

`portMAX_DELAY` allows the task to wait indefinitely until the semaphore becomes available, subject to the FreeRTOS configuration.

The task does not continuously poll the semaphore. It enters the Blocked state while waiting.

---

## 4. Semaphore Creation

The binary semaphore is created in `main()`:

```c
uartsemaphore = xSemaphoreCreateBinary();

if(uartsemaphore == NULL)
{
    while(1)
    {
    }
}
```

A newly created binary semaphore is initially empty.

Therefore, when the scheduler starts, `Uart_task` cannot immediately take the semaphore and will block until `Event_task` gives it.

---

## 5. Task Creation

Both tasks are created with priority 2 and a stack depth of 256.

```c
xTaskCreate(Uart_task, "Uart", 256, NULL, 2, &uarthandle);

xTaskCreate(Event_task, "Event", 256, NULL, 2, &eventhandle);
```

Both tasks have the same priority.

The Event task periodically delays, allowing the UART task to execute when it becomes Ready.

---

## 6. Event Flow

The expected synchronization sequence is:

1. Scheduler starts.
2. `Uart_task` attempts to take the empty semaphore.
3. `Uart_task` becomes Blocked.
4. `Event_task` executes its delay.
5. Event task wakes after approximately 2 seconds.
6. Event task calls `xSemaphoreGive()`.
7. Semaphore becomes available.
8. The waiting UART task becomes Ready.
9. UART task runs and successfully takes the semaphore.
10. `uartEventCount` increments.
11. UART task waits for the next event.

The process repeats continuously.

---

## 7. Binary Semaphore vs Queue

| Binary Semaphore | Queue |
|---|---|
| Signals an event | Transfers data |
| Does not carry application data | Can carry items |
| Token state is available/unavailable | Stores queued items |
| Multiple signals may coalesce | Multiple items can accumulate, subject to capacity |
| Useful for event notification | Useful for producer-consumer data transfer |

For a real UART implementation, the ISR or driver would store received bytes in a buffer/ring buffer. The semaphore would notify the task that data needs processing.

---

## 8. Task Context vs ISR Context

Task context:

```c
xSemaphoreGive(uartsemaphore);
```

Interrupt context:

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

xSemaphoreGiveFromISR(uartsemaphore, &xHigherPriorityTaskWoken);

portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

The ISR API must be used from interrupt context.

The ISR should perform minimal work and defer processing to a task.

**Important:** The `UART_IRQHandler()` in this exercise is currently a stub. It is not connected to a real UART peripheral or an active interrupt vector. This milestone simulates event generation from a task context using `Event_task`.

---

## 9. Runtime Validation with QEMU and GDB

QEMU was used to run the firmware on the Cortex-M4 MPS2-AN386 target.

GDB was used to:

- Break inside `Event_task`
- Break inside `Uart_task`
- Observe task execution
- Set hardware watchpoints on application counters
- Confirm counter changes during runtime

Watchpoints:

```gdb
watch uartEventCount
watch eventGenerateCount
```

These watchpoints allow GDB to stop when the corresponding global variables are written.

---

## 10. Runtime Observations

During debugging:

- `Event_task` was observed executing.
- `Uart_task` reached `xSemaphoreTake()`.
- Event generation was observed through `eventGenerateCount`.
- UART task processing was observed through `uartEventCount`.
- Both counters were observed reaching 34.

Final observed values:

```text
eventGenerateCount = 34
uartEventCount     = 34
```

This confirms that at the final observation point, the number of successfully generated semaphore events and processed events matched.

---

## 11. Stack Overflow Hook

The application provides the required FreeRTOS stack overflow hook:

```c
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();

    while(1)
    {
    }
}
```

The hook halts the system if a stack overflow is detected.

The task handle and task name are currently unused. They can later be inspected through debugging or used for diagnostics.

---

## 12. Key Learnings

- A binary semaphore is useful for event signaling.
- A binary semaphore does not accumulate multiple signals.
- A task waiting on an empty semaphore becomes Blocked.
- Giving the semaphore can unblock a waiting task.
- `xSemaphoreTake()` is not busy polling when the task blocks correctly.
- Task-context and ISR-context APIs must be used appropriately.
- A semaphore does not transfer data.
- GDB breakpoints help inspect execution flow.
- GDB watchpoints help observe real runtime changes in variables.
- Runtime validation is important in addition to successful compilation.

---

## 13. Milestone Status

**Completed:**

- Binary semaphore created successfully.
- Event task implemented.
- UART processing task implemented.
- Tasks created successfully.
- Firmware compiled and linked.
- Firmware executed in QEMU.
- Task execution observed through GDB.
- Application counters observed through GDB watchpoints.
- Final counters matched at 34.

**Limitation:** Event generation was simulated from task context. A real UART ISR and peripheral integration are not part of this milestone.

---

## Next Topic

Counting Semaphores — understanding how multiple events/tokens can be accumulated, unlike binary semaphore signaling.
