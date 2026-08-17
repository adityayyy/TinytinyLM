# RP2040 TinyStoryGen

This is a Raspberry Pi Pico / RP2040 version of the repository's on-device text
generation idea.

It is **not** a port of the ESP32-S3 28.9M-parameter model. The RP2040 has only
264KB SRAM, no PSRAM, and no ESP-IDF flash partition mmap API. This sketch uses a
smaller byte-level backoff language model that lives in flash as C arrays and
uses only a few bytes of RAM while generating.

## Hardware

- Raspberry Pi Pico, Pico W, or another RP2040 board
- 128x64 / 64x128 I2C OLED, usually SSD1306-compatible at address `0x3C`
- USB cable
- Arduino IDE

The sketch is configured for:

```text
OLED SDA -> GP28
OLED SCL -> GP29
OLED VCC -> 3V3
OLED GND -> GND
```

The code uses bit-banged I2C, so it can use these pins even if they are not a
normal hardware-I2C pair.

Important: the `GP28` / `GP29` names are **GPIO numbers**, not physical package
pin numbers. On a Raspberry Pi Pico header, physical pin 29 is GP22, and physical
pin 28 is GND. If you meant physical header pins, change these lines in
`rp2040_tinylm.ino`:

```cpp
#define OLED_SDA 28
#define OLED_SCL 29
```

to the actual GPIO numbers you used.

## Arduino IDE Setup

Install the RP2040 Arduino core:

1. Open Arduino IDE.
2. Go to **File > Preferences**.
3. Add this Boards Manager URL:

   ```text
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```

4. Go to **Tools > Board > Boards Manager**.
5. Install **Raspberry Pi Pico/RP2040** by Earle F. Philhower.
6. Select **Tools > Board > Raspberry Pi RP2040 Boards > Raspberry Pi Pico**.

## Generate The Model Header

From the repository root:

```powershell
python src\gen_rp2040_assets.py
```

That writes:

```text
firmware/rp2040_tinylm/rp2040_model.h
```

You can train the tiny byte model from your own text file:

```powershell
python src\gen_rp2040_assets.py path\to\story_corpus.txt
```

## Flash

Open this sketch in Arduino IDE:

```text
firmware/rp2040_tinylm/rp2040_tinylm.ino
```

Upload it to the Pico. Then open Serial Monitor at:

```text
115200 baud
```

You should see this on Serial Monitor and the OLED:

```text
RP2040 TinyStoryGen
```

## Giving Input

Use the Arduino IDE Serial Monitor:

```text
Baud: 115200
Line ending: Newline
```

Type a prompt and press Enter:

```text
Once upon a time
```

The device continues from that prompt. Send an empty line to use the default
prompt.

Serial output keeps normal lowercase text. The OLED uses a compact built-in
uppercase font so the code does not require display libraries.

## How It Works

The generator creates three flash-resident tables:

- order-2 byte contexts, like `"he" -> "r", " ", "n", ...`
- order-1 byte contexts, like `"e" -> " ", "r", "d", ...`
- unigram fallback, the most common bytes overall

At runtime, the RP2040 samples from the best available context and writes bytes
to Serial. It is a tiny language model, not an instruction-following chatbot.

## Why This Fits RP2040

The ESP32-S3 demo depends on:

- 16MB flash
- 8MB PSRAM
- ESP-IDF partition APIs
- ESP32-specific heap/timer APIs

This RP2040 version avoids all of that. The generated model is just C arrays in
program flash, and runtime RAM use stays tiny.
