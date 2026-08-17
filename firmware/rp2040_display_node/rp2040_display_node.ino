/*
  RP2040 OLED display node for the ESP32-S3 PLE TinyLM engine.

  Architecture:
    ESP32-S3 runs the actual 28.9M-parameter LLM and writes generated text.
    RP2040 receives that serial text and renders it on a 128x64 I2C OLED.

  Suggested UART wiring:
    ESP32 TX  -> RP2040 GP1 / UART0 RX
    ESP32 GND -> RP2040 GND

  OLED wiring:
    OLED SDA -> GP28
    OLED SCL -> GP29
    OLED VCC -> 3V3
    OLED GND -> GND

  Arduino libraries:
    - U8g2 by oliver (olikraus)
*/

#include <U8g2lib.h>
#include <Wire.h>

#define OLED_SDA 28
#define OLED_SCL 29
#define SERIAL_BAUD 115200
#define SCREEN_W 128
#define SCREEN_H 64
#define CHAR_W 6
#define LINE_H 10

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
// If your OLED is SH1106, use this instead:
// U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static int cursor_x = 0;
static int cursor_y = 10;
static char word_buf[32];
static int word_len = 0;

static void clear_screen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  cursor_x = 0;
  cursor_y = 10;
  u8g2.sendBuffer();
}

static void newline() {
  cursor_x = 0;
  cursor_y += LINE_H;
  if (cursor_y > SCREEN_H) clear_screen();
}

static void draw_raw_char(char c) {
  if (c == '\r') return;
  if (c == '\n') {
    newline();
    return;
  }
  if (c < 32 || c > 126) return;

  if (cursor_x + CHAR_W > SCREEN_W) newline();
  u8g2.drawGlyph(cursor_x, cursor_y, c);
  cursor_x += CHAR_W;
}

static void flush_word() {
  if (word_len == 0) return;
  word_buf[word_len] = 0;

  int word_px = u8g2.getStrWidth(word_buf);
  if (cursor_x > 0 && cursor_x + word_px > SCREEN_W) newline();

  for (int i = 0; i < word_len; i++) draw_raw_char(word_buf[i]);
  word_len = 0;
}

static void consume_char(char c) {
  if (c == ' ' || c == '\n' || c == '\t') {
    flush_word();
    if (c == '\n') newline();
    else draw_raw_char(' ');
    u8g2.sendBuffer();
    return;
  }

  if (word_len < (int)sizeof(word_buf) - 1) {
    word_buf[word_len++] = c;
  } else {
    flush_word();
    draw_raw_char(c);
    u8g2.sendBuffer();
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial1.begin(SERIAL_BAUD);

  Wire.setSDA(OLED_SDA);
  Wire.setSCL(OLED_SCL);
  Wire.begin();

  u8g2.begin();
  clear_screen();
  u8g2.drawStr(0, 10, "RP2040 DISPLAY NODE");
  u8g2.drawStr(0, 24, "Waiting for ESP32...");
  u8g2.sendBuffer();
}

void loop() {
  while (Serial1.available() > 0) {
    char c = (char)Serial1.read();
    Serial.write(c);
    consume_char(c);
  }

  while (Serial.available() > 0) {
    Serial1.write(Serial.read());
  }
}
