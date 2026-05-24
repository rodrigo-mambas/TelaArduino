// Relogio de cabeceira: Mega 2560 + Ethernet W5100 + OpenSmart 3.5" TFT (ST7793)
// Sincroniza hora via NTP (servidor NTP.br) e exibe data/hora em tela cheia.

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

// --------- Rede ---------
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetUDP udp;
// UTC-3 (Brasil sem horario de verao). Atualizacao interna a cada 60s.
NTPClient ntp(udp, "a.st1.ntp.br", -3L * 3600L, 60000UL);

// --------- TFT ---------
MCUFRIEND_kbv tft;
#define BLACK   0x0000
#define WHITE   0xFFFF
#define CYAN    0x07FF
#define YELLOW  0xFFE0
#define GRAY    0x7BEF
#define RED     0xF800

// SD do shield TFT compartilha SPI com o Ethernet. Mantemos seu CS desativado.
// No Mega o HW-SS (D53) precisa ficar como OUTPUT para o SPI master funcionar.
const uint8_t TFT_SD_CS_PIN = 53;

// --------- Localizacao PT-BR ---------
const char* DIAS[]  = {"Domingo","Segunda","Terca","Quarta","Quinta","Sexta","Sabado"};
const char* MESES[] = {"Jan","Fev","Mar","Abr","Mai","Jun","Jul","Ago","Set","Out","Nov","Dez"};

// --------- Estado ---------
int  lastMin  = -1;
int  lastHour = -1;
int  lastDay  = -1;
unsigned long lastNtpMs = 0;
const unsigned long NTP_RESYNC_MS = 6UL * 3600UL * 1000UL; // 6 horas

// Dimensoes do display em modo paisagem (ST7793: 240x400 -> rotacionado: 400x240)
const int16_t SCR_W = 400;
const int16_t SCR_H = 240;

// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(TFT_SD_CS_PIN, OUTPUT);
  digitalWrite(TFT_SD_CS_PIN, HIGH);

  // ---- TFT ----
  uint16_t id = tft.readID();
  Serial.print(F("TFT ID lido: 0x")); Serial.println(id, HEX);
  if (id == 0x0 || id == 0xFFFF || id == 0xD3D3) id = 0x7793;
  tft.begin(id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  showBootMessage("Iniciando...", WHITE);

  // ---- Ethernet ----
  showBootMessage("Conectando rede (DHCP)...", WHITE);
  if (Ethernet.begin(mac) == 0) {
    showBootMessage("Falha DHCP. Verifique o cabo.", RED);
    while (true) { delay(1000); }
  }
  IPAddress ip = Ethernet.localIP();
  char ipBuf[32];
  snprintf(ipBuf, sizeof(ipBuf), "IP: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  showBootMessage(ipBuf, WHITE);

  // ---- NTP ----
  showBootMessage("Sincronizando NTP...", WHITE);
  ntp.begin();
  if (!ntp.forceUpdate()) {
    showBootMessage("Falha NTP. Tentando novamente...", RED);
    delay(2000);
    ntp.forceUpdate();
  }
  setTime(ntp.getEpochTime());
  lastNtpMs = millis();

  delay(800);
  tft.fillScreen(BLACK);
}

// ============================================================
void loop() {
  Ethernet.maintain();

  if (millis() - lastNtpMs > NTP_RESYNC_MS) {
    if (ntp.forceUpdate()) {
      setTime(ntp.getEpochTime());
      lastNtpMs = millis();
    }
  }

  updateClock();
  delay(250);
}

// ============================================================
void updateClock() {
  int h  = hour();
  int m  = minute();
  int d  = day();
  int mo = month();
  int y  = year();
  int dw = weekday() - 1;

  // ---- Hora HH:MM (centro, grande) ----
  if (m != lastMin || h != lastHour) {
    char buf[6];
    sprintf(buf, "%02d:%02d", h, m);
    drawCenteredText(buf, SCR_H / 2 - 35, 8, CYAN, BLACK, SCR_W, 80);
    lastMin = m;
    lastHour = h;
  }

  // ---- Data + dia da semana (rodape) ----
  if (d != lastDay) {
    char buf[40];
    sprintf(buf, "%s, %02d %s %d", DIAS[dw], d, MESES[mo - 1], y);
    drawCenteredText(buf, SCR_H - 50, 3, YELLOW, BLACK, SCR_W, 30);
    lastDay = d;
  }
}

// ============================================================
void drawCenteredText(const char* txt, int16_t y, uint8_t size,
                      uint16_t fg, uint16_t bg,
                      int16_t areaW, int16_t areaH) {
  tft.fillRect(0, y, areaW, areaH, bg);
  tft.setTextSize(size);
  tft.setTextColor(fg);
  int16_t x1, y1; uint16_t w, htxt;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &htxt);
  int16_t xPos = (areaW - (int16_t)w) / 2;
  if (xPos < 0) xPos = 0;
  tft.setCursor(xPos, y);
  tft.print(txt);
}

void showBootMessage(const char* txt, uint16_t color) {
  static int16_t y = 10;
  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(10, y);
  tft.print(txt);
  y += 24;
  if (y > SCR_H - 20) y = 10;
  Serial.println(txt);
}
