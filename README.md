# Running a 28.9M parameter model on ESP32 and Raspberry Pi 2040

RP2040 and ESP32 AI is a two-board embedded AI demo built around an ESP32-S3
LLM engine and an RP2040 OLED display node. The ESP32-S3 runs the actual
28.9M-parameter PLE TinyLM model, while the RP2040 acts as a lightweight serial
display companion for the generated text.

This repository contains the ESP32-S3 inference firmware, model export and
verification tooling, an RP2040 serial OLED display-node sketch, a workflow
diagram, and demo output assets.

![ESP32 LLM Workflow](img/workflow-block-diagram.svg)

## Project Story

1. Tried to put the same million-parameter LLM idea directly on the RP2040.
Because the RP2040 only has 264 KB SRAM and no PSRAM, not practical there.
I used a smaller alternative for the RP2040 demo, but the output
quality was not enough for a real LLM project.

2. Final version uses the ESP32-S3 as the actual LLM engine, based on the
excellent `esp32-ai` work by Viacheslav Sierbov / slvDev:

https://github.com/slvDev/esp32-ai

The RP2040 is kept as a display node. It receives the generated LLM text over
serial and renders it on the OLED.

## Features

+- ESP32-S3 runs the actual 28.9M-parameter PLE TinyLM model
+- Model is stored in a custom flash partition and memory-mapped at runtime
+- Hot output head and scratch buffers are staged in PSRAM
+- Host-side export and verification tools are included
+- RP2040 display node receives generated text over UART
+- 128x64 I2C OLED output using U8g2 on the RP2040
+- Demo output GIF included in `img/`
+- No cloud API is required for the embedded inference demo

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


## Credit
This person has already ran the 28.9M parameter on the microcontroller - https://github.com/slvDev/esp32-ai
I attempted to take it further, but couldn't do it - Leaving this as experimental.
TinyStories is the dataset this trains on (Ronen Eldan and Yuanzhi Li, Microsoft Research, arXiv:2305.07759). 

The other half is Per-Layer Embeddings, Google's design from Gemma 3n, which is what lets a big model fit on a small chip.
Andrej Karpathy's llama2.c is the reference for training a small language model and running it in plain C.

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for the
full terms and [NOTICE.md](NOTICE.md) for upstream attribution.
