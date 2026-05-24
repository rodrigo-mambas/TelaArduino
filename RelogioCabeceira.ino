// Relogio de cabeceira: Mega 2560 + Ethernet W5100 + OpenSmart 3.5" TFT (ST7793)
// Previsao do tempo de 5 dias via Open-Meteo (gratis, sem cadastro, HTTP)

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include <ArduinoJson.h>

// ============================================================
// CONFIGURACAO: ajuste para a sua cidade
// Encontre lat/lon em: https://www.latlong.net/
#define LATITUDE   "-23.5505"   // Sao Paulo
#define LONGITUDE  "-46.6333"   // Sao Paulo
// ============================================================

// --------- Rede ---------
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3L * 3600L, 60000UL);

// --------- TFT ---------
MCUFRIEND_kbv tft;
#define BLACK   0x0000
#define WHITE   0xFFFF
#define CYAN    0x07FF
#define YELLOW  0xFFE0
#define GRAY    0x7BEF
#define RED     0xF800
#define ORANGE  0xFBA0
#define LBLUE   0x049F

const uint8_t TFT_SD_CS_PIN = 53;

// --------- Localizacao PT-BR ---------
const char* DIAS[]      = {"Domingo","Segunda","Terca","Quarta","Quinta","Sexta","Sabado"};
const char* MESES[]     = {"Jan","Fev","Mar","Abr","Mai","Jun","Jul","Ago","Set","Out","Nov","Dez"};
const char* DIA_ABREV[] = {"DOM","SEG","TER","QUA","QUI","SEX","SAB"};

// --------- Layout (pixels, landscape 400x240) ---------
const int16_t SCR_W = 400;
const int16_t SCR_H = 240;
#define Y_HORA      5
#define Y_DATA      68
#define Y_SEP       90
#define Y_DIA_NOME  97
#define Y_TEMP_MAX  120
#define Y_TEMP_MIN  143
#define Y_CHUVA     166
#define Y_STATUS    197
#define COL_W       (SCR_W / 5)  // 80px por coluna

// --------- Estado do relogio ---------
int  lastMin  = -1;
int  lastHour = -1;
int  lastDay  = -1;
unsigned long lastNtpMs = 0;
const unsigned long NTP_RESYNC_MS = 6UL * 3600UL * 1000UL;

// --------- Previsao do tempo ---------
struct DiaClima { int8_t tMax; int8_t tMin; uint8_t chuva; };
DiaClima previsao[5];
bool climaValido    = false;
bool climaAtualizar = false;
unsigned long ultimoClimaMs = 0;
const unsigned long CLIMA_INTERVALO = 30UL * 60UL * 1000UL; // 30 min
int8_t climaHora = -1, climaMinuto = -1;

// Documentos JSON globais (memoria fixa, sem heap fragmentation)
StaticJsonDocument<192> filtroClima;
StaticJsonDocument<512> docClima;

// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(TFT_SD_CS_PIN, OUTPUT);
  digitalWrite(TFT_SD_CS_PIN, HIGH);

  // TFT
  uint16_t id = tft.readID();
  Serial.print(F("TFT ID: 0x")); Serial.println(id, HEX);
  if (id == 0x0 || id == 0xFFFF || id == 0xD3D3) id = 0x7793;
  tft.begin(id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  // Configura filtro JSON uma unica vez
  filtroClima["daily"]["temperature_2m_max"] = true;
  filtroClima["daily"]["temperature_2m_min"] = true;
  filtroClima["daily"]["precipitation_sum"]  = true;

  showBootMessage("Iniciando...", WHITE);

  // Ethernet DHCP
  showBootMessage("Conectando rede...", WHITE);
  if (Ethernet.begin(mac) == 0) {
    showBootMessage("Falha DHCP!", RED);
    while (true) delay(1000);
  }
  delay(500); // aguarda DNS estabilizar
  char ipBuf[32];
  IPAddress ip = Ethernet.localIP();
  snprintf(ipBuf, sizeof(ipBuf), "IP: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  showBootMessage(ipBuf, WHITE);

  // NTP
  showBootMessage("Sincronizando NTP...", WHITE);
  ntp.begin();
  if (!ntp.forceUpdate()) {
    showBootMessage("NTP falhou, tentando...", RED);
    delay(2000);
    ntp.forceUpdate();
  }
  setTime(ntp.getEpochTime());
  lastNtpMs = millis();

  // Primeira previsao
  showBootMessage("Buscando previsao...", WHITE);
  buscarClima();

  delay(400);
  tft.fillScreen(BLACK);
  tft.drawFastHLine(0, Y_SEP, SCR_W, GRAY);
  desenharPrevisao();
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

  if (millis() - ultimoClimaMs > CLIMA_INTERVALO) {
    buscarClima();
  }

  updateClock();

  if (climaAtualizar) {
    desenharPrevisao();
    climaAtualizar = false;
  }

  delay(250);
}

// ============================================================
bool buscarClima() {
  EthernetClient client;
  if (!client.connect("api.open-meteo.com", 80)) {
    Serial.println(F("Clima: falha na conexao"));
    ultimoClimaMs = millis();
    return false;
  }

  client.print(F("GET /v1/forecast?latitude="));
  client.print(F(LATITUDE));
  client.print(F("&longitude="));
  client.print(F(LONGITUDE));
  client.print(F("&daily=temperature_2m_max,temperature_2m_min,precipitation_sum"));
  client.print(F("&timezone=America%2FSao_Paulo&forecast_days=5 HTTP/1.0\r\n"));
  client.print(F("Host: api.open-meteo.com\r\n"));
  client.print(F("Connection: close\r\n\r\n"));

  // Aguarda resposta com timeout de 10s
  unsigned long t = millis();
  while (!client.available() && millis() - t < 10000) delay(50);
  if (!client.available()) {
    Serial.println(F("Clima: timeout"));
    client.stop();
    ultimoClimaMs = millis();
    return false;
  }

  pularCabecalhoHttp(client);

  docClima.clear();
  DeserializationError err = deserializeJson(
    docClima, client, DeserializationOption::Filter(filtroClima));
  client.stop();

  if (err) {
    Serial.print(F("Clima JSON erro: ")); Serial.println(err.c_str());
    ultimoClimaMs = millis();
    return false;
  }

  auto daily = docClima["daily"];
  for (uint8_t i = 0; i < 5; i++) {
    previsao[i].tMax = (int8_t)round((float)daily["temperature_2m_max"][i]);
    previsao[i].tMin = (int8_t)round((float)daily["temperature_2m_min"][i]);
    float mm = daily["precipitation_sum"][i] | 0.0f;
    previsao[i].chuva = (uint8_t)(mm > 255 ? 255 : mm);
  }

  climaValido    = true;
  climaAtualizar = true;
  ultimoClimaMs  = millis();
  climaHora      = (int8_t)hour();
  climaMinuto    = (int8_t)minute();
  Serial.println(F("Clima OK"));
  return true;
}

void pularCabecalhoHttp(EthernetClient& client) {
  uint8_t st = 0;
  unsigned long t = millis();
  while (client.connected() && millis() - t < 5000) {
    while (client.available()) {
      char c = client.read();
      if      (st == 0 && c == '\r') st = 1;
      else if (st == 1 && c == '\n') st = 2;
      else if (st == 2 && c == '\r') st = 3;
      else if (st == 3 && c == '\n') return;
      else st = (c == '\r') ? 1 : 0;
    }
  }
}

// ============================================================
void updateClock() {
  int h = hour(), m = minute(), d = day(), mo = month(), y = year();
  int dw = weekday() - 1;

  if (m != lastMin || h != lastHour) {
    char buf[6];
    sprintf(buf, "%02d:%02d", h, m);
    drawCenteredText(buf, Y_HORA, 7, CYAN, BLACK, SCR_W, 62);
    lastMin = m; lastHour = h;
    atualizarStatusClima();
  }

  if (d != lastDay) {
    char buf[40];
    sprintf(buf, "%s, %02d %s %d", DIAS[dw], d, MESES[mo - 1], y);
    drawCenteredText(buf, Y_DATA, 2, YELLOW, BLACK, SCR_W, 18);
    lastDay = d;
    if (climaValido) climaAtualizar = true; // atualiza labels dos dias
  }
}

// ============================================================
void desenharPrevisao() {
  int dwHoje = weekday() - 1; // 0=Dom

  for (uint8_t i = 0; i < 5; i++) {
    int16_t xCentro = i * COL_W + COL_W / 2;

    // Label do dia
    const char* label;
    if      (i == 0) label = "HOJ";
    else if (i == 1) label = "AMH";
    else             label = DIA_ABREV[(dwHoje + i) % 7];
    drawColText(label, Y_DIA_NOME, 2, WHITE, xCentro);

    char buf[8];
    if (climaValido) {
      // Temperatura maxima em laranja
      sprintf(buf, "%d\xB0", (int)previsao[i].tMax);
      drawColText(buf, Y_TEMP_MAX, 2, ORANGE, xCentro);

      // Temperatura minima em ciano
      sprintf(buf, "%d\xB0", (int)previsao[i].tMin);
      drawColText(buf, Y_TEMP_MIN, 2, CYAN, xCentro);

      // Chuva: mostra mm se >= 1, "---" se seco
      if (previsao[i].chuva >= 1) {
        int mm = (previsao[i].chuva > 99) ? 99 : previsao[i].chuva;
        sprintf(buf, "%dmm", mm);
        drawColText(buf, Y_CHUVA, 2, LBLUE, xCentro);
      } else {
        drawColText("---", Y_CHUVA, 2, GRAY, xCentro);
      }
    } else {
      drawColText("--\xB0", Y_TEMP_MAX, 2, GRAY, xCentro);
      drawColText("--\xB0", Y_TEMP_MIN, 2, GRAY, xCentro);
      drawColText("---",    Y_CHUVA,    2, GRAY, xCentro);
    }

    // Linha separadora vertical entre colunas
    if (i < 4) {
      tft.drawFastVLine((i + 1) * COL_W, Y_SEP + 1, SCR_H - Y_SEP - 10, GRAY);
    }
  }

  tft.drawFastHLine(0, Y_SEP, SCR_W, GRAY);
  atualizarStatusClima();
}

void atualizarStatusClima() {
  tft.fillRect(0, Y_STATUS, SCR_W, 10, BLACK);
  tft.setTextColor(GRAY);
  tft.setTextSize(1);
  char buf[28];
  if (climaValido && climaHora >= 0) {
    sprintf(buf, "Clima: %02d:%02d", (int)climaHora, (int)climaMinuto);
  } else {
    strcpy(buf, "Clima: aguardando...");
  }
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(SCR_W - w - 2, Y_STATUS);
  tft.print(buf);
}

// ============================================================
void drawCenteredText(const char* txt, int16_t y, uint8_t size,
                      uint16_t fg, uint16_t bg,
                      int16_t areaW, int16_t areaH) {
  tft.fillRect(0, y, areaW, areaH, bg);
  tft.setTextSize(size);
  tft.setTextColor(fg);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t xPos = (areaW - (int16_t)w) / 2;
  if (xPos < 0) xPos = 0;
  tft.setCursor(xPos, y);
  tft.print(txt);
}

void drawColText(const char* txt, int16_t y, uint8_t size,
                 uint16_t fg, int16_t xCentro) {
  tft.fillRect(xCentro - COL_W / 2, y, COL_W, size * 8 + 2, BLACK);
  tft.setTextSize(size);
  tft.setTextColor(fg);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  int16_t xPos = xCentro - (int16_t)w / 2;
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
