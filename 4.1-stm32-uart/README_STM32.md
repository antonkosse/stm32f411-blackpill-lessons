# Lesson 4.1 — UART link, STM32 side (slave)

STM32F411CEU6 "Black Pill" (WeAct) acting as the **slave** in a two-board UART link with an
ESP32-S3 DevKit (the master, separate repository).

- ESP32 → STM32: a button on the ESP32 sends `LED TOGGLE`, the STM32 switches its on-board LED.
- STM32 → ESP32: the on-board KEY button sends `INFO ESP_LED ON/OFF`, the ESP32 switches an external LED.

Status: built, flashed over USB (DFU) and verified end-to-end with the ESP32.

---

## 1. Hardware

| Item | Detail |
|---|---|
| Board | WeAct Black Pill, STM32F411CEU6 |
| Part number decoded | **F4** Cortex-M4 · **11** line · **C** 48 pins · **E** 512 KB flash · **U** UFQFPN48 · **6** −40…85 °C |
| On-board LED | **PC13**, **active low** (lights when the pin is LOW). Weak pin: max 3 mA, 2 MHz |
| On-board button KEY | **PA0** to GND, **active low**, needs the internal pull-up |
| Crystals | 25 MHz HSE, 32.768 kHz LSE (PC14/PC15 — leave free) |
| USB-C | wired to PA11/PA12 (USB OTG FS) — leave those pins free |
| SWD header | 4 pads: **3V3 · DIO (PA13) · SCK (PA14) · GND** — **pins are NOT soldered yet** |
| Debug probe | WeAct Mini Debugger V1.0 (ST-LINK V2-1 or DAPLink firmware, has SWD + SWO + NRST + USB-UART) |
| USB-UART adapter | CH340 stick — keep the yellow jumper on **VCC–3V3** (3.3 V logic) |

### Wiring to the ESP32-S3

| STM32 (Black Pill label) | | ESP32-S3 |
|---|---|---|
| **A9** (PA9, USART1_TX, AF7) | → | GPIO16 (UART2 RX) |
| **A10** (PA10, USART1_RX, AF7) | ← | GPIO17 (UART2 TX) |
| **G** | — | GND |

- Both chips are 3.3 V logic → no level shifter.
- Connect **only GND** between the boards. Never link 5V/3V3 rails while both are on USB
  (the Black Pill 5V pin is tied straight to USB VBUS, unprotected).
- Link settings: **115200 baud, 8N1, no flow control** on both sides.

---

## 2. How to read STM32 pin documentation

Every pin has three names. Only two matter for code.

| Layer | Example (board label "A9") | Where | Who uses it |
|---|---|---|---|
| Package pin number | 30 | Datasheet Fig. 10 (p. 35) | PCB designers only |
| GPIO name = Port + bit | **PA9** → `GPIOA`, `GPIO_PIN_9` | Datasheet Table 8 (p. 39) | You, in code. The board drops the "P": **A9** |
| Alternate function (AF) | **USART1_TX via AF7** | Datasheet Table 9 (p. 48–53) | You, through CubeMX |

- **Table 9** is a grid: rows = pins, columns = AF0…AF15. Find the signal (e.g. `USART1_TX`), read
  its column header for the AF number and its row for the pin.
- The AF number is mostly per peripheral (USART1/2 = AF7, USART6 = AF8) **but not always**
  (I2C2_SDA is AF9 on PB3/PB9, I2C2_SCL is AF4 on PB10) — always read it from the cell.
- **Key difference from ESP32:** the ESP32 has a GPIO matrix (`uart_set_pin()` → almost any pin).
  STM32 pins have a **fixed menu** of functions; if a signal is not in the pin's row, that pin
  cannot do it.
- In Table 8 use only the **UFQFPN48** column; `-` = the pin does not exist on this chip.
  `FT` = 5 V tolerant, `TC` = 3.3 V only (PA0, PB5). Table 7 explains all abbreviations.
- CubeMX contains the same tables: click a pin → the dropdown is that pin's row of Table 9.
  Ctrl+click a pin → alternative pins for that signal are highlighted.
- Generated code shows the result:
  `.Pin = GPIO_PIN_9 | GPIO_PIN_10; .Mode = GPIO_MODE_AF_PP; .Alternate = GPIO_AF7_USART1;`
  All valid pairings are macros in `stm32f4xx_hal_gpio_ex.h`.

### Documentation set

| Document | ID | Answers |
|---|---|---|
| Datasheet | DS10314 (Rev 8) | What the chip has; pinout; AF table; electrical limits |
| Reference manual | RM0383 | How each peripheral works, every register |
| Programming manual | PM0214 | Cortex-M4 core: NVIC, SysTick, faults |
| Errata | ES0287 (verify on st.com product page) | Silicon bugs |
| HAL/LL manual | UM1725 | What HAL functions do (or Ctrl+click in VS Code) |
| Board schematic | github.com/WeActStudio/WeActStudio.MiniSTM32F4x1 | What the board already uses |
| CubeMX manual | UM1718 | CubeMX UI |
| CubeProgrammer manual | UM2237 | Flashing over SWD / USB / UART |

Pins already used by the board: PA11/PA12 (USB), PC13 (LED), PA0 (KEY), PC14/PC15 (LSE),
PA13/PA14 (SWD), PB2 (BOOT1, sampled at reset).

---

## 3. Toolchain setup (VS Code on Windows)

1. **VS Code profile "STM32"** (separate from the ESP-IDF profile). Created with *Extensions = None*,
   then install only **"STM32CubeIDE for Visual Studio Code"** (publisher STMicroelectronics). It is
   an extension pack: clangd, CMake Tools, peripheral viewer, bundles manager, etc.
   - Do **not** install Microsoft C/C++ in this profile — it conflicts with clangd.
2. **STM32CubeMX** (classic, not CubeMX2) — separate install from st.com (myST login).
3. **STM32CubeProgrammer** — separate install from st.com. Also installs the USB DFU driver.
4. **ST-Link USB drivers** — STM32Cube panel → *Install ST-Link USB drivers*.

The extension does **not** contain a serial monitor. Use the Microsoft "Serial Monitor" extension or any terminal.

---

## 4. Creating / changing the project with CubeMX

### New project

1. STM32Cube panel → **Launch STM32CubeMX** → *File → New Project* → **MCU/MPU Selector**
   (Black Pill is not in the Board Selector) → `STM32F411CEU6` → *Start Project*.
2. **Pinout & Configuration** (current project):

   | Where | Setting |
   |---|---|
   | System Core → SYS | Debug = **Serial Wire** (keeps PA13/PA14 for the debugger — never skip) |
   | Connectivity → USART1 | Mode = **Asynchronous**, 115200 8N1; NVIC Settings → ✓ **USART1 global interrupt** |
   | PA0 | GPIO_Input, **Pull-up**, User Label `KEY` |
   | PC13 | GPIO_Output, initial level **High** (LED off), User Label `LED` |
   | RCC / clock | default **HSI 16 MHz**, no PLL (accurate enough for 115200 baud) |

3. **Project Manager:**
   - Project Name: letters, digits, underscores (avoid spaces/dots). CubeMX **creates a folder** with this name.
   - Project Location: the **parent** folder (e.g. `EMB\stm32`), not the project folder itself.
   - Toolchain / IDE: **CMake**.
   - Code Generator: *Copy only the necessary library files*; ✓ *Generate peripheral initialization as a
     pair of .c/.h files per peripheral*; ✓ *Keep User Code when re-generating*;
     ✓ *Delete previously generated files when not re-generated*.
4. **GENERATE CODE**. Keep the `.ioc` file — it *is* the hardware configuration.

### Changing the configuration later

Open the `.ioc` (double-click or *Launch STM32CubeMX → File → Load Project*) → change →
**Ctrl+S** → **GENERATE CODE** → in VS Code rebuild.

Rules:
- Only code inside `/* USER CODE BEGIN x */ … /* USER CODE END x */` survives regeneration.
- CubeMX only rewrites `cmake/stm32cubemx/`. Own source files (e.g. `Core/Src/app.c`) must be
  added to `target_sources(...)` in the **top-level `CMakeLists.txt`**.
- New lesson from an old one: open the old `.ioc` → *File → Save Project As* (do not copy the
  folder in Explorer — names inside `.ioc`/CMake would stay old).
- One lesson = one CubeMX project folder, because the `.ioc` defines the hardware init code.

### Importing into VS Code (first time per project)

1. *File → Open Folder* → the generated folder.
2. Accept the popup "configure as STM32Cube project", or STM32Cube panel → **Set Up STM32Cube project(s)**.
   Board/Device = `STM32F411CEU6`, **Toolchain = GCC** → *Configure*.
3. **Bundles Manager** → select the project → install the listed bundles (cmake, ninja, GNU toolchain).
   If empty: *Lock to latest* → *Install locked*.
4. A successful setup creates a `.settings` folder in the project root.

---

## 5. Build

- CMake preset **Debug**, *Build* button in the status bar (or F7).
- Output: `build\Debug\4.1-stm32-uart.elf`. The end of the log shows flash/RAM usage.

---

## 6. Flashing WITHOUT the debugger (current method: USB DFU)

### How BOOT0 works

BOOT0 is read **once, at reset**. It selects *what runs*, it does **not** erase anything.

| BOOT0 at reset | Runs |
|---|---|
| 0 (normal) | your program from flash (0x08000000) |
| 1 (button held) | ST's built-in bootloader (ROM), waits for USB DFU or UART |

Same idea as the ESP32's BOOT button (GPIO0) — `idf.py flash` toggles it automatically, here you
press it by hand. Writing a new program erases the needed flash sectors automatically; a full chip
erase (eraser icon in CubeProgrammer) is rarely needed.

### Steps

1. Disconnect the debugger's wires; plug in the Black Pill **USB-C**.
2. **Unplug the wire on PA10.** The ROM bootloader also listens on USART1 (PA9/PA10); activity from
   the ESP32 on PA10 can make it pick UART instead of USB.
3. Enter the bootloader: **hold BOOT0 → tap NRST → release BOOT0**
   (alternative: hold BOOT0 while plugging in the USB-C cable). The LED stops.
4. Device Manager should show **"STM32 BOOTLOADER"**.
5. Flash:
   - **GUI:** STM32CubeProgrammer → **USB** → ↻ (Port shows `USB1`) → *Connect* →
     *Erasing & Programming* → select the `.elf` → ✓ Verify → *Start Programming* → *Disconnect*.
   - **CLI:**
     ```
     STM32_Programmer_CLI.exe -c port=USB1 -w build\Debug\4.1-stm32-uart.elf -v
     ```
     (full path usually `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\`)
6. Press **NRST** (BOOT0 released) → your program runs. Reconnect the PA10 wire.

### Troubleshooting DFU

| Symptom | Fix |
|---|---|
| Windows: "USB device not recognized" | Known F411 Black Pill quirk. Retry 3–5×; use the "hold BOOT0 while plugging in" method; warm the chip with a finger; direct laptop port, other cable |
| "STM32 BOOTLOADER" with yellow ⚠ | Driver missing: run `Drivers\DFU_Driver\STM32Bootloader.bat` from the CubeProgrammer folder as Administrator |
| CubeProgrammer Port = "No DFU…" | Chip not in bootloader → repeat step 3, then ↻ |
| Never works | Fallback: UART bootloader with the CH340 — CH340 TXD → A10, RXD → A9, GND → G; CubeProgrammer → **UART** → COM port → Connect |

---

## 7. Moving forward: using the debugger (recommended)

The debugger gives one-click flash, breakpoints, variable/register view and no BOOT0 dance.

### 7.1 Solder the SWD header (required)

The 4 SWD pads have no pins — a connector pressed on bare holes gives "Target no device found".
Short ends of a 4-pin strip into the holes from the top, solder 4 joints from below (2–3 s each,
shiny cone). Pins only reach PA13/PA14 through these pads.

### 7.2 Wiring (match by printed labels, not by position)

Mini Debugger cable order: `3V3 · SWDIO · SWCLK · GND · SWO · NRST · TXD · RXD · GND · 5V`

| Mini Debugger | Black Pill | Needed |
|---|---|---|
| SWDIO | DIO (SWD header) | yes |
| SWCLK | SCK (SWD header) | yes |
| GND | GND (SWD header) | yes |
| 3V3 | 3V3 (SWD header) | only if the Black Pill USB-C is **unplugged** (one power source only) |
| NRST | **R** (main header) | recommended — enables "connect under reset" recovery |
| 5V, SWO | — | leave off; tape loose ends (a loose 5V lead is dangerous) |

Check in Device Manager: "STLink dongle" + "STLink Virtual COM Port" = ST-LINK firmware (works with
the VS Code extension). "CMSIS-DAP / DAPLink" = needs OpenOCD/pyOCD instead. Decline firmware
update prompts unless it refuses to connect.

### 7.3 Flash and debug from VS Code

- **Run and Debug** (Ctrl+Shift+D) → **STM32Cube: STLink GDB Server** → ▶. Builds, flashes,
  stops at `main()`. F5 continue, F10/F11 step, click the margin for breakpoints, Shift+F5 stop.
  The *Peripherals* view shows live registers (e.g. `USART1 → SR`).
- **Flash only** (like `idf.py flash`), `.vscode/tasks.json`:
  ```json
  {
    "version": "2.0.0",
    "tasks": [{
      "label": "Flash (ST-LINK)",
      "type": "shell",
      "command": "STM32_Programmer_CLI",
      "args": ["-c", "port=SWD", "mode=UR", "-w", "${command:cmake.launchTargetPath}", "-v", "-rst"],
      "dependsOn": "CMake: build",
      "problemMatcher": []
    }]
  }
  ```
  `mode=UR` = connect under reset (uses the NRST wire), `-rst` = run after flashing.

### 7.4 Troubleshooting the debugger

| Message | Meaning / fix |
|---|---|
| "No ST-LINK detected" | probe/driver: install ST-Link drivers, check Device Manager |
| "Target no device found" / "No device found on target" | probe OK, chip unreachable: unsoldered/loose SWD pins, SWDIO/SWCLK swapped, missing GND, no target power |
| Connected once, never again | firmware disabled PA13/PA14 (SYS Debug not Serial Wire) or deep sleep → BOOT0 trick (hold BOOT0, tap NRST, release, connect) or CubeProgrammer *Mode = Under reset* |
| Check target voltage | CubeProgrammer → ST-LINK → Mode Under reset, 1000 kHz → Connect: ~3.3 V = power OK |

### 7.5 printf console through the debugger (future)

USART1 is now the ESP32 link, so the console must use **USART2**:
- CubeMX: USART2 Asynchronous 115200 → **PA2** (TX) / **PA3** (RX).
- Wiring: PA2 (A2) → debugger **RXD**, PA3 (A3) ← debugger **TXD** (swap if silent).
- Code (USER CODE block): `int __io_putchar(int ch)` → `HAL_UART_Transmit(&huart2, …)`.
  CubeMX's `syscalls.c` already routes `printf` there.
- Monitor: "STMicroelectronics STLink Virtual COM Port (COMx)" at 115200.
- Gotchas: output is buffered until `\n` (use `"\r\n"`); `%f` is disabled by default.

---

## 8. Application (Core/Src/app.c)

Structure: `main.c` (generated) only calls `App_Start()` once (USER CODE 2) and `App_Poll()` in the
loop (USER CODE 3). All logic lives in `app.c` / `app.h`, listed in the top-level `CMakeLists.txt`.

### Protocol (from the lecture)

- Text lines ending in `\n` (`\r` ignored). ESP32 = master.
- Every command gets exactly **one** reply line starting with `OK` or `ERR`.
- Lines the STM32 sends on its own start with `INFO` (so the master never takes them for a reply).
- After reset: several `INFO` lines, then `OK READY`.
- Commands are case-insensitive.

| Command | Reply |
|---|---|
| `LED ON` / `LED OFF` / `LED TOGGLE` | `OK LED ON` / `OK LED OFF` |
| `PING` | `OK PONG` |
| `HELP` | `OK LED ON\|OFF\|TOGGLE \| TIME \| STATUS \| PING \| HELP` |
| `TIME` | `OK TIME LOCAL_MS=… UPTIME=hh:mm:ss` (used by the master's SYNC) |
| `STATUS` | `OK STATUS LED=… ESP_LED=… LOCAL_MS=…` |
| anything else | `ERR unknown` (the lecture's `PWM …` commands are **not** implemented) |

KEY press → alternately `INFO ESP_LED ON` / `INFO ESP_LED OFF`.

### How it works

- **RX:** `HAL_UART_Receive_IT` for 1 byte. `HAL_UART_RxCpltCallback` appends to a line buffer, on
  `\n` sets `volatile g_line_ready`, and **re-arms** reception (without re-arming only the first byte
  ever arrives). `HAL_UART_ErrorCallback` re-arms after framing/noise errors.
- **Main loop:** copies the line with interrupts disabled (so the ISR cannot tear it), clears the
  flag, parses with `sscanf`, replies with blocking `HAL_UART_Transmit` (100 ms timeout).
- **KEY:** sampled every 20 ms with `HAL_GetTick()` (no blocking delay) → this sampling period is the
  debounce; acts on the press edge (HIGH → LOW).

### Known limitations

- Single line buffer: relies on the half-duplex protocol (master waits for the reply). A second
  line arriving before the main loop copied the first would overwrite it.
- Clock is HSI without PLL (lecture used HSE 25 MHz → 96 MHz + TIM2 1 ms tick + TIM3 PWM on
  PA6/PA7/PB0/PB1 — not used here).

---

## 9. Git

Commit everything **except** `build/` and `mx.scratch`. Suggested `.gitignore`:
```
build/
mx.scratch
```
Keep the `.ioc`, `CMakeLists.txt`, `CMakePresets.json`, `cmake/`, `Core/`, `Drivers/`, `.settings/`, `.vscode/`.

---

## 10. Next steps

- [ ] Solder the 4-pin SWD header, switch flashing to the Mini Debugger (section 7).
- [ ] Optional: printf console on USART2 through the debugger (7.5).
- [ ] Optional: implement the lecture's `PWM MAX|OFF|HALF|FADE` (TIM3 CH1–4 on PA6, PA7, PB0, PB1)
      so the master's `DEMO` passes completely.
- [ ] Optional: HSE 25 MHz crystal + PLL; then recheck UART baud and timer prescalers
      (`f = timer_clk / ((PSC+1)(ARR+1))`).
- [ ] Move the debounce into a portable module shared with the ESP32 (inputs: pin level + ms tick).

---

## Self-check

Answers are Base64-encoded (decode with any Base64 tool, e.g. `certutil -decode` or an online decoder) so they can't be read at a glance.

**Q1.** The board label is "A9". What is its GPIO name, what do you write in HAL code, and which AF number selects USART1_TX on it?

`UEE5ID0gcG9ydCBBLCBiaXQgOS4gSEFMOiBHUElPQSwgR1BJT19QSU5fOS4gVVNBUlQxX1RYIGlzIGluIHRoZSBBRjcgY29sdW1uIG9mIFRhYmxlIDkgKEdQSU9fQUY3X1VTQVJUMSku`

**Q2.** Why can't you move USART1_TX to PB10, the way uart_set_pin() moves a UART on the ESP32?

`UEIxMCdzIHJvdyBpbiBUYWJsZSA5IGhhcyBubyBVU0FSVDEgZnVuY3Rpb24uIFNUTTMyIHBpbnMgaGF2ZSBhIGZpeGVkIG1lbnUgb2YgYWx0ZXJuYXRlIGZ1bmN0aW9ucyAoQUYwLUFGMTUpOyB0aGVyZSBpcyBubyBHUElPIG1hdHJpeCBsaWtlIG9uIHRoZSBFU1AzMi4=`

**Q3.** What does holding BOOT0 during reset do? Does it erase the flash?

`Qk9PVDAgaXMgcmVhZCBvbmNlIGF0IHJlc2V0IGFuZCBzZWxlY3RzIHdoYXQgcnVuczogQk9PVDA9MSBzdGFydHMgU1QncyBidWlsdC1pbiBST00gYm9vdGxvYWRlciAoVVNCIERGVSAvIFVBUlQpIGluc3RlYWQgb2YgeW91ciBwcm9ncmFtLiBOb3RoaW5nIGlzIGVyYXNlZDsgdGhlIGZsYXNoIGtlZXBzIGl0cyBjb250ZW50LiBQcm9ncmFtbWluZyBlcmFzZXMgdGhlIG5lZWRlZCBzZWN0b3JzIGF1dG9tYXRpY2FsbHku`

**Q4.** Why unplug the PA10 wire before entering the USB bootloader?

`VGhlIFJPTSBib290bG9hZGVyIGFsc28gbGlzdGVucyBvbiBVU0FSVDEgKFBBOS9QQTEwKS4gQWN0aXZpdHkgZnJvbSB0aGUgRVNQMzIgb24gUEExMCBjYW4gbWFrZSBpdCBzZWxlY3QgdGhlIFVBUlQgaW50ZXJmYWNlIGluc3RlYWQgb2YgVVNCIERGVS4=`

**Q5.** You edit main.c outside any USER CODE block, then regenerate in CubeMX. What happens? And how do you add your own app.c to the build?

`VGhlIGVkaXQgaXMgb3ZlcndyaXR0ZW4gYW5kIGxvc3Q7IG9ubHkgY29kZSBiZXR3ZWVuIFVTRVIgQ09ERSBCRUdJTi9FTkQgc3Vydml2ZXMuIEN1YmVNWCBkb2VzIG5vdCBrbm93IGFib3V0IGFwcC5jOiBhZGQgaXQgdG8gdGFyZ2V0X3NvdXJjZXMoLi4uKSBpbiB0aGUgdG9wLWxldmVsIENNYWtlTGlzdHMudHh0IChDdWJlTVggb25seSByZXdyaXRlcyBjbWFrZS9zdG0zMmN1YmVteC8pLg==`

**Q6.** The STM32 receives the first byte from the ESP32 and then nothing. Most likely cause?

`SEFMX1VBUlRfUmVjZWl2ZV9JVCBhcm1zIHJlY2VwdGlvbiBvZiBvbmx5IE4gYnl0ZXMuIElmIEhBTF9VQVJUX1J4Q3BsdENhbGxiYWNrIChhbmQgRXJyb3JDYWxsYmFjaykgZG9lcyBub3QgY2FsbCBpdCBhZ2FpbiwgcmVjZXB0aW9uIHN0b3BzIGFmdGVyIHRoZSBmaXJzdCBieXRlLg==`

**Q7.** Why is g_line_ready declared volatile, and why does App_Poll copy the line with interrupts disabled?

`SXQgaXMgY2hhbmdlZCBpbnNpZGUgYW4gaW50ZXJydXB0OyB3aXRob3V0IHZvbGF0aWxlIHRoZSBjb21waWxlciBtYXkga2VlcCBpdCBpbiBhIHJlZ2lzdGVyIGFuZCB0aGUgbWFpbiBsb29wIG5ldmVyIHNlZXMgdGhlIGNoYW5nZS4gSW50ZXJydXB0cyBhcmUgZGlzYWJsZWQgZHVyaW5nIHRoZSBjb3B5IHNvIHRoZSBVQVJUIGNhbGxiYWNrIGNhbm5vdCBtb2RpZnkgdGhlIGJ1ZmZlciBoYWxmd2F5IHRocm91Z2ggKGEgdG9ybiBsaW5lKS4=`

**Q8.** HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET) - does the Black Pill LED turn on?

`Tm8sIGl0IHR1cm5zIG9mZi4gUEMxMyBMRUQgaXMgYWN0aXZlIGxvdzogaXQgbGlnaHRzIHdoZW4gdGhlIHBpbiBpcyBMT1cgKEdQSU9fUElOX1JFU0VUKS4=`

**Q9.** Why must SYS -> Debug be set to Serial Wire in CubeMX?

`T3RoZXJ3aXNlIHRoZSBnZW5lcmF0ZWQgY29kZSB0dXJucyBQQTEzL1BBMTQgaW50byBwbGFpbiBHUElPLCBTV0Qgc3RvcHMgd29ya2luZyBhbmQgdGhlIGRlYnVnZ2VyIGNhbm5vdCBjb25uZWN0IChyZWNvdmVyeSB0aGVuIG5lZWRzIGNvbm5lY3QtdW5kZXItcmVzZXQgdmlhIE5SU1Qgb3IgdGhlIEJPT1QwIHRyaWNrKS4=`

**Q10.** The debugger reports 'Target no device found'. Which part works and which doesn't?

`VGhlIFNULUxJTksgcHJvYmUgaXRzZWxmIHdhcyBmb3VuZCBhbmQgaW5pdGlhbGlzZWQgKFVTQiBhbmQgZHJpdmVyIE9LKS4gVGhlIFNUTTMyIGNoaXAgZG9lcyBub3QgYW5zd2VyIG92ZXIgU1dEOiB1bnNvbGRlcmVkL2xvb3NlIFNXRCBwaW5zLCBTV0RJTy9TV0NMSyBzd2FwcGVkLCBtaXNzaW5nIEdORCwgbm8gdGFyZ2V0IHBvd2VyLCBvciBTV0QgZGlzYWJsZWQgYnkgdGhlIHJ1bm5pbmcgZmlybXdhcmUu`

**Q11.** Timer clock 96 MHz, PSC = 95, ARR = 999. How often does the update interrupt fire?

`ZiA9IDk2IDAwMCAwMDAgLyAoKDk1KzEpICogKDk5OSsxKSkgPSA5NiAwMDAgMDAwIC8gOTYgMDAwID0gMTAwMCBIeiwgaS5lLiBldmVyeSAxIG1zLiBUaGUgKzEgaXMgYmVjYXVzZSBib3RoIHJlZ2lzdGVycyBjb3VudCBmcm9tIDAu`

**Q12.** Why are printf and the ESP32 link on different USARTs in the future setup?

`VVNBUlQxIChQQTkvUEExMCkgaXMgdGhlIGxpbmsgdG8gdGhlIEVTUDMyOyBwcmludGluZyB0aGVyZSB3b3VsZCBzZW5kIGdhcmJhZ2UgY29tbWFuZHMgdG8gdGhlIG1hc3Rlci4gVGhlIGNvbnNvbGUgZ29lcyB0byBVU0FSVDIgKFBBMi9QQTMpIHRocm91Z2ggdGhlIE1pbmkgRGVidWdnZXIncyB2aXJ0dWFsIENPTSBwb3J0Lg==`

