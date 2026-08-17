# Notice

RP2040 and ESP32 AI
Copyright (c) 2026 Velan E

This project is based on and gives credit to:

- esp32-ai by Viacheslav Sierbov / slvDev
- Source location: https://github.com/slvDev/esp32-ai

The upstream project demonstrates a 28.9M-parameter PLE TinyLM running on an
ESP32-S3. This repository adapts that work into a two-board demo where the
ESP32-S3 acts as the LLM engine and the RP2040 acts as a serial OLED display
node.

The upstream project is MIT licensed:

Copyright (c) 2026 Viacheslav Sierbov

Local additions include the RP2040 display-node firmware, GitHub documentation,
workflow diagram, project notices, and repo packaging.

The software is provided without warranty. Validate model artifacts, flash
layout, PSRAM configuration, UART wiring, OLED compatibility, and runtime
behavior before using this project in demos or derivative work.

Project names, marks, logos, and official presentation are covered separately in
`BRANDING.md`.
