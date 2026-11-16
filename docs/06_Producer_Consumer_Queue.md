# FreeRTOS Producer–Consumer Communication Using Queues

## 1. Objective

Implement communication between two FreeRTOS tasks using a queue.

- Producer task generates sensor readings.
- Consumer task receives and processes sensor readings.
- FreeRTOS queue provides thread-safe communication between the tasks.

## 2. Application Architecture

The application consists of three main components:

1. **Producer Task:** Generates sensor readings periodically.
2. **FreeRTOS Queue:** Stores sensor readings until they are consumed.
3. **Consumer Task:** Receives and processes sensor readings.

Data flow:

`Producer Task → FreeRTOS Queue → Consumer Task`

## 3. Sensor Data Structure

```c
typedef struct
{
    uint16_t sensor_id;
    uint32_t timestamp;
    int16_t temperature;
} SensorData;
```

The structure represents one sensor reading.

## 4. Queue Creation

FreeRTOS provides:

```c
QueueHandle_t xQueueCreate(
    UBaseType_t uxQueueLength,
    UBaseType_t uxItemSize
);
```

Example:

```c
QueueHandle_t sensorQueue;

sensorQueue = xQueueCreate(10, sizeof(SensorData));
```

Parameters:

- `uxQueueLength`: Maximum number of items the queue can hold.
- `uxItemSize`: Size of each item in bytes.

For this application, the queue can hold 10 sensor readings.

Always check whether queue creation succeeded:

```c
if (sensorQueue == NULL)
{
    /* Queue creation failed */
}
```

`xQueueCreate()` dynamically allocates memory for the queue.

## 5. Sending Data to the Queue

API:

```c
xQueueSend(queue_handle, item_address, timeout);
```

Example:

```c
SensorData sensor;

sensor.sensor_id = 1;
sensor.timestamp = 100;
sensor.temperature = 25;

xQueueSend(sensorQueue, &sensor, pdMS_TO_TICKS(100));
```

Important points:

- Pass the address of the data using `&sensor`.
- FreeRTOS copies the item into its own queue storage.
- The producer can safely modify its original structure after a successful send.
- The return value should be checked using `pdPASS`.

If the queue is full, the producer can wait for space for the specified timeout.

## 6. Receiving Data from the Queue

API:

```c
xQueueReceive(queue_handle, destination_address, timeout);
```

Example:

```c
SensorData receivedSensor;

if (xQueueReceive(sensorQueue,
                  &receivedSensor,
                  portMAX_DELAY) == pdPASS)
{
    /* Process received sensor data */
}
```

Important points:

- FreeRTOS copies the queued item into the destination variable.
- The consumer can modify its local received structure.
- `portMAX_DELAY` requests an indefinite wait when supported by the FreeRTOS configuration.
- If the queue is empty, the consumer can enter the Blocked state.

## 7. Queue Data Copying

FreeRTOS queues copy items rather than retaining the address of the original variable.

For ordinary structures, the complete structure is copied.

However, if a structure contains a pointer:

```c
typedef struct
{
    uint8_t *data;
    uint16_t length;
} Packet;
```

FreeRTOS copies the pointer value and length, not the memory pointed to by `data`.

This distinction is important when handling DMA buffers, UART data and other shared memory.

## 8. Blocking and Queue Behaviour

### Queue empty

- Consumer calls `xQueueReceive()`.
- No data is available.
- Consumer enters Blocked state.
- Producer sends data.
- Consumer moves from Blocked to Ready.
- Scheduler selects the consumer when appropriate.

### Queue full

- Producer calls `xQueueSend()`.
- No space is available.
- Producer waits for the specified timeout.
- Consumer receives an item and frees a slot.
- Producer can become Ready and complete the send.
- If space does not become available before the timeout, the send fails.

## 9. Task Scheduling and Priorities

FreeRTOS uses priority-based scheduling.

Example:

| Task | Priority |
|---|---:|
| Producer | 2 |
| Consumer | 3 |

A higher numerical priority means a higher task priority.

If the consumer is Blocked and the producer sends data:

1. Consumer becomes Ready.
2. Scheduler identifies that consumer has a higher priority.
3. Consumer can preempt the producer.

The actual context switch is handled through the FreeRTOS scheduler and Cortex-M PendSV mechanism.

## 10. Delaying Tasks

Use:

```c
vTaskDelay(pdMS_TO_TICKS(10));
```

`pdMS_TO_TICKS()` converts milliseconds into RTOS ticks.

Advantages over busy waiting:

- Allows other Ready tasks to execute.
- Avoids wasting CPU cycles.
- Integrates with FreeRTOS scheduling.

For periodic tasks requiring a more consistent execution period, FreeRTOS also provides `vTaskDelayUntil()`.

## 11. Error Handling

Always check the return value of queue operations.

Example:

```c
if (xQueueSend(sensorQueue,
               &sensor,
               pdMS_TO_TICKS(100)) == pdPASS)
{
    /* Send successful */
}
else
{
    /* Handle failed transmission */
}
```

Possible application-level responses:

- Increment a failure counter.
- Log an error.
- Discard the current sample.
- Apply a retry strategy.
- Adjust producer or consumer throughput.

A queue does not automatically resolve a sustained producer-consumer throughput mismatch. If the producer continuously generates data faster than the consumer processes it, the queue will eventually fill.

## 12. Key Learning Outcomes

Topics covered:

- `QueueHandle_t`
- `xQueueCreate()`
- `xQueueSend()`
- `xQueueReceive()`
- `xTaskCreate()`
- Task handles and task parameters
- Queue blocking and timeouts
- Queue-full and queue-empty handling
- Structure copying versus pointer copying
- Task states: Blocked, Ready and Running
- Priority-based preemptive scheduling
- `vTaskDelay()` and `pdMS_TO_TICKS()`
- Basic producer-consumer architecture

## 13. Practical Application

This producer-consumer pattern is commonly used in embedded firmware:

- Sensor acquisition and processing
- UART receive and parsing tasks
- DMA data processing
- Interrupt-to-task communication
- Display data processing
- Hardware driver and application-layer communication
