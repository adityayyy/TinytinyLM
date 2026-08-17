// RP2040 TinyStoryGen.
//
// Pico/RP2040 version of the ESP32-S3 on-device text demo. This does not run
// the 28.9M-param PLE model; the RP2040 has 264KB SRAM and no PSRAM. Instead it
// runs a compact byte-level backoff language model from flash.
//
// Display wiring requested:
//   OLED SDA -> GP28
//   OLED SCL -> GP29
//   OLED VCC -> 3V3
//   OLED GND -> GND
//
// GP28/GP29 are awkward for hardware I2C on many Pico setups, so this sketch
// uses a tiny bit-banged SSD1306-compatible I2C driver. Default address: 0x3C.
//
// Input:
//   Open Serial Monitor at 115200, set line ending to "Newline", type a prompt,
//   and press Enter. The model continues from your prompt.

#include "rp2040_model.h"

#define OLED_SDA 28
#define OLED_SCL 29
#define OLED_ADDR 0x3C
#define OLED_W 128
#define OLED_H 64
#define OLED_PAGES (OLED_H / 8)
#define OLED_TEXT_COLS 21
#define OLED_TEXT_ROWS 8

static uint32_t rng_state = 0xC0DEC0DEu;
static uint8_t prev2 = ' ';
static uint8_t prev1 = ' ';
static char prompt_buf[96];
static int prompt_len = 0;
static int text_col = 0;
static int text_row = 0;

// ---- bit-banged I2C + minimal OLED text ------------------------------------
static void i2c_delay() { delayMicroseconds(4); }

static void sda_high() { pinMode(OLED_SDA, INPUT_PULLUP); }
static void scl_high() { pinMode(OLED_SCL, INPUT_PULLUP); }
static void sda_low() {
  pinMode(OLED_SDA, OUTPUT);
  digitalWrite(OLED_SDA, LOW);
}
static void scl_low() {
  pinMode(OLED_SCL, OUTPUT);
  digitalWrite(OLED_SCL, LOW);
}

static void i2c_start() {
  sda_high(); scl_high(); i2c_delay();
  sda_low(); i2c_delay();
  scl_low(); i2c_delay();
}

static void i2c_stop() {
  sda_low(); i2c_delay();
  scl_high(); i2c_delay();
  sda_high(); i2c_delay();
}

static bool i2c_write(uint8_t b) {
  for (int i = 0; i < 8; i++) {
    if (b & 0x80) sda_high();
    else sda_low();
    i2c_delay();
    scl_high(); i2c_delay();
    scl_low(); i2c_delay();
    b <<= 1;
  }
  sda_high();
  i2c_delay();
  scl_high();
  bool ack = digitalRead(OLED_SDA) == LOW;
  i2c_delay();
  scl_low();
  return ack;
}

static void oled_cmd(uint8_t c) {
  i2c_start();
  i2c_write(OLED_ADDR << 1);
  i2c_write(0x00);
  i2c_write(c);
  i2c_stop();
}

static void oled_data(uint8_t d) {
  i2c_start();
  i2c_write(OLED_ADDR << 1);
  i2c_write(0x40);
  i2c_write(d);
  i2c_stop();
}

static void oled_pos(int col, int page) {
  oled_cmd(0xB0 + page);
  oled_cmd(0x00 + (col & 0x0F));
  oled_cmd(0x10 + (col >> 4));
}

static void oled_clear() {
  for (int p = 0; p < OLED_PAGES; p++) {
    oled_pos(0, p);
    for (int x = 0; x < OLED_W; x++) oled_data(0x00);
  }
  text_col = 0;
  text_row = 0;
  oled_pos(0, 0);
}

static void oled_begin() {
  sda_high();
  scl_high();
  delay(50);
  oled_cmd(0xAE);
  oled_cmd(0xD5); oled_cmd(0x80);
  oled_cmd(0xA8); oled_cmd(0x3F);
  oled_cmd(0xD3); oled_cmd(0x00);
  oled_cmd(0x40);
  oled_cmd(0x8D); oled_cmd(0x14);
  oled_cmd(0x20); oled_cmd(0x02);
  oled_cmd(0xA1);
  oled_cmd(0xC8);
  oled_cmd(0xDA); oled_cmd(0x12);
  oled_cmd(0x81); oled_cmd(0xCF);
  oled_cmd(0xD9); oled_cmd(0xF1);
  oled_cmd(0xDB); oled_cmd(0x40);
  oled_cmd(0xA4);
  oled_cmd(0xA6);
  oled_cmd(0xAF);
  oled_clear();
}

static const uint8_t FONT_3X5[][3] = {
  {0x00,0x00,0x00}, {0x00,0x17,0x00}, {0x03,0x00,0x03}, {0x1F,0x0A,0x1F},
  {0x12,0x1F,0x09}, {0x19,0x04,0x13}, {0x0A,0x15,0x1A}, {0x00,0x03,0x00},
  {0x00,0x0E,0x11}, {0x11,0x0E,0x00}, {0x0A,0x04,0x0A}, {0x04,0x0E,0x04},
  {0x10,0x08,0x00}, {0x04,0x04,0x04}, {0x00,0x10,0x00}, {0x18,0x04,0x03},
  {0x1F,0x11,0x1F}, {0x12,0x1F,0x10}, {0x1D,0x15,0x17}, {0x15,0x15,0x1F},
  {0x07,0x04,0x1F}, {0x17,0x15,0x1D}, {0x1F,0x15,0x1D}, {0x01,0x01,0x1F},
  {0x1F,0x15,0x1F}, {0x17,0x15,0x1F}, {0x00,0x0A,0x00}, {0x10,0x0A,0x00},
  {0x04,0x0A,0x11}, {0x0A,0x0A,0x0A}, {0x11,0x0A,0x04}, {0x01,0x15,0x03},
  {0x0E,0x15,0x16}, {0x1E,0x05,0x1E}, {0x1F,0x15,0x0A}, {0x0E,0x11,0x11},
  {0x1F,0x11,0x0E}, {0x1F,0x15,0x11}, {0x1F,0x05,0x01}, {0x0E,0x11,0x1D},
  {0x1F,0x04,0x1F}, {0x11,0x1F,0x11}, {0x08,0x10,0x0F}, {0x1F,0x04,0x1B},
  {0x1F,0x10,0x10}, {0x1F,0x02,0x1F}, {0x1F,0x01,0x1E}, {0x0E,0x11,0x0E},
  {0x1F,0x05,0x02}, {0x0E,0x19,0x1E}, {0x1F,0x05,0x1A}, {0x12,0x15,0x09},
  {0x01,0x1F,0x01}, {0x0F,0x10,0x0F}, {0x07,0x18,0x07}, {0x1F,0x08,0x1F},
  {0x1B,0x04,0x1B}, {0x03,0x1C,0x03}, {0x19,0x15,0x13},
};

static void oled_newline() {
  text_col = 0;
  text_row++;
  if (text_row >= OLED_TEXT_ROWS) oled_clear();
}

static void oled_char(char c) {
  if (c == '\r') return;
  if (c == '\n') {
    oled_newline();
    return;
  }
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c < 32 || c > 'Z') c = ' ';
  if (text_col >= OLED_TEXT_COLS) oled_newline();

  const uint8_t *glyph = FONT_3X5[(int)c - 32];
  oled_pos(text_col * 6, text_row);
  for (int i = 0; i < 3; i++) {
    uint8_t col = glyph[i];
    oled_data(col);
    oled_data(col);
  }
  text_col++;
}

static void oled_print(const char *s) {
  while (*s) oled_char(*s++);
}

static void oled_status() {
  oled_clear();
  oled_print("RP2040 TINYSTORY");
  oled_char('\n');
  oled_print("TYPE PROMPT IN");
  oled_char('\n');
  oled_print("SERIAL MONITOR");
  oled_char('\n');
  oled_print("115200 NEWLINE");
}

// ---- tiny byte-level language model runtime --------------------------------
static uint32_t rng32() {
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x ? x : 0xA341316Cu;
  return rng_state;
}

static int cmp_two_ctx(int idx, uint8_t a, uint8_t b) {
  uint8_t x = LM_TWO_CTX[idx * 2];
  uint8_t y = LM_TWO_CTX[idx * 2 + 1];
  if (x != a) return (int)x - (int)a;
  return (int)y - (int)b;
}

static int find_one(uint8_t a) {
  int lo = 0, hi = RP2040_MODEL_ONE_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    uint8_t x = LM_ONE_CTX[mid];
    if (x == a) return mid;
    if (x < a) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

static int find_two(uint8_t a, uint8_t b) {
  int lo = 0, hi = RP2040_MODEL_TWO_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    int c = cmp_two_ctx(mid, a, b);
    if (c == 0) return mid;
    if (c < 0) lo = mid + 1;
    else hi = mid - 1;
  }
  return -1;
}

static uint8_t weighted_pick(const uint8_t *bytes, const uint16_t *weights, int n) {
  uint32_t total = 0;
  for (int i = 0; i < n; i++) total += weights[i];
  uint32_t r = rng32() % total;
  for (int i = 0; i < n; i++) {
    if (r < weights[i]) return bytes[i];
    r -= weights[i];
  }
  return bytes[n - 1];
}

static uint8_t sample_next() {
  int idx = find_two(prev2, prev1);
  if (idx >= 0) {
    int a = LM_TWO_OFF[idx];
    int b = LM_TWO_OFF[idx + 1];
    if (b > a) return weighted_pick(LM_TWO_BYTES + a, LM_TWO_WEIGHTS + a, b - a);
  }

  idx = find_one(prev1);
  if (idx >= 0) {
    int a = LM_ONE_OFF[idx];
    int b = LM_ONE_OFF[idx + 1];
    if (b > a) return weighted_pick(LM_ONE_BYTES + a, LM_ONE_WEIGHTS + a, b - a);
  }

  return weighted_pick(LM_UNI_BYTES, LM_UNI_WEIGHTS, RP2040_MODEL_UNI_COUNT);
}

static void reset_context(const char *prompt) {
  prev2 = ' ';
  prev1 = ' ';
  while (*prompt) {
    prev2 = prev1;
    prev1 = (uint8_t)*prompt++;
  }
}

static void generate_story(const char *prompt, int n_bytes) {
  reset_context(prompt);
  Serial.print(prompt);
  oled_clear();
  oled_print(prompt);

  for (int i = 0; i < n_bytes; i++) {
    uint8_t b = sample_next();
    if (b < 32 && b != '\n') b = ' ';
    Serial.write(b);
    oled_char((char)b);
    prev2 = prev1;
    prev1 = b;
    delay(15);
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(2500);
  oled_begin();

  rng_state ^= micros();
  rng_state ^= (uint32_t)analogRead(A0) << 16;

  Serial.println();
  Serial.println("=== RP2040 TinyStoryGen ===");
  Serial.print("corpus bytes: ");
  Serial.println(RP2040_MODEL_CORPUS_BYTES);
  Serial.print("order-2 contexts: ");
  Serial.println(RP2040_MODEL_TWO_COUNT);
  Serial.print("order-1 contexts: ");
  Serial.println(RP2040_MODEL_ONE_COUNT);
  Serial.println();
  Serial.println("Type a prompt and press Enter. Example: Once upon a time");
  Serial.println("Send an empty line to use the default prompt.");
  Serial.println();

  oled_status();
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      prompt_buf[prompt_len] = 0;
      Serial.println();
      if (prompt_len == 0) {
        generate_story("Once upon a time ", 900);
      } else {
        if (prompt_buf[prompt_len - 1] != ' ' && prompt_len < (int)sizeof(prompt_buf) - 2) {
          prompt_buf[prompt_len++] = ' ';
          prompt_buf[prompt_len] = 0;
        }
        generate_story(prompt_buf, 900);
      }
      prompt_len = 0;
      Serial.println();
      Serial.println("--- type another prompt and press Enter ---");
      oled_char('\n');
      oled_print("READY");
    } else if (c == 8 || c == 127) {
      if (prompt_len > 0) prompt_len--;
    } else if (prompt_len < (int)sizeof(prompt_buf) - 1 && c >= 32 && c < 127) {
      prompt_buf[prompt_len++] = c;
      Serial.write(c);
    }
  }
  delay(10);
}
