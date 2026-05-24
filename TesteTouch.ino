// Diagnostico de touch: descobre automaticamente quais pinos respondem ao toque.
// Monitora pinos D2-D13 e A0-A5. Abre Serial Monitor 115200 e toca a tela.
// O pino que mudar de estado quando tocado e o pino de touch.

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;
#define BLACK  0x0000
#define WHITE  0xFFFF
#define GREEN  0x07E0
#define YELLOW 0xFFE0

// Pinos a monitorar (exclui os ja usados pelo display: D2-D9, A0-A4)
// Monitoramos os que SOBRAM livres no Mega
const uint8_t PINOS[] = {
  10, 11, 12, 13,        // digitais livres
  14, 15, 16, 17,        // D14-D17 (TX3/RX3/TX2/RX2 - livres se nao usar serial)
  A5, A6, A7, A8,        // analogicos livres
  A9, A10, A11, A12,
  A13, A14, A15
};
const uint8_t N_PINOS = sizeof(PINOS) / sizeof(PINOS[0]);

uint8_t estadoAnterior[sizeof(PINOS) / sizeof(PINOS[0])];
bool qualquerMudanca = false;
unsigned long ultimaImpressao = 0;

void setup() {
  Serial.begin(115200);
  Serial.println(F("=== Scanner de Pinos de Touch ==="));
  Serial.println(F("Configure todos os pinos monitorados como INPUT_PULLUP"));
  Serial.println(F("e observe qual muda quando voce toca a tela."));
  Serial.println();

  // Inicializa display
  uint16_t id = tft.readID();
  if (id == 0x0 || id == 0xFFFF || id == 0xD3D3) id = 0x7793;
  tft.begin(id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print(F("Toque a tela"));
  tft.setCursor(10, 40);
  tft.print(F("Veja Serial Monitor"));

  // Configura todos os pinos monitorados como INPUT_PULLUP
  for (uint8_t i = 0; i < N_PINOS; i++) {
    pinMode(PINOS[i], INPUT_PULLUP);
    estadoAnterior[i] = digitalRead(PINOS[i]);
  }

  Serial.println(F("Pinos monitorados | Estado inicial:"));
  for (uint8_t i = 0; i < N_PINOS; i++) {
    Serial.print(F("  D/A")); Serial.print(PINOS[i]);
    Serial.print(F(" = ")); Serial.println(estadoAnterior[i] == LOW ? F("LOW") : F("HIGH"));
  }
  Serial.println(F(""));
  Serial.println(F(">>> TOQUE A TELA E OBSERVE <<<"));
  Serial.println(F("------------------------------"));
}

void loop() {
  // Le estado atual de todos os pinos
  for (uint8_t i = 0; i < N_PINOS; i++) {
    uint8_t estadoAtual = digitalRead(PINOS[i]);

    if (estadoAtual != estadoAnterior[i]) {
      Serial.print(F("*** MUDANCA no pino D"));
      Serial.print(PINOS[i]);
      Serial.print(F(": "));
      Serial.print(estadoAnterior[i] == LOW ? F("LOW") : F("HIGH"));
      Serial.print(F(" -> "));
      Serial.println(estadoAtual == LOW ? F("LOW (PUXADO) ***") : F("HIGH (SOLTO) ***"));

      estadoAnterior[i] = estadoAtual;
      qualquerMudanca = true;

      // Pisca a tela verde quando detecta mudanca
      tft.fillRect(0, 100, 400, 60, GREEN);
      tft.setTextColor(BLACK);
      tft.setTextSize(3);
      tft.setCursor(20, 115);
      tft.print(F("D")); tft.print(PINOS[i]); tft.print(F(" mudou!"));
    }
  }

  // A cada 5 segundos imprime status resumido no serial
  if (millis() - ultimaImpressao > 5000) {
    ultimaImpressao = millis();
    if (!qualquerMudanca) {
      Serial.println(F("(aguardando mudanca nos pinos... toque a tela)"));
    }
  }

  delay(10);
}
