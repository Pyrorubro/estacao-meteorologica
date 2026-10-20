
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>

// =====================================
// ESTACAO METEOROLOGICA v0.04
// ESP8266 NodeMCU V3
//
// OLED SSD1306
// BH1750 - Luminosidade
// AHT20  - Temperatura e umidade
// BMP280 - Pressao atmosferica
// =====================================

#define LARGURA 128
#define ALTURA 64
#define OLED_RESET -1

// PINOS TESTADOS E FUNCIONANDO
#define SDA_PIN D1
#define SCL_PIN D2

Adafruit_SSD1306 display(
  LARGURA, ALTURA, &Wire, OLED_RESET
);

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

// Enderecos I2C
uint8_t enderecoOLED = 0;
uint8_t enderecoBH = 0;
uint8_t enderecoBMP = 0;

// Estado dos sensores
bool ahtOK = false;
bool bmpOK = false;
bool bhOK = false;

// Medidas
float temperatura = NAN;
float umidade = NAN;
float pressao = NAN;
float luminosidade = NAN;

unsigned long ultimaLeitura = 0;

// =====================================
// VERIFICAR DISPOSITIVO I2C
// =====================================

bool presente(uint8_t endereco) {
  Wire.beginTransmission(endereco);
  return Wire.endTransmission() == 0;
}

// =====================================
// INICIALIZAR BH1750
// =====================================

bool iniciarBH1750() {

  if (presente(0x23)) {
    enderecoBH = 0x23;
  } else if (presente(0x5C)) {
    enderecoBH = 0x5C;
  } else {
    return false;
  }

  // Power on
  Wire.beginTransmission(enderecoBH);
  Wire.write(0x01);

  if (Wire.endTransmission() != 0) {
    return false;
  }

  // Modo continuo de alta resolucao
  Wire.beginTransmission(enderecoBH);
  Wire.write(0x10);

  return Wire.endTransmission() == 0;
}

// =====================================
// LER BH1750
// =====================================

float lerBH1750() {

  if (!bhOK) {
    return NAN;
  }

  uint8_t recebidos =
    Wire.requestFrom(enderecoBH, (uint8_t)2);

  if (recebidos != 2) {
    return NAN;
  }

  uint16_t bruto = Wire.read() << 8;
  bruto |= Wire.read();

  return bruto / 1.2f;
}

// =====================================
// EXIBIR MEDIDA OU ERRO
// =====================================

void mostrarValor(float valor, uint8_t casas) {

  if (isnan(valor)) {
    display.print("--");
  } else {
    display.print(valor, casas);
  }
}

// =====================================
// ATUALIZAR OLED
// =====================================

void atualizarOLED() {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ESTACAO METEO v0.04");

  display.drawLine(
    0, 11, 127, 11, SSD1306_WHITE
  );

  // Temperatura
  display.setCursor(0, 17);
  display.print("TEMP: ");
  mostrarValor(temperatura, 1);
  display.print(" C");

  // Umidade
  display.setCursor(0, 28);
  display.print("UMID: ");
  mostrarValor(umidade, 1);
  display.print(" %");

  // Pressao
  display.setCursor(0, 39);
  display.print("PRES: ");
  mostrarValor(pressao, 1);
  display.print(" hPa");

  // Luminosidade
  display.setCursor(0, 50);
  display.print("LUZ : ");
  mostrarValor(luminosidade, 0);
  display.print(" lx");

  display.display();
}

// =====================================
// INICIALIZACAO
// =====================================

void setup() {

  Serial.begin(115200);
  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  // OLED - mesma inicializacao
  // das versoes que funcionaram

  if (presente(0x3C)) {
    enderecoOLED = 0x3C;
  } else if (presente(0x3D)) {
    enderecoOLED = 0x3D;
  }

  if (!enderecoOLED) {
    Serial.println("OLED nao encontrado!");
    return;
  }

  if (!display.begin(
      SSD1306_SWITCHCAPVCC,
      enderecoOLED)) {
    Serial.println("Falha no OLED!");
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 5);
  display.println("ESTACAO METEO v0.04");

  display.setCursor(0, 25);
  display.println("Iniciando sensores...");

  display.display();

  // AHT20
  ahtOK = aht.begin(&Wire);

  Serial.print("AHT20: ");
  Serial.println(ahtOK ? "OK" : "FALHA");

  // BMP280
  if (presente(0x76)) {
    enderecoBMP = 0x76;
  } else if (presente(0x77)) {
    enderecoBMP = 0x77;
  }

  if (enderecoBMP) {
    bmpOK = bmp.begin(enderecoBMP);
  }

  Serial.print("BMP280: ");
  Serial.println(bmpOK ? "OK" : "FALHA");

  // BH1750
  bhOK = iniciarBH1750();

  Serial.print("BH1750: ");
  Serial.println(bhOK ? "OK" : "FALHA");

  delay(1000);

  // Primeira leitura imediata
  ultimaLeitura = millis() - 2000;
}

// =====================================
// LOOP PRINCIPAL
// =====================================

void loop() {

  if (millis() - ultimaLeitura >= 2000) {

    ultimaLeitura = millis();

    // AHT20
    if (ahtOK) {

      sensors_event_t eventoUmidade;
      sensors_event_t eventoTemp;

      if (aht.getEvent(
          &eventoUmidade,
          &eventoTemp)) {

        temperatura = eventoTemp.temperature;
        umidade = eventoUmidade.relative_humidity;

      } else {
        temperatura = NAN;
        umidade = NAN;
      }
    }

    // BMP280
    if (bmpOK) {
      pressao = bmp.readPressure() / 100.0f;
    }

    // BH1750
    luminosidade = lerBH1750();

    // Atualizar OLED
    atualizarOLED();

    // Monitor serial
    Serial.println();
    Serial.println("=== ESTACAO v0.04 ===");

    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" C");

    Serial.print("Umidade: ");
    Serial.print(umidade);
    Serial.println(" %");

    Serial.print("Pressao: ");
    Serial.print(pressao);
    Serial.println(" hPa");

    Serial.print("Luminosidade: ");
    Serial.print(luminosidade);
    Serial.println(" lux");
  }

  yield();
}
