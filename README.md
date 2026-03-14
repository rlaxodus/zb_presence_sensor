# AMBI PS-1 — Zigbee Presence Sensor

| Supported Targets | ESP32-H2 | ESP32-C6 | ESP32-C5 |
| ----------------- | -------- | -------- | -------- |

Firmware for the AMBI PS-1 multi-sensor node. Reads from hardware sensors, filters data at the component level, and reports state changes to a Zigbee coordinator via the Home Automation profile.

---

## Project Structure

```
zb_presence_sensor/
├── main/
│   ├── ps1.c               # Application entry point. Creates queues, spawns handler tasks, owns all Zigbee logic.
│   └── ps1.h               # Pin definitions, Zigbee config constants.
└── components/
    ├── uart_component/     # UART sensor template — copy this to start a new UART-based sensor component.
    ├── test_component/     # Reference implementation with simulated data. Use for development/testing.
    └── light_driver/       # LED strip output driver.
```

---

## Architecture

### Core principle

> **Components read hardware and enqueue data. Main decides what to do with it.**

Components are completely unaware of the application. They do not call into `main`, do not touch Zigbee, and do not know about other components. Main owns all application logic.

### Data flow

```
┌──────────────────────────────────────────────────────┐
│                    Hardware / Sensors                │
└───────────┬──────────────────────────────────────────┘
            │  raw frames (UART / I2C / SPI)
            ▼
┌──────────────────────────────────────────────────────┐
│                  Sensor Component                    │
│                                                      │
│  1. Read raw data from hardware                      │
│  2. Apply hardware-level filtering (debounce,        │
│     delta threshold, rate limiting)                  │
│  3. xQueueSend() on meaningful state change          │
│  4. Update internal snapshot for poll (get_latest)   │
└───────────┬──────────────────────────────────────────┘
            │  xQueue (only on real state changes)
            ▼
┌──────────────────────────────────────────────────────┐
│             FreeRTOS Queue (one per component)       │
└───────────┬──────────────────────────────────────────┘
            │  xQueueReceive() in handler task
            ▼
┌──────────────────────────────────────────────────────┐
│                    ps1.c (main)                      │
│                                                      │
│  sensor_handler_task() / uart_handler_task()         │
│  - Sensor fusion (combine readings from 2+ sensors)  │
│  - Application decisions                             │
│  - Update Zigbee attributes                          │
└──────────────────────────────────────────────────────┘
```

### Queue-per-component pattern

Each component receives a `QueueHandle_t` at init time (created by `main`). Main spawns one lightweight handler task per component that blocks on `xQueueReceive`. This keeps component logic isolated and lets each handler run at its own priority.

For high-throughput sensors (e.g. raw radar frames at 100 Hz), size the queue appropriately or process inline — the handler task will naturally absorb bursts without affecting other components.

---

## What belongs where

### Inside a component ✓
- Reading raw bytes from UART / I2C / SPI
- Parsing the hardware protocol
- Debouncing (e.g. presence flickering on/off in <100ms)
- Delta filtering (e.g. only report if temp changed by >0.5°C)
- Rate limiting (e.g. minimum 1s between reports)
- Maintaining an internal snapshot for `get_latest` polling
- Calling `xQueueSend()` on meaningful state changes

### Inside a component ✗ — put this in main
- Logic that involves more than one sensor ("if sensor_a AND sensor_b detect presence...")
- Zigbee attribute updates
- Any decision about what the application does with the data
- Cross-component state

---

## Adding a new sensor component

1. **Copy `uart_component`** into `components/<sensor_name>/`

2. **Rename** every occurrence of `uart_comp` / `uart_data_t` to match your sensor:
   ```
   uart_comp_*   →  mmwave_comp_*
   uart_data_t   →  mmwave_data_t
   ```

3. **Fill in the header** — update `<sensor>_data_t` with the fields your sensor actually produces:
   ```c
   typedef struct {
       bool     presence;     // example field
       float    distance_m;   // example field
       uint32_t timestamp;
   } mmwave_data_t;
   ```

4. **Fill in `component_task`** — follow the four TODOs in order:
   ```c
   // 1. Read from hardware (UART/I2C/SPI)
   // 2. Parse and apply filtering (debounce, delta, rate limit)
   // 3. Update internal_data under mutex  (for get_latest polling)
   // 4. xQueueSend() if conditions are met
   ```

5. **Register the component in CMakeLists**:
   - `components/<sensor_name>/CMakeLists.txt` — already correct if copied from template
   - `main/CMakeLists.txt` — add `<sensor_name>` to `PRIV_REQUIRES`

6. **Create a queue and init the component in `ps1.c`**:
   ```c
   static QueueHandle_t s_mmwave_queue;

   // In app_main, before zigbee init:
   s_mmwave_queue = xQueueCreate(4, sizeof(mmwave_data_t));
   mmwave_comp_init(s_mmwave_queue /*, port, tx_pin, rx_pin */);
   ```

7. **Spawn a handler task in `ps1.c`**:
   ```c
   static void mmwave_handler_task(void *pv) {
       mmwave_data_t data;
       while (1) {
           if (xQueueReceive(s_mmwave_queue, &data, portMAX_DELAY)) {
               // application logic here — update Zigbee attributes, fuse with other sensors, etc.
           }
       }
   }

   // In app_main:
   xTaskCreate(mmwave_handler_task, "mmwave_handler", 4096, NULL, 5, NULL);
   ```

---

## Planned sensors

| Component | Interface | Status |
|---|---|---|
| LD2412 | UART | Planned |
| LD2450 | UART | Planned |
| temp_humidity | I2C | Planned |
| BH1750 | I2C | Planned |

---

## Build & Flash

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p PORT flash monitor
```

Erase NVS before first flash or after changing Zigbee network config:
```bash
idf.py -p PORT erase-flash
```

Factory reset at runtime: hold the BOOT button for 6 seconds.

---

## Hardware

- **Target SoC**: ESP32-C6
- **Zigbee role**: Router
- **Zigbee profile**: Home Automation
- **Manufacturer**: AMBI
- **Model**: PS-1
