# AMBI PS-1 — Zigbee Presence Sensor

| Supported Targets | ESP32-H2 | ESP32-C6 | ESP32-C5 |
| ----------------- | -------- | -------- | -------- |

Firmware for the AMBI PS-1 multi-sensor node. Reads from hardware sensors, filters data at the component level, and reports state changes to a Zigbee coordinator via the Home Automation profile.

---

## Project Structure

```
zb_presence_sensor/
├── main/
│   ├── ps1.c               # Application entry point. Registers event handlers, owns all Zigbee logic.
│   └── ps1.h               # Pin definitions, Zigbee config constants.
└── components/
    ├── base_component/     # Skeleton template — copy this to start a new sensor component.
    ├── test_component/     # Reference implementation with simulated data. Use for development/testing.
    └── light_driver/       # LED strip output driver.
```

---

## Architecture

### Core principle

> **Components read hardware and emit events. Main decides what to do with them.**

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
│  3. esp_event_post() on meaningful state change      │
│  4. Update internal snapshot for poll (get_latest)   │
└───────────┬──────────────────────────────────────────┘
            │  esp_event (only on real state changes)
            ▼
┌──────────────────────────────────────────────────────┐
│               ESP-IDF Default Event Loop             │
└───────────┬──────────────────────────────────────────┘
            │  dispatches to registered handlers
            ▼
┌──────────────────────────────────────────────────────┐
│                    ps1.c (main)                      │
│                                                      │
│  on_sensor_event()                                   │
│  - Sensor fusion (combine readings from 2+ sensors)  │
│  - Application decisions                             │
│  - Update Zigbee attributes                          │
└──────────────────────────────────────────────────────┘
```

### Why `esp_event` and not `xQueue`

`xQueue` is one pipe with one reader — you need one queue per sensor. `esp_event` is a shared bus with a built-in dispatcher. Adding a new sensor means one new `esp_event_handler_register` call in `app_main`. No new tasks, no new queues.

If a sensor is high-throughput (e.g. raw radar frames at 100Hz), give it a **dedicated event loop** (`esp_event_loop_create`) so it cannot starve other sensors on the shared bus. For presence/temp/lux at normal reporting rates the default shared loop is sufficient.

---

## What belongs where

### Inside a component ✓
- Reading raw bytes from UART / I2C / SPI
- Parsing the hardware protocol
- Debouncing (e.g. presence flickering on/off in <100ms)
- Delta filtering (e.g. only report if temp changed by >0.5°C)
- Rate limiting (e.g. minimum 1s between events)
- Maintaining an internal snapshot for `get_latest` polling

### Inside a component ✗ — put this in main
- Logic that involves more than one sensor ("if sensor_a AND sensor_b detect presence...")
- Zigbee attribute updates
- Any decision about what the application does with the data
- Cross-component state

---

## Adding a new sensor component

1. **Copy `base_component`** into `components/<sensor_name>/`

2. **Rename** every occurrence of `base_comp` / `BASE_COMP` / `base_data_t` to match your sensor:
   ```
   base_comp_*      →  mmwave_comp_*
   BASE_COMP_EVENTS →  MMWAVE_EVENTS
   base_data_t      →  mmwave_data_t
   ```

3. **Fill in the header** — update `<sensor>_data_t` with the fields your sensor actually produces:
   ```c
   typedef struct {
       bool  presence;       // example field
       float distance_m;     // example field
       uint32_t timestamp;
   } mmwave_data_t;
   ```

4. **Fill in `component_task`** — follow the four TODOs in order:
   ```c
   // 1. Read from hardware (UART/I2C/SPI)
   // 2. Parse and apply filtering (debounce, delta, rate limit)
   // 3. Update internal_data under mutex  (for get_latest polling)
   // 4. esp_event_post() if conditions are met
   ```

5. **Register the component in CMakeLists**:
   - `components/<sensor_name>/CMakeLists.txt` — already correct if copied from base
   - `main/CMakeLists.txt` — add `<sensor_name>` to `PRIV_REQUIRES`

6. **Register an event handler in `ps1.c`**:
   ```c
   // In app_main, after esp_event_loop_create_default():
   esp_event_handler_register(MMWAVE_EVENTS, MMWAVE_EVENT_DATA_READY, on_mmwave_event, NULL);
   ```

7. **Write the handler in `ps1.c`**:
   ```c
   static void on_mmwave_event(void *arg, esp_event_base_t base, int32_t id, void *event_data) {
       mmwave_data_t *data = (mmwave_data_t *)event_data;
       // application logic here
   }
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
