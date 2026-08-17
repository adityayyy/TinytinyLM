# RP2040 and ESP32 AI

RP2040 and ESP32 AI is a two-board embedded AI demo built around an ESP32-S3
LLM engine and an RP2040 OLED display node. The ESP32-S3 runs the actual
28.9M-parameter PLE TinyLM model, while the RP2040 acts as a lightweight serial
display companion for the generated text.

This repository contains the ESP32-S3 inference firmware, model export and
verification tooling, an RP2040 serial OLED display-node sketch, a workflow
diagram, and demo output assets.

![ESP32 LLM output demo](img/esp32-llm-output.gif)

## Project Story

I first tried to put the same million-parameter LLM idea directly on the RP2040.
Because the RP2040 only has 264 KB SRAM and no PSRAM, the real LLM path was not
practical there. I used a smaller alternative for the RP2040 demo, but the output
quality was not enough for a real LLM project.

The final version uses the ESP32-S3 as the actual LLM engine, based on the
excellent `esp32-ai` work by Viacheslav Sierbov / slvDev:

https://github.com/slvDev/esp32-ai

The RP2040 is kept as a display node. It receives the generated LLM text over
serial and renders it on the OLED.

## Features

- ESP32-S3 runs the actual 28.9M-parameter PLE TinyLM model
- Model is stored in a custom flash partition and memory-mapped at runtime
- Hot output head and scratch buffers are staged in PSRAM
- Host-side export and verification tools are included
- RP2040 display node receives generated text over UART
- 128x64 I2C OLED output using U8g2 on the RP2040
- Demo output GIF included in `img/`
- No cloud API is required for the embedded inference demo

## Repository Structure

```text
.
+-- .gitattributes
+-- .gitignore
+-- .github/
|   +-- ISSUE_TEMPLATE/
|   |   +-- bug_report.md
|   |   +-- feature_request.md
|   +-- pull_request_template.md
+-- data/
|   +-- prepare.py
+-- experiments/
|   +-- run_ablation.sh
|   +-- run_ple_dim_sweep.sh
|   +-- run_seed1.sh
+-- firmware/
|   +-- common/
|   |   +-- llm.h
|   +-- esp32_llm/
|   |   +-- esp32_llm.ino
|   |   +-- display.h
|   |   +-- partitions.csv
|   |   +-- README.md
|   +-- host_verify/
|   |   +-- verify.c
|   |   +-- ppl.c
|   +-- rp2040_display_node/
|   |   +-- rp2040_display_node.ino
|   +-- rp2040_tinylm/
|   |   +-- rp2040_tinylm.ino
|   |   +-- rp2040_model.h
+-- img/
|   +-- esp32-llm-output.gif
|   +-- workflow-block-diagram.svg
+-- media/
|   +-- esp32-ple-demo.gif
+-- src/
|   +-- analyze.py
|   +-- budget.py
|   +-- export.py
|   +-- gen_assets.py
|   +-- model.py
|   +-- quantize.py
|   +-- sample.py
|   +-- train.py
+-- BRANDING.md
+-- CHANGELOG.md
+-- CONTRIBUTING.md
+-- LICENSE
+-- NOTICE.md
+-- RESULTS.md
+-- README.md
```

## Firmware Files

- `firmware/esp32_llm/esp32_llm.ino`: ESP32-S3 LLM inference firmware.
- `firmware/esp32_llm/partitions.csv`: custom flash partition layout with the
  `model` partition.
- `firmware/common/llm.h`: portable C inference runtime shared by host and ESP32.
- `firmware/host_verify/verify.c`: host-side numerical correctness check.
- `firmware/host_verify/ppl.c`: host-side perplexity check.
- `firmware/rp2040_display_node/rp2040_display_node.ino`: RP2040 UART-to-OLED
  display node.
- `src/export.py`: exports the quantized model payload used by the ESP32.
- `img/workflow-block-diagram.svg`: block diagram for the final system.

## System Workflow

![RP2040 and ESP32 AI workflow](img/workflow-block-diagram.svg)

The system is organized around these functional blocks:

- Model training, export, and quantization on the development machine
- ESP32-S3 model partition in flash
- ESP32-S3 PSRAM staging and inference runtime
- UART serial stream carrying generated tokens/text
- RP2040 OLED display node
- 128x64 I2C OLED output

## Build and Flash Checklist

Before running the full demo, confirm:

- ESP32-S3 board has 16 MB flash and 8 MB PSRAM
- Arduino ESP32 core is installed and selected for ESP32-S3
- Custom partition scheme is enabled using `firmware/esp32_llm/partitions.csv`
- Model payload is exported with `src/export.py`
- Host verification passes before flashing the model
- ESP32 firmware is uploaded before writing the model partition
- Model binary is written to flash offset `0x110000`
- ESP32 UART TX is wired to RP2040 UART RX
- ESP32 and RP2040 share ground
- RP2040 OLED controller is SSD1306 or SH1106 as configured in the sketch
- Serial baud rate is `115200`

## Hardware Summary

The final demo uses:

- 1x ESP32-S3 N16R8 or equivalent board with PSRAM
- 1x Raspberry Pi Pico / RP2040 board
- 1x 128x64 I2C OLED display
- USB cable for flashing and monitoring
- UART connection from ESP32 TX to RP2040 RX
- Common ground between ESP32 and RP2040

## Revision Status

This is a revision 1 embedded AI demo package. Treat it as an experimental
firmware project that should be validated on the exact ESP32-S3 board, PSRAM
configuration, flash layout, RP2040 board, and OLED controller before public
demo use.

## Contributing

Issues, suggestions, and pull requests are welcome. For firmware changes,
include:

- A short description of the reason for the change
- The ESP32-S3 or RP2040 board used for testing
- Notes about model export, host verification, compile, upload, and runtime
  checks where applicable
- Serial output or demo media when behavior changes visibly

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for the
full terms and [NOTICE.md](NOTICE.md) for upstream attribution.

Project branding is not licensed as open source. See [BRANDING.md](BRANDING.md)
for the RP2040 and ESP32 AI branding notice.
