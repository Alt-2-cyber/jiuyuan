# Ground OLED, Route Distance, and Environment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a one-person-at-a-time OLED UI, two-button navigation, local AHT20/gas display, and backward-compatible multi-hop route metadata, then build all five roles and flash only the ground firmware to COM14.

**Architecture:** Keep transport integration in `sle_relay.c`, move the bounded JR protocol into a platform-independent `rescue_route` module, keep victim parsing/button state in a platform-independent `ground_model`, and isolate all WS63 OLED/AHT20/ADC/GPIO work in a ROOT-only `ground_ui` adapter. Host GCC tests cover protocol/model behavior; Python contract tests cover SDK wiring and role isolation.

**Tech Stack:** C11, WS63 LiteOS SDK, SLE/SSAP, SSD1306, AHT20, ADC V154, OSAL tasks/mutexes, Python `unittest`, MinGW GCC, PowerShell build/burn scripts.

---

The workspace has no Git root, so the commit steps normally required after each task cannot run. Preserve `delivery/firmware` as the rollback checkpoint and build only into a timestamped `delivery/staging` directory.

### Task 1: Pure JR Route Protocol

**Files:**
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/rescue_route.h`
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/rescue_route.c`
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/tests/native/test_rescue_route.c`

- [ ] **Step 1: Write failing protocol tests**

Create tests that decode an old packet as `route_ok=false`, encode a new packet without changing its JSON body, convert all RSSI boundaries, add exactly one hop, allow `ttl=1` to become `ttl=0`, reject an input already at `ttl=0`, and reject output beyond 255 bytes.

```c
static void test_add_final_hop(void)
{
    const char *raw = "JR|src=5|dst=1|ttl=1|seq=7|hops=3|route_dm=100|route_ok=1|last_rssi=-60|{\"type\":\"VICTIM\"}";
    rescue_route_packet_t packet;
    char output[RESCUE_ROUTE_WIRE_CAP];
    assert(rescue_route_decode(raw, strlen(raw), &packet));
    assert(rescue_route_add_hop(&packet, -55, true));
    assert(packet.ttl == 0);
    assert(packet.hops == 4);
    assert(packet.route_dm == 150);
    assert(rescue_route_encode(&packet, output, sizeof(output)) > 0);
}
```

- [ ] **Step 2: Run the protocol test and verify RED**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -Werror `
  -I application/samples/products/sle_relay `
  tests/native/test_rescue_route.c `
  application/samples/products/sle_relay/rescue_route.c `
  -o tests/native/test_rescue_route.exe
```

Expected: compilation fails because `rescue_route.h/.c` do not exist yet.

- [ ] **Step 3: Add the protocol API and minimal implementation**

Use this public contract:

```c
#define RESCUE_ROUTE_WIRE_CAP 256

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t ttl;
    uint8_t hops;
    uint32_t seq;
    uint32_t route_dm;
    bool route_ok;
    int8_t last_rssi;
    const char *body;
    size_t body_len;
} rescue_route_packet_t;

bool rescue_route_decode(const char *wire, size_t wire_len, rescue_route_packet_t *packet);
int rescue_route_encode(const rescue_route_packet_t *packet, char *wire, size_t wire_cap);
uint16_t rescue_route_rssi_to_dm(int8_t rssi);
bool rescue_route_add_hop(rescue_route_packet_t *packet, int8_t filtered_rssi, bool rssi_valid);
```

Parsing must be length-bounded, accept the legacy `JR|src|dst|ttl|seq|JSON` form with `route_ok=false`, preserve the body byte-for-byte, check integer ranges, and never read past `wire_len`.

- [ ] **Step 4: Run protocol tests and verify GREEN**

Run the GCC command from Step 2, then:

```powershell
& tests/native/test_rescue_route.exe
```

Expected: `rescue_route tests: PASS` and exit code 0.

- [ ] **Step 5: Record the checkpoint**

Run:

```powershell
Get-FileHash application/samples/products/sle_relay/rescue_route.c,
             application/samples/products/sle_relay/rescue_route.h,
             tests/native/test_rescue_route.c -Algorithm SHA256
```

Expected: three hashes are printed; no Git commit is attempted because there is no repository.

### Task 2: Pure Ground Model and Button State Machine

**Files:**
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/ground_model.h`
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/ground_model.c`
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/tests/native/test_ground_model.c`

- [ ] **Step 1: Write failing model tests**

Cover fixed VD-001/002/003 slots, known JSON fields, old sequence rejection, legacy packets with unknown route, `MAX30102` versus `SIM`, 6-second stale state, K1 previous, K2 next, chord precedence, 30 ms debounce, and no hold repeat.

```c
static void test_chord_consumes_single_keys(void)
{
    ground_model_t model;
    ground_model_init(&model);
    ground_model_sample_buttons(&model, true, false, 0);
    ground_model_sample_buttons(&model, true, true, 40);
    ground_model_sample_buttons(&model, false, false, 80);
    assert(model.page == GROUND_PAGE_ENV);
    assert(model.selected_victim == 0);
    ground_model_sample_buttons(&model, false, false, 200);
    assert(model.page == GROUND_PAGE_ENV);
}
```

- [ ] **Step 2: Run the model test and verify RED**

Run:

```powershell
gcc -std=c11 -Wall -Wextra -Werror `
  -I application/samples/products/sle_relay `
  tests/native/test_ground_model.c `
  application/samples/products/sle_relay/ground_model.c `
  application/samples/products/sle_relay/rescue_route.c `
  -o tests/native/test_ground_model.exe
```

Expected: compilation fails because `ground_model.h/.c` do not exist yet.

- [ ] **Step 3: Add the model API and minimal implementation**

Use fixed-size state only:

```c
typedef enum { GROUND_PAGE_VICTIM = 0, GROUND_PAGE_ENV = 1 } ground_page_t;
typedef enum { GROUND_LINK_NONE = 0, GROUND_LINK_OK, GROUND_LINK_LOST } ground_link_t;

typedef struct {
    bool valid;
    uint32_t seq;
    int spo2;
    int heart_rate;
    int battery;
    int sos;
    uint32_t route_dm;
    uint8_t hops;
    bool route_ok;
    bool source_max30102;
    bool source_sim;
    uint64_t last_rx_ms;
} ground_victim_t;

typedef struct {
    ground_victim_t victims[3];
    uint8_t selected_victim;
    ground_page_t page;
    ground_button_state_t buttons;
} ground_model_t;

typedef struct {
    uint8_t victim_index;
    ground_victim_t victim;
} ground_report_t;

void ground_model_init(ground_model_t *model);
bool ground_model_parse_report(const uint8_t *data, size_t len, uint64_t now_ms, ground_report_t *report);
bool ground_model_apply_report(ground_model_t *model, const ground_report_t *report);
ground_link_t ground_model_link(const ground_victim_t *victim, uint64_t now_ms);
void ground_model_sample_buttons(ground_model_t *model, bool k1_pressed, bool k2_pressed, uint64_t now_ms);
```

Copy the callback payload into a 256-byte local NUL-terminated buffer before parsing. Accept only whitelisted IDs and bounded numeric fields. `ground_model_parse_report()` fills a temporary `ground_report_t` without touching shared state; the adapter locks only around `ground_model_apply_report()`.

- [ ] **Step 4: Run model tests and verify GREEN**

Run the GCC command from Step 2, then:

```powershell
& tests/native/test_ground_model.exe
```

Expected: `ground_model tests: PASS` and exit code 0.

- [ ] **Step 5: Record the checkpoint hashes**

Run `Get-FileHash` for `ground_model.c/.h` and `test_ground_model.c`; expect three SHA256 values.

### Task 3: Hardware and Build Contracts

**Files:**
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/tests/test_ground_oled_contract.py`

- [ ] **Step 1: Write the failing static contract test**

The Python test must assert:

```python
self.assertMacro(source, "GROUND_I2C_BUS", "1")
self.assertMacro(source, "GROUND_I2C_SDA_PIN", "15")
self.assertMacro(source, "GROUND_I2C_SCL_PIN", "16")
self.assertMacro(source, "GROUND_I2C_BAUDRATE", "400000")
self.assertMacro(source, "GROUND_GAS_ADC_CHANNEL", "5")
self.assertMacro(source, "GROUND_BUTTON_K1_PIN", "14")
self.assertMacro(source, "GROUND_BUTTON_K2_PIN", "13")
self.assertMacro(source, "GROUND_GAS_ALERT_MV", "1800")
self.assertMacro(source, "GROUND_GAS_CLEAR_MV", "1500")
```

Also assert ROOT-only CMake inclusion of `ground_ui.c`, `ground_model.c`, AHT20, SSD1306 and fonts; `ground_ui_ingest_report()` in the ROOT receive branch; `read_rssi_cb`; and `sle_read_remote_device_rssi()` in the shared relay code.

- [ ] **Step 2: Run contract tests and verify RED**

Run:

```powershell
python -B -m unittest tests.test_ground_oled_contract -v
```

Expected: failures report missing `ground_ui.c` and missing CMake entries.

### Task 4: ROOT-Only OLED, AHT20, Gas, and Buttons

**Files:**
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/ground_ui.h`
- Create: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/ground_ui.c`
- Modify: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/CMakeLists.txt`

- [ ] **Step 1: Add the ROOT adapter API**

```c
int ground_ui_init(void);
int ground_ui_start(void);
void ground_ui_ingest_report(const uint8_t *data, uint16_t len);
```

`ground_ui_init()` initializes the model mutex before SLE callbacks can run. `ground_ui_start()` creates separate UI and environment tasks and returns without blocking. `ground_ui_ingest_report()` calls `ground_model_parse_report()` outside the mutex, then locks only around `ground_model_apply_report()`.

- [ ] **Step 2: Implement hardware initialization with failure isolation**

Use I2C1/GPIO15/16 at 400 kHz, calling `uapi_i2c_set_baudrate()` after `ERRCODE_I2C_ALREADY_INIT`; probe 0x3C before declaring OLED ready; calibrate AHT20 up to five times; initialize ADC with `ADC_CLOCK_NONE`; initialize GPIO13/14 as pull-up inputs.

```c
#define GROUND_I2C_BUS 1
#define GROUND_I2C_SDA_PIN 15
#define GROUND_I2C_SCL_PIN 16
#define GROUND_I2C_BAUDRATE 400000
#define GROUND_GAS_ADC_CHANNEL 5
#define GROUND_BUTTON_K1_PIN 14
#define GROUND_BUTTON_K2_PIN 13
#define GROUND_GAS_ALERT_MV 1800
#define GROUND_GAS_CLEAR_MV 1500
#define GROUND_UI_TASK_STACK_SIZE 0x3000
```

I2C errors, OLED absence, AHT20 invalid range, and ADC failures update error flags and retry; none may stop SLE reception.

- [ ] **Step 3: Implement environment sampling and hysteresis**

Every 500 ms call `adc_port_read(5, &mv)`. Enter danger at 1800 mV and clear at 1500 mV. Every 2 s call `AHT20_StartMeasure()`, wait 80 ms in the environment task, then call `AHT20_GetMeasureResult()` and accept only -40..85 C and 0..100% RH.

- [ ] **Step 4: Implement the two OLED pages**

Use `Font_6x8` and these bounded ASCII formats:

```text
VD-001 1/3
PATH ~12.4m NEAR
SpO2 96 HR 78
BAT 82 SOS NO
LINK OK SRC MAX
```

```text
LOCAL ENV GROUND
TEMP 26.4C OK
HUM 58.0% OK
GAS 1240mV SAFE
SENSOR OK ENV
```

Unknown, stale, and failed values use `--`, `LOST`, or `ERR`. Refresh only on changed content or after 500 ms. Never draw from the SLE callback.

- [ ] **Step 5: Add ROOT-only CMake sources**

Keep only `rescue_route.c` in common sources. Add `ground_model.c`, `ground_ui.c`, the existing OLED driver/font sources, and the HiHope AHT20 source only when `CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT=y` appears in CMake definitions. Add their include directories once so `sle_uart` and `sle_relay` cannot both own duplicate SSD1306 symbols in this build.

- [ ] **Step 6: Run the contract test and verify remaining failures identify relay integration only**

Run `python -B -m unittest tests.test_ground_oled_contract -v`.

Expected: hardware/CMake assertions pass; relay callback/RSSI assertions remain red until Task 5.

### Task 5: SLE Integration and Per-Hop RSSI

**Files:**
- Modify: `xingshan/fbb_ws63-shouzaiduan/fbb_ws63-master/src/application/samples/products/sle_relay/sle_relay.c`

- [ ] **Step 1: Initialize and feed the ground UI**

Under `CONFIG_SAMPLE_SUPPORT_SLE_RELAY_ROOT`, call `ground_ui_init()` before `sle_uart_server_init()`, call `ground_ui_start()` after server initialization, and call `ground_ui_ingest_report(value, length)` in the ROOT `ssaps_write_cb()` before retaining the existing `[GROUND_RX]` log.

- [ ] **Step 2: Add asynchronous upstream RSSI state**

Add a one-second RSSI task and callback:

```c
typedef struct {
    int8_t filtered;
    uint8_t valid;
    uint64_t updated_ms;
} relay_rssi_state_t;

static void relay_read_rssi_cb(uint16_t conn_id, int8_t rssi, errcode_t status);
```

Register it through `g_conn_cbks.read_rssi_cb`. Accept only the current upstream connection, successful status, RSSI != 127, and -127..20. Apply `(3 * old + current) / 4`; expire after 5 s and clear on disconnect.

- [ ] **Step 3: Route every local and forwarded packet through one add-hop path**

Extend `relay_format_packet()` to initialize `hops=0|route_dm=0|route_ok=1|last_rssi=127`. In `relay_send_upstream()`, decode, add the current upstream hop exactly once, encode into a 256-byte local buffer, check the returned length, and pass the rewritten buffer to `ssapc_write_req()`.

Legacy packets remain forwardable but keep `route_ok=0`. A packet with input TTL 1 is sent as TTL 0; a packet received with TTL 0 is not forwarded.

- [ ] **Step 4: Run all host and contract tests**

Run both native executables and:

```powershell
python -B -m unittest discover -s tests -p "test_*.py" -v
```

Expected: all native tests and all Python tests pass, including the existing MAX30102 profile tests.

### Task 6: Build All Roles Without Overwriting Rollback Firmware

**Files:**
- Use: `delivery/build_rescue_network.ps1`
- Create output only under the PowerShell `$stage` path generated as `delivery/staging/ground-ui-distance-env-YYYYMMDD-HHmmss`

- [ ] **Step 1: Create a unique staging path and build**

Run:

```powershell
$stage = "D:\table\jiuyuan\delivery\staging\ground-ui-distance-env-$(Get-Date -Format yyyyMMdd-HHmmss)"
& D:\table\jiuyuan\delivery\build_rescue_network.ps1 -OutputDir $stage
```

Expected: `All five rescue firmware images were built successfully.`

- [ ] **Step 2: Verify all packages and manifest**

Run `Get-FileHash $stage\*_all.fwpkg -Algorithm SHA256` and compare every hash with `$stage\SHA256SUMS.txt`.

Expected: five matching firmware hashes; `delivery/firmware` timestamps and hashes remain unchanged.

### Task 7: Flash and Verify Only COM14 Ground

**Files:**
- Use: `delivery/burn_rescue_network.ps1`
- Firmware: `$stage\ground_all.fwpkg`

- [ ] **Step 1: Confirm COM14 exists and is not held by a monitor**

Open and close COM14 exclusively at 115200. Expected: open succeeds. Stop only known rescue monitor processes if they hold COM14; do not alter other ports.

- [ ] **Step 2: Flash only COM14**

Run:

```powershell
& D:\table\jiuyuan\delivery\burn_rescue_network.ps1 `
  -FirmwareDir $stage `
  -TargetPorts COM14 `
  -BaudRate 115200
```

When BurnTool waits, hold `RECOVRY`, tap `RST`, wait one second, then release `RECOVRY`. Expected: BurnTool exits 0 and reports only ground/COM14.

- [ ] **Step 3: Reset and verify ground runtime**

After one RESET, monitor COM14 for at least 30 seconds. Expected logs include:

```text
[JRTEST] boot role=ROOT id=1
[GROUND_UI] oled=READY
[GROUND_ENV] aht20=READY
[GROUND_RX]
```

Old upstream packets are accepted and personnel pages update. Until sky/victim firmware is upgraded, route fields are absent and the OLED correctly shows `PATH ~--.-m UNKNOWN`.

- [ ] **Step 4: Physical acceptance**

Verify K1 moves to the previous victim, K2 moves to the next victim, K1+K2 switches to the local environment page, long press does not repeat, and the environment page shows temperature, humidity, gas mV, and sensor status.
