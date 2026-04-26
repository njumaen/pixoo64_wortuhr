/*
 * English Wordclock for Pixoo64 – ESP32
 * =======================================
 * The ESP32 fetches time via NTP, builds the letter grid
 * and sends each frame via HTTP to the Pixoo64.
 *
 * Required libraries (Arduino Library Manager):
 *   - ArduinoJson  (Benoit Blanchon)
 *   - NTPClient    (Fabrice Weinberg)
 *
 * Board: "ESP32 Dev Module" (or your specific board)
 *
 * Configuration: only adjust the "SETTINGS" section below.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <base64.h>   // included in ESP32 core

// ═══════════════════════════════════════════════════════════════
// SETTINGS
// ═══════════════════════════════════════════════════════════════
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* PIXOO_IP      = "192.168.1.50";   // IP of your Pixoo64
const int   PIXOO_PORT    = 80;
const int   BRIGHTNESS    = 80;               // 0–100
const long  UTC_OFFSET_S  = 0;               // UTC=0, CET=3600, EST=-18000

// Colors  (R, G, B)
const uint8_t COLOR_FG[3]  = {255, 255, 255}; // active letters
const uint8_t COLOR_DIM[3] = { 40,  40,  60}; // inactive letters
const uint8_t COLOR_BG[3]  = {  0,   0,   0}; // background
const uint8_t COLOR_DOT[3] = {255, 200,   0}; // minute dots

// ═══════════════════════════════════════════════════════════════
// Constants
// ═══════════════════════════════════════════════════════════════
#define DISPLAY_SIZE 64
#define ROWS         10
#define COLS         11
#define CELL_W        5
#define CELL_H        6
// off_x = (64 - (11*5-2)) / 2 = 5
// off_y = (64 - (10*6-1)) / 2 = 2
#define OFF_X         5
#define OFF_Y         2

// ═══════════════════════════════════════════════════════════════
// Letter grid  10 × 11  (all words verified, exactly 11 chars/row)
// ═══════════════════════════════════════════════════════════════
const uint8_t GRID[ROWS][COLS] = {
  {'I','T','K','I','S','H','A','L','F','A','S'}, // 0  IT IS HALF
  {'T','E','N','Q','U','A','R','T','E','R','X'}, // 1  TEN(m) QUARTER
  {'T','W','E','N','T','Y','F','I','V','E','W'}, // 2  TWENTY FIVE(m)
  {'M','I','N','U','T','E','S','T','O','A','Z'}, // 3  MINUTES TO
  {'P','A','S','T','A','O','N','E','T','W','O'}, // 4  PAST ONE TWO
  {'T','H','R','E','E','X','F','O','U','R','X'}, // 5  THREE FOUR
  {'F','I','V','E','X','S','I','X','Z','A','B'}, // 6  FIVE(h) SIX
  {'S','E','V','E','N','X','E','I','G','H','T'}, // 7  SEVEN EIGHT
  {'N','I','N','E','X','E','L','E','V','E','N'}, // 8  NINE ELEVEN
  {'T','E','N','X','T','W','E','L','V','E','X'}, // 9  TEN(h) TWELVE
};

// ═══════════════════════════════════════════════════════════════
// Word definitions  {row, start_col, length}
// ═══════════════════════════════════════════════════════════════
struct WordDef { uint8_t row, col, len; };

const WordDef W_IT       = {0, 0, 2};
const WordDef W_IS       = {0, 3, 2};
// Minutes
const WordDef W_HALF     = {0, 5, 4};
const WordDef W_TEN_M    = {1, 0, 3};
const WordDef W_QUARTER  = {1, 3, 7};
const WordDef W_TWENTY   = {2, 0, 6};
const WordDef W_FIVE_M   = {2, 6, 4};
const WordDef W_MINUTES  = {3, 0, 7};
const WordDef W_TO       = {3, 7, 2};
const WordDef W_PAST     = {4, 0, 4};
// Hours
const WordDef W_ONE      = {4, 5, 3};
const WordDef W_TWO      = {4, 8, 3};
const WordDef W_THREE    = {5, 0, 5};
const WordDef W_FOUR     = {5, 6, 4};
const WordDef W_FIVE_H   = {6, 0, 4};
const WordDef W_SIX      = {6, 5, 3};
const WordDef W_SEVEN    = {7, 0, 5};
const WordDef W_EIGHT    = {7, 6, 5};
const WordDef W_NINE     = {8, 0, 4};
const WordDef W_ELEVEN   = {8, 5, 6};
const WordDef W_TEN_H    = {9, 0, 3};
const WordDef W_TWELVE   = {9, 4, 6};

// ═══════════════════════════════════════════════════════════════
// 3×5 pixel font
// Each row = bitmask of width 3 (MSB = left column)
// ═══════════════════════════════════════════════════════════════
const uint8_t FONT[][5] = {
  {0b111,0b101,0b111,0b101,0b101}, // A
  {0b110,0b101,0b110,0b101,0b110}, // B
  {0b111,0b100,0b100,0b100,0b111}, // C
  {0b110,0b101,0b101,0b101,0b110}, // D
  {0b111,0b100,0b111,0b100,0b111}, // E
  {0b111,0b100,0b111,0b100,0b100}, // F
  {0b111,0b100,0b101,0b101,0b111}, // G
  {0b101,0b101,0b111,0b101,0b101}, // H
  {0b111,0b010,0b010,0b010,0b111}, // I
  {0b011,0b001,0b001,0b101,0b111}, // J
  {0b101,0b110,0b100,0b110,0b101}, // K
  {0b100,0b100,0b100,0b100,0b111}, // L
  {0b101,0b111,0b111,0b101,0b101}, // M
  {0b101,0b111,0b111,0b111,0b101}, // N
  {0b111,0b101,0b101,0b101,0b111}, // O
  {0b111,0b101,0b111,0b100,0b100}, // P
  {0b111,0b101,0b101,0b110,0b111}, // Q
  {0b110,0b101,0b110,0b110,0b101}, // R
  {0b111,0b100,0b111,0b001,0b111}, // S
  {0b111,0b010,0b010,0b010,0b010}, // T
  {0b101,0b101,0b101,0b101,0b111}, // U
  {0b101,0b101,0b101,0b111,0b010}, // V
  {0b101,0b101,0b111,0b111,0b101}, // W
  {0b101,0b101,0b010,0b101,0b101}, // X
  {0b101,0b101,0b111,0b010,0b010}, // Y
  {0b111,0b001,0b010,0b100,0b111}, // Z
};

// ═══════════════════════════════════════════════════════════════
// Frame buffer  64×64 × 3 bytes = 12,288 bytes
// ═══════════════════════════════════════════════════════════════
uint8_t frameBuf[DISPLAY_SIZE][DISPLAY_SIZE][3];
bool    activeCell[ROWS][COLS];

// ─────────────────────────────────────────────────────────────
void clearBuffer() {
  for (int y = 0; y < DISPLAY_SIZE; y++)
    for (int x = 0; x < DISPLAY_SIZE; x++)
      memcpy(frameBuf[y][x], COLOR_BG, 3);
}

void drawChar(int cx, int cy, uint8_t ch, const uint8_t color[3]) {
  if (ch < 'A' || ch > 'Z') return;
  const uint8_t* glyph = FONT[ch - 'A'];
  for (int ry = 0; ry < 5; ry++) {
    uint8_t row = glyph[ry];
    for (int rx = 0; rx < 3; rx++) {
      if (row & (0b100 >> rx)) {
        int px = cx + rx, py = cy + ry;
        if (px >= 0 && px < DISPLAY_SIZE && py >= 0 && py < DISPLAY_SIZE)
          memcpy(frameBuf[py][px], color, 3);
      }
    }
  }
}

void markWord(const WordDef& w) {
  for (int c = w.col; c < w.col + w.len; c++)
    activeCell[w.row][c] = true;
}

void buildFrame(int hour, int minute) {
  clearBuffer();
  memset(activeCell, 0, sizeof(activeCell));

  int m5 = (minute / 5) * 5;
  int h  = hour % 12;
  int hn = (h + 1) % 12;

  const WordDef* HOURS[12] = {
    &W_TWELVE, &W_ONE,   &W_TWO,   &W_THREE, &W_FOUR,
    &W_FIVE_H, &W_SIX,   &W_SEVEN, &W_EIGHT, &W_NINE,
    &W_TEN_H,  &W_ELEVEN
  };

  markWord(W_IT);
  markWord(W_IS);

  switch (m5) {
    case 0:
      markWord(*HOURS[h]);
      break;
    case 5:
      markWord(W_FIVE_M); markWord(W_MINUTES); markWord(W_PAST); markWord(*HOURS[h]);
      break;
    case 10:
      markWord(W_TEN_M);  markWord(W_MINUTES); markWord(W_PAST); markWord(*HOURS[h]);
      break;
    case 15:
      markWord(W_QUARTER); markWord(W_PAST); markWord(*HOURS[h]);
      break;
    case 20:
      markWord(W_TWENTY); markWord(W_MINUTES); markWord(W_PAST); markWord(*HOURS[h]);
      break;
    case 25:
      markWord(W_TWENTY); markWord(W_FIVE_M); markWord(W_MINUTES); markWord(W_PAST); markWord(*HOURS[h]);
      break;
    case 30:
      markWord(W_HALF); markWord(W_PAST); markWord(*HOURS[h]);
      break;
    case 35:
      markWord(W_TWENTY); markWord(W_FIVE_M); markWord(W_MINUTES); markWord(W_TO); markWord(*HOURS[hn]);
      break;
    case 40:
      markWord(W_TWENTY); markWord(W_MINUTES); markWord(W_TO); markWord(*HOURS[hn]);
      break;
    case 45:
      markWord(W_QUARTER); markWord(W_TO); markWord(*HOURS[hn]);
      break;
    case 50:
      markWord(W_TEN_M);  markWord(W_MINUTES); markWord(W_TO); markWord(*HOURS[hn]);
      break;
    case 55:
      markWord(W_FIVE_M); markWord(W_MINUTES); markWord(W_TO); markWord(*HOURS[hn]);
      break;
  }

  // Draw grid
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      int cx = OFF_X + col * CELL_W;
      int cy = OFF_Y + row * CELL_H;
      const uint8_t* color = activeCell[row][col] ? COLOR_FG : COLOR_DIM;
      drawChar(cx, cy, GRID[row][col], color);
    }
  }

  // Minute corner dots (remainder minutes 1–4)
  const int DOTS[4][2] = {{0,63},{63,63},{63,0},{0,0}};
  int extra = minute % 5;
  for (int d = 0; d < extra; d++) {
    int py = DOTS[d][0], px = DOTS[d][1];
    memcpy(frameBuf[py][px], COLOR_DOT, 3);
  }
}

// ═══════════════════════════════════════════════════════════════
// Base64 encode frame buffer
// ═══════════════════════════════════════════════════════════════
String frameToBase64() {
  const int RAW_LEN = DISPLAY_SIZE * DISPLAY_SIZE * 3;
  return base64::encode((uint8_t*)frameBuf, RAW_LEN);
}

// ═══════════════════════════════════════════════════════════════
// Pixoo64 HTTP API
// ═══════════════════════════════════════════════════════════════
String pixooUrl() {
  return String("http://") + PIXOO_IP + ":" + PIXOO_PORT + "/post";
}

bool pixooPost(const String& jsonBody) {
  HTTPClient http;
  http.begin(pixooUrl());
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(jsonBody);
  http.end();
  return (code == 200);
}

void pixooSetBrightness(int v) {
  StaticJsonDocument<64> doc;
  doc["Command"]    = "Channel/SetBrightness";
  doc["Brightness"] = v;
  String body;
  serializeJson(doc, body);
  pixooPost(body);
}

void pixooResetGif() {
  pixooPost("{\"Command\":\"Draw/ResetHttpGifId\"}");
}

void pixooSendFrame(const String& b64data) {
  String body = "{\"Command\":\"Draw/SendHttpGif\","
                "\"PicNum\":1,"
                "\"PicWidth\":" + String(DISPLAY_SIZE) + ","
                "\"PicOffset\":0,"
                "\"PicID\":1,"
                "\"PicSpeed\":1000,"
                "\"PicData\":\"" + b64data + "\"}";
  pixooPost(body);
}

// ═══════════════════════════════════════════════════════════════
// NTP
// ═══════════════════════════════════════════════════════════════
WiFiUDP   ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_S, 60000);

// ═══════════════════════════════════════════════════════════════
// Setup & Loop
// ═══════════════════════════════════════════════════════════════
int lastM5 = -1;

void setup() {
  Serial.begin(115200);
  Serial.println("\nEnglish Wordclock – ESP32 → Pixoo64");

  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  ntpClient.begin();
  ntpClient.update();

  pixooSetBrightness(BRIGHTNESS);
  Serial.printf("Brightness: %d%%\n", BRIGHTNESS);
}

void loop() {
  ntpClient.update();

  int hour   = ntpClient.getHours();
  int minute = ntpClient.getMinutes();
  int m5     = (minute / 5) * 5;

  if (m5 != lastM5) {
    Serial.printf("[%02d:%02d] → Building frame...\n", hour, minute);

    buildFrame(hour, minute);
    String b64 = frameToBase64();

    pixooResetGif();
    pixooSendFrame(b64);

    Serial.println("  ✓ Frame sent");
    lastM5 = m5;
  }

  delay(10000);  // check every 10 seconds
}
