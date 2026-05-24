// Scanner completo de touch - monitora TODOS os pinos, incluindo os do display.
// Durante a leitura o display e pausado para nao interferir.
// Abre Serial Monitor 115200 e toca a tela com alguma pressao.

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;
#define BLACK  0x0000
#define WHITE  0xFFFF
#define GREEN  0x07E0
#define RED    0xF800

// Todos os pinos a testar (display + livres)
const uint8_t PINOS[] = {
  2, 3, 4, 5, 6, 7, 8, 9,          // DB0-DB7 do display
  A0, A1, A2, A3, A4,               // RD WR RS CS RST do display
  10, 11, 12, 13,                    // pinos livres digitais
  A5, A6, A7, A8, A9, A10,          // pinos livres analogicos
  22, 23, 24, 25, 26, 27, 28, 29,   // pinos extendidos Mega
  38, 39, 40, 41, 42                 // pinos extendidos Mega
};
const uint8_t N = sizeof(PINOS) / sizeof(PINOS[0]);
uint8_t estadoBase[sizeof(PINOS) / sizeof(PINOS[0])];

bool displayAtivo = true;
unsigned long tDisplay = 0;

void setup() {
  Serial.begin(115200);
  Serial.println(F("=== Scanner COMPLETO de Touch ==="));

  // Inicializa display so para confirmar que funciona
  uint16_t id = tft.readID();
  if (id == 0x0 || id == 0xFFFF || id == 0xD3D3) id = 0x7793;
  tft.begin(id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 80);
  tft.print(F("Toque a tela"));
  tft.setCursor(10, 110);
  tft.print(F("com pressao!"));

  // Pausa o display: configura todos os pinos como INPUT_PULLUP
  // para poder ler o touch sem interferencia
  for (uint8_t i = 0; i < N; i++) {
    pinMode(PINOS[i], INPUT_PULLUP);
  }
  delay(5); // aguarda estabilizar

  // Salva estado base (sem toque)
  Serial.println(F("Estado inicial dos pinos (sem toque):"));
  for (uint8_t i = 0; i < N; i++) {
    estadoBase[i] = digitalRead(PINOS[i]);
    // Imprime apenas os que estao LOW (suspeitos)
    if (estadoBase[i] == LOW) {
      Serial.print(F("  !!! Pino ")); Serial.print(PINOS[i]);
      Serial.println(F(" esta LOW ja de inicio (verificar)"));
    }
  }
  Serial.println(F("Pronto. TOQUE A TELA COM PRESSAO FIRME:"));
  Serial.println(F("------------------------------------------"));
}

void loop() {
  // Faz 20 leituras rapidas e conta quantas vezes cada pino foi LOW
  uint8_t contLow[N] = {0};
  for (uint8_t s = 0; s < 20; s++) {
    for (uint8_t i = 0; i < N; i++) {
      if (digitalRead(PINOS[i]) == LOW) contLow[i]++;
    }
    delayMicroseconds(200);
  }

  // Verifica se algum pino mudou de HIGH para LOW de forma consistente
  bool algumMudou = false;
  for (uint8_t i = 0; i < N; i++) {
    // Considera positivo se ficou LOW em pelo menos 12 das 20 amostras
    // e o estado base era HIGH
    if (estadoBase[i] == HIGH && contLow[i] >= 12) {
      Serial.print(F(">>> TOQUE no pino "));
      Serial.print(PINOS[i]);
      Serial.print(F(" (LOW em "));
      Serial.print(contLow[i]);
      Serial.println(F("/20 amostras)"));
      algumMudou = true;
    }
  }

  // Atualiza o display brevemente a cada 2s para mostrar status
  if (millis() - tDisplay > 2000) {
    tDisplay = millis();

    // Reativa display momentaneamente
    uint16_t id = tft.readID();
    if (id == 0x0 || id == 0xFFFF || id == 0xD3D3) id = 0x7793;
    tft.begin(id);
    tft.setRotation(1);
    tft.fillScreen(BLACK);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, 60);
    tft.print(F("Toque com pressao"));
    tft.setCursor(10, 90);
    tft.print(F("Ver Serial 115200"));

    // Pausa display de novo para leitura de touch
    for (uint8_t i = 0; i < N; i++) {
      pinMode(PINOS[i], INPUT_PULLUP);
    }
    delay(3);
    // Atualiza estado base
    for (uint8_t i = 0; i < N; i++) {
      estadoBase[i] = digitalRead(PINOS[i]);
    }

    if (!algumMudou) {
      Serial.println(F("(aguardando toque...)"));
    }
  }
}
