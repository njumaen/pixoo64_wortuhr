/*
 * Deutsche Wordclock für Pixoo64 – ESP32
 * =======================================
 * Der ESP32 holt die Zeit per NTP, baut das Buchstabenraster
 * und schickt jeden Frame per HTTP an den Pixoo64.
 *
 * Benötigte Bibliotheken (Arduino Library Manager):
 *   - ArduinoJson  (Benoit Blanchon)
 *   - NTPClient    (Fabrice Weinberg)
 *
 * Board: "ESP32 Dev Module" (oder dein spezifisches Board)
 *
 * Konfiguration: nur den Abschnitt "EINSTELLUNGEN" anpassen.
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <base64.h>   // im ESP32-Core enthalten

// ═══════════════════════════════════════════════════════════════
// EINSTELLUNGEN
// ═══════════════════════════════════════════════════════════════
const char* WIFI_SSID     = "DEIN_WLAN_NAME";
const char* WIFI_PASSWORD = "DEIN_WLAN_PASSWORT";
const char* PIXOO_IP      = "192.168.1.50";   // IP deines Pixoo64
const int   PIXOO_PORT    = 80;
const int   BRIGHTNESS    = 80;               // 0–100
const long  UTC_OFFSET_S  = 3600;             // MEZ=3600, MESZ=7200

// Farben  (R, G, B)
const uint8_t COLOR_FG[3]  = {255, 255, 255}; // aktive Buchstaben
const uint8_t COLOR_DIM[3] = { 40,  40,  60}; // inaktive Buchstaben
const uint8_t COLOR_BG[3]  = {  0,   0,   0}; // Hintergrund
const uint8_t COLOR_DOT[3] = {255, 200,   0}; // Minuten-Punkte

// ═══════════════════════════════════════════════════════════════
// Konstanten
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
// Buchstabenraster  10 × 11
// Alle Wörter verifiziert, exakt 11 Zeichen pro Zeile
// ═══════════════════════════════════════════════════════════════
// Umlaute als Mehrbytezeichen würden den Index verschieben –
// wir ersetzen Ä→A Ö→O Ü→U nur für die Anzeige im Font.
// Im Raster speichern wir Sonderzeichen als eigene Codes:
//   0xC4 = Ä   0xD6 = Ö   0xDC = Ü
const uint8_t GRID[ROWS][COLS] = {
  {'E','S','K','I','S','T','L','F',0xDC,'N','F'}, // 0 ES IST FÜNF
  {'Z','E','H','N','Z','W','A','N','Z','I','G'}, // 1 ZEHN ZWANZIG
  {'D','R','E','I','V','I','E','R','T','E','L'}, // 2 DREI VIERTEL
  {'T','G','N','A','C','H','V','O','R','J','M'}, // 3 NACH VOR
  {'H','A','L','B','Q','Z','W',0xD6,'L','F','P'}, // 4 HALB ZWÖLF
  {'Z','W','E','I','N','S','I','E','B','E','N'}, // 5 ZWEI EIN(S) SIEBEN
  {'K','D','R','E','I','I','F',0xDC,'N','F','Q'}, // 6 DREI FÜNF(h)
  {'E','L','F','N','E','U','N','V','I','E','R'}, // 7 ELF NEUN VIER
  {'W','A','C','H','T','Z','E','H','N','R','S'}, // 8 ACHT ZEHN(h)
  {'B','S','E','C','H','S','F','M','U','H','R'}, // 9 SECHS UHR
};

// ═══════════════════════════════════════════════════════════════
// Wort-Definitionen  {zeile, start_spalte, länge}
// ═══════════════════════════════════════════════════════════════
struct WordDef { uint8_t row, col, len; };

// Minuten
const WordDef W_ES       = {0, 0, 2};
const WordDef W_IST      = {0, 3, 3};
const WordDef W_FUENF_M  = {0, 7, 4};
const WordDef W_ZEHN_M   = {1, 0, 4};
const WordDef W_ZWANZIG  = {1, 4, 7};
const WordDef W_DREI_V   = {2, 0, 4};
const WordDef W_VIERTEL  = {2, 4, 7};
const WordDef W_NACH     = {3, 2, 4};
const WordDef W_VOR      = {3, 6, 3};
const WordDef W_HALB     = {4, 0, 4};
// Stunden
const WordDef W_ZWOELF   = {4, 5, 5};
const WordDef W_EIN      = {5, 2, 3};
const WordDef W_EINS     = {5, 2, 4};
const WordDef W_ZWEI     = {5, 0, 4};
const WordDef W_SIEBEN   = {5, 5, 6};
const WordDef W_DREI     = {6, 1, 4};
const WordDef W_FUENF_H  = {6, 6, 4};
const WordDef W_ELF      = {7, 0, 3};
const WordDef W_NEUN     = {7, 3, 4};
const WordDef W_VIER     = {7, 7, 4};
const WordDef W_ACHT     = {8, 1, 4};
const WordDef W_ZEHN_H   = {8, 5, 4};
const WordDef W_SECHS    = {9, 1, 5};
const WordDef W_UHR      = {9, 8, 3};

// ═══════════════════════════════════════════════════════════════
// 3×5-Pixel-Font
// Jede Zeile = Bitmaske der Breite 3 (MSB = linke Spalte)
// ═══════════════════════════════════════════════════════════════
const uint8_t FONT[][5] = {
  // A-Z in ASCII-Reihenfolge ab Index 0 = 'A'
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

// Umlaute: Ä Ö Ü
const uint8_t FONT_AE[5] = {0b010,0b111,0b101,0b111,0b101};
const uint8_t FONT_OE[5] = {0b010,0b111,0b101,0b101,0b111};
const uint8_t FONT_UE[5] = {0b010,0b101,0b101,0b101,0b111};

// ═══════════════════════════════════════════════════════════════
// Frame-Buffer  64×64 × 3 Bytes = 12 288 Bytes
// ═══════════════════════════════════════════════════════════════
uint8_t frameBuf[DISPLAY_SIZE][DISPLAY_SIZE][3];

// aktive Zellen-Flags
bool activeCell[ROWS][COLS];

// ─────────────────────────────────────────────────────────────
void clearBuffer() {
  for (int y = 0; y < DISPLAY_SIZE; y++)
    for (int x = 0; x < DISPLAY_SIZE; x++)
      memcpy(frameBuf[y][x], COLOR_BG, 3);
}

void drawChar(int cx, int cy, uint8_t ch, const uint8_t color[3]) {
  const uint8_t* glyph;
  if      (ch == 0xC4) glyph = FONT_AE;   // Ä
  else if (ch == 0xD6) glyph = FONT_OE;   // Ö
  else if (ch == 0xDC) glyph = FONT_UE;   // Ü
  else if (ch >= 'A' && ch <= 'Z') glyph = FONT[ch - 'A'];
  else return;  // unbekanntes Zeichen → überspringen

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

  // Stunden-Wörter
  const WordDef* HOURS[12] = {
    &W_ZWOELF, &W_EIN, &W_ZWEI, &W_DREI, &W_VIER,
    &W_FUENF_H, &W_SECHS, &W_SIEBEN, &W_ACHT, &W_NEUN,
    &W_ZEHN_H, &W_ELF
  };

  markWord(W_ES);
  markWord(W_IST);

  switch (m5) {
    case 0:
      if (h == 1) markWord(W_EINS); else markWord(*HOURS[h]);
      markWord(W_UHR);
      break;
    case 5:
      markWord(W_FUENF_M); markWord(W_NACH); markWord(*HOURS[h]);
      break;
    case 10:
      markWord(W_ZEHN_M);  markWord(W_NACH); markWord(*HOURS[h]);
      break;
    case 15:
      markWord(W_VIERTEL); markWord(W_NACH); markWord(*HOURS[h]);
      break;
    case 20:
      markWord(W_ZWANZIG); markWord(W_NACH); markWord(*HOURS[h]);
      break;
    case 25:
      markWord(W_FUENF_M); markWord(W_VOR);  markWord(W_HALB); markWord(*HOURS[hn]);
      break;
    case 30:
      markWord(W_HALB);    markWord(*HOURS[hn]);
      break;
    case 35:
      markWord(W_FUENF_M); markWord(W_NACH); markWord(W_HALB); markWord(*HOURS[hn]);
      break;
    case 40:
      markWord(W_ZWANZIG); markWord(W_VOR);  markWord(*HOURS[hn]);
      break;
    case 45:
      markWord(W_VIERTEL); markWord(W_VOR);  markWord(*HOURS[hn]);
      break;
    case 50:
      markWord(W_ZEHN_M);  markWord(W_VOR);  markWord(*HOURS[hn]);
      break;
    case 55:
      markWord(W_FUENF_M); markWord(W_VOR);  markWord(*HOURS[hn]);
      break;
  }

  // Raster zeichnen
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      int cx = OFF_X + col * CELL_W;
      int cy = OFF_Y + row * CELL_H;
      const uint8_t* color = activeCell[row][col] ? COLOR_FG : COLOR_DIM;
      drawChar(cx, cy, GRID[row][col], color);
    }
  }

  // Minuten-Eckpunkte (Rest-Minuten 1–4)
  const int DOTS[4][2] = {{0,63},{63,63},{63,0},{0,0}};
  int extra = minute % 5;
  for (int d = 0; d < extra; d++) {
    int py = DOTS[d][0], px = DOTS[d][1];
    memcpy(frameBuf[py][px], COLOR_DOT, 3);
  }
}

// ═══════════════════════════════════════════════════════════════
// Base64-Kodierung des Frame-Buffers
// ═══════════════════════════════════════════════════════════════
String frameToBase64() {
  // 12288 Bytes raw → ~16384 Bytes base64
  const int RAW_LEN = DISPLAY_SIZE * DISPLAY_SIZE * 3;
  uint8_t* raw = (uint8_t*)frameBuf;  // flach lesen
  return base64::encode(raw, RAW_LEN);
}

// ═══════════════════════════════════════════════════════════════
// Pixoo64 HTTP-API
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
  // JSON manuell bauen – ArduinoJson würde 16 KB+ Heap brauchen
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
  Serial.println("\nDeutsche Wordclock – ESP32 → Pixoo64");

  // WLAN verbinden
  Serial.printf("Verbinde mit %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nVerbunden! IP: %s\n", WiFi.localIP().toString().c_str());

  // NTP starten
  ntpClient.begin();
  ntpClient.update();

  // Helligkeit setzen
  pixooSetBrightness(BRIGHTNESS);
  Serial.printf("Helligkeit: %d%%\n", BRIGHTNESS);
}

void loop() {
  ntpClient.update();

  int hour   = ntpClient.getHours();
  int minute = ntpClient.getMinutes();
  int m5     = (minute / 5) * 5;

  if (m5 != lastM5) {
    Serial.printf("[%02d:%02d] → Frame wird gebaut...\n", hour, minute);

    buildFrame(hour, minute);
    String b64 = frameToBase64();

    pixooResetGif();
    pixooSendFrame(b64);

    Serial.println("  ✓ Frame übertragen");
    lastM5 = m5;
  }

  delay(10000);  // alle 10 Sekunden prüfen
}
