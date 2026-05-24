// Teste de touch screen para OpenSmart 3.5" ST7793 + Arduino Mega 2560
//
// COMO USAR:
// 1. Sobe o sketch
// 2. Abre Serial Monitor (115200 baud)
// 3. Toca a tela e observa o que imprime no Serial
// 4. Ajuste os #define TOUCH_* se necessario

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>

MCUFRIEND_kbv tft;

#define BLACK   0x0000
#define WHITE   0xFFFF
#define GREEN   0x07E0
#define RED     0xF800
#define YELLOW  0xFFE0
#define CYAN    0x07FF

// ============================================================
// PINOS DE TOUCH - ajuste aqui se nao detectar corretamente
// Os pinos compartilham as linhas de dados do display.
// Mapeamento padrao OpenSmart 3.5" 8-bit ST7793:
#define TOUCH_YP   9    // Y+ compartilha com DB1
#define TOUCH_XM   2    // X- compartilha com DB2
#define TOUCH_XP   6    // X+ compartilha com DB6
#define TOUCH_YM   7    // Y- compartilha com DB7
// ============================================================

// Limiar: quantas leituras LOW consecutivas para confirmar toque
#define AMOSTRAS   5
#define LIMIAR_MIN 3   // minimo de amostras positivas para confirmar

int contadorToque = 0;
bool tocando      = false;

void setup() {
  Serial.begin(115200);
  Serial.println(F("=== Teste Touch OpenSmart 3.5 (ST7793) ==="));
  Serial.println(F("Pinos configurados:"));
  Serial.print(F("  YP (Y+) = D")); Serial.println(TOUCH_YP);
  Serial.print(F("  XM (X-) = D")); Serial.println(TOUCH_XM);
  Serial.print(F("  XP (X+) = D")); Serial.println(TOUCH_XP);
  Serial.print(F("  YM (Y-) = D")); Serial.println(TOUCH_YM);

  // Inicializa display
  uint16_t id = tft.readID();
  if (id == 0x0 || id == 0xFFFF || id == 0xD3D3) id = 0x7793;
  tft.begin(id);
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  drawEstado(false);
}

void loop() {
  // Acumula amostras para confirmar toque (evita ruido)
  int positivas = 0;
  for (int i = 0; i < AMOSTRAS; i++) {
    if (lerToque()) positivas++;
    delayMicroseconds(500);
  }

  bool toqueAtual = (positivas >= LIMIAR_MIN);

  // Borda de subida: novo toque detectado
  if (toqueAtual && !tocando) {
    contadorToque++;
    tocando = true;

    Serial.print(F("[TOQUE #")); Serial.print(contadorToque);
    Serial.print(F("] amostras positivas: ")); Serial.print(positivas);
    Serial.print(F("/")); Serial.println(AMOSTRAS);

    drawEstado(true);
    testarVariacaoTensao();
  }

  // Borda de descida: dedo levantado
  if (!toqueAtual && tocando) {
    tocando = false;
    Serial.println(F("[DEDO LEVANTADO]"));
    drawEstado(false);
  }

  delay(20);
}

// ============================================================
bool lerToque() {
  // Aplica tensao no eixo Y e le o eixo X para detectar contato
  pinMode(TOUCH_YM, OUTPUT);  digitalWrite(TOUCH_YM, LOW);
  pinMode(TOUCH_XP, INPUT);
  pinMode(TOUCH_XM, INPUT);
  pinMode(TOUCH_YP, INPUT_PULLUP);
  delayMicroseconds(30);
  bool resultado = (digitalRead(TOUCH_YP) == LOW);

  // Restaura pinos para OUTPUT antes do display assumir controle
  pinMode(TOUCH_YP, OUTPUT);
  pinMode(TOUCH_YM, OUTPUT);
  pinMode(TOUCH_XP, OUTPUT);
  pinMode(TOUCH_XM, OUTPUT);
  return resultado;
}

// Verifica os 4 eixos possiveis para ajudar a identificar pinos corretos
void testarVariacaoTensao() {
  Serial.println(F("--- Diagnostico de eixos ---"));

  // Eixo YP/YM
  Serial.print(F("  YP pullup, YM=LOW -> YP="));
  pinMode(TOUCH_YM, OUTPUT); digitalWrite(TOUCH_YM, LOW);
  pinMode(TOUCH_XP, INPUT);  pinMode(TOUCH_XM, INPUT);
  pinMode(TOUCH_YP, INPUT_PULLUP); delayMicroseconds(50);
  Serial.println(digitalRead(TOUCH_YP) == LOW ? F("LOW (contato!)") : F("HIGH (sem contato)"));

  // Eixo XP/XM
  Serial.print(F("  XP pullup, XM=LOW -> XP="));
  pinMode(TOUCH_XM, OUTPUT); digitalWrite(TOUCH_XM, LOW);
  pinMode(TOUCH_YP, INPUT);  pinMode(TOUCH_YM, INPUT);
  pinMode(TOUCH_XP, INPUT_PULLUP); delayMicroseconds(50);
  Serial.println(digitalRead(TOUCH_XP) == LOW ? F("LOW (contato!)") : F("HIGH (sem contato)"));

  // Restaura
  pinMode(TOUCH_YP, OUTPUT); pinMode(TOUCH_YM, OUTPUT);
  pinMode(TOUCH_XP, OUTPUT); pinMode(TOUCH_XM, OUTPUT);
  Serial.println(F("----------------------------"));
}

void drawEstado(bool tocado) {
  tft.fillScreen(BLACK);
  tft.setTextSize(3);

  if (tocado) {
    tft.fillScreen(GREEN);
    tft.setTextColor(BLACK);
    tft.setCursor(80, 80);
    tft.println(F("TOCADO!"));
    tft.setTextSize(2);
    tft.setCursor(60, 130);
    tft.print(F("Contagem: ")); tft.println(contadorToque);
  } else {
    tft.setTextColor(WHITE);
    tft.setCursor(30, 80);
    tft.println(F("Toque a tela..."));
    tft.setTextSize(2);
    tft.setTextColor(YELLOW);
    tft.setCursor(20, 140);
    tft.println(F("Serial: 115200 baud"));
    if (contadorToque > 0) {
      tft.setCursor(20, 170);
      tft.print(F("Ultimo toque: #")); tft.println(contadorToque);
    }
  }
}
