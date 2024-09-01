#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "wifi_credentials.h"

TFT_eSPI tft = TFT_eSPI();

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define FONT_SIZE 2

// Usamos las constantes de color predefinidas
#define TERMINAL_COLOR TFT_GREEN
#define BACKGROUND_COLOR TFT_BLACK

int x, y, z;
int currentScreen = 0; // 0: Splash, 1: Selector, 2: Precio
String selectedCrypto = "";
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 300000; // 5 minutos en milisegundos

const String bitcoinURL = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";
const String ethereumURL = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd";

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a la red WiFi.");
}

String getCryptoPrice(String url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      String cryptoId = url.indexOf("bitcoin") != -1 ? "bitcoin" : "ethereum";
      float price = doc[cryptoId]["usd"];
      return String(price, 2);
    } else {
      Serial.println("Error en la solicitud HTTP");
      return "Error";
    }
    http.end();
  } else {
    Serial.println("Error de conexión WiFi");
    return "No WiFi";
  }
}

void drawSplashScreen() {
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(TERMINAL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("VT220 Terminal");
  tft.println("---------------");
  tft.println("");
  tft.println("Loading...");
  tft.println("");
  tft.setTextSize(3);
  tft.setCursor(20, SCREEN_HEIGHT / 2 - 30);
  tft.println("@LaCompulab");
  delay(3000);
  currentScreen = 1;
}

void drawSelectorScreen() {
  static bool initialized = false;
  
  if (!initialized) {
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setTextColor(TERMINAL_COLOR);
    
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Select Crypto:");
    
    // Botón Bitcoin
    tft.drawRect(20, 50, 200, 60, TERMINAL_COLOR);
    tft.setCursor(80, 70);
    tft.println("Bitcoin");

    // Botón Ethereum
    tft.drawRect(20, 130, 200, 60, TERMINAL_COLOR);
    tft.setCursor(70, 150);
    tft.println("Ethereum");
    
    initialized = true;
  }
}

void drawPriceScreen(String crypto, String price) {
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(TERMINAL_COLOR);
  
  // Nombre de la criptomoneda
  tft.setTextSize(3);
  tft.setCursor(10, 20);
  tft.println(crypto);
  
  // Precio
  tft.setTextSize(4);
  String priceDisplay = price + "$";
  int textWidth = priceDisplay.length() * 24; // Estimación aproximada del ancho del texto
  int x = (SCREEN_WIDTH - textWidth) / 2;
  int y = SCREEN_HEIGHT / 2 - 16; // 16 es aproximadamente la mitad de la altura del texto
  tft.setCursor(x, y);
  tft.println(priceDisplay);
  
  updateCountdown();
}

void updateCountdown() {
  unsigned long elapsedTime = millis() - lastUpdate;
  unsigned long remainingTime = updateInterval - elapsedTime;
  int minutes = remainingTime / 60000;
  int seconds = (remainingTime % 60000) / 1000;
  
  tft.fillRect(0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30, BACKGROUND_COLOR);
  tft.setTextColor(TERMINAL_COLOR);
  tft.setTextSize(2);
  tft.setCursor(10, SCREEN_HEIGHT - 30);
  tft.printf("Actualiza: %02d:%02d", minutes, seconds);
}

void checkTouch() {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = map(p.y, 240, 3800, 0, SCREEN_WIDTH);
    y = map(p.x, 200, 3700, SCREEN_HEIGHT, 0);
    
    if (currentScreen == 1) { // Pantalla de selección
      if (x > 20 && x < 220 && y > 50 && y < 110) {
        selectedCrypto = "Bitcoin";
        currentScreen = 2;
      } else if (x > 20 && x < 220 && y > 130 && y < 190) {
        selectedCrypto = "Ethereum";
        currentScreen = 2;
      }
      
      if (currentScreen == 2) {
        String price = getCryptoPrice(selectedCrypto == "Bitcoin" ? bitcoinURL : ethereumURL);
        drawPriceScreen(selectedCrypto, price);
        lastUpdate = millis();
      }
    }
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(3);

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(true);
  
  connectToWiFi();
  drawSplashScreen();
}

void loop() {
  if (currentScreen == 0) {
    drawSplashScreen();
  } else if (currentScreen == 1) {
    drawSelectorScreen();
  } else if (currentScreen == 2) {
    if (millis() - lastUpdate >= updateInterval) {
      String price = getCryptoPrice(selectedCrypto == "Bitcoin" ? bitcoinURL : ethereumURL);
      drawPriceScreen(selectedCrypto, price);
      lastUpdate = millis();
    } else {
      updateCountdown();
    }
  }
  
  checkTouch();
  delay(100);
}