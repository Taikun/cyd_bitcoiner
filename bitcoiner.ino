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

#define TERMINAL_COLOR TFT_GREEN
#define BACKGROUND_COLOR TFT_BLACK
#define CHART_COLOR TFT_YELLOW

int x, y, z;
int currentScreen = 0; // 0: Splash, 1: Selector, 2: Precio
String selectedCrypto = "";
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 1800000; // 30 minutos en milisegundos

const String bitcoinURL = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";
const String ethereumURL = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd";

#define MAX_PRICE_HISTORY 24
float priceHistory[MAX_PRICE_HISTORY];
int priceHistoryIndex = 0;

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
  tft.println("v1.1");
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


void calculateMinMaxPrice(float &minPrice, float &maxPrice) {
  minPrice = priceHistory[0];
  maxPrice = priceHistory[0];
  for (int i = 1; i < MAX_PRICE_HISTORY; i++) {
    if (priceHistory[i] < minPrice) minPrice = priceHistory[i];
    if (priceHistory[i] > maxPrice) maxPrice = priceHistory[i];
  }
}


void drawChart() {
  int chartWidth = SCREEN_WIDTH - 40;
  int chartHeight = SCREEN_HEIGHT - 150;  // Dejar espacio para la cotización y la cuenta regresiva
  int chartX = 20;
  int chartY = 100;  // Ajustar para dejar espacio para la cotización de precio

  float minPrice, maxPrice;
  calculateMinMaxPrice(minPrice, maxPrice); // Calcular precios mínimos y máximos

  // Dibujar ejes
  tft.drawLine(chartX, chartY, chartX, chartY + chartHeight, TERMINAL_COLOR);
  tft.drawLine(chartX, chartY + chartHeight, chartX + chartWidth, chartY + chartHeight, TERMINAL_COLOR);

  // Dibujar líneas del gráfico con colores dependiendo de si sube o baja el precio
  for (int i = 0; i < MAX_PRICE_HISTORY - 1; i++) {
    int x1 = chartX + (i * chartWidth / (MAX_PRICE_HISTORY - 1));
    int y1 = chartY + chartHeight - ((priceHistory[i] - minPrice) / (maxPrice - minPrice) * chartHeight);
    int x2 = chartX + ((i + 1) * chartWidth / (MAX_PRICE_HISTORY - 1));
    int y2 = chartY + chartHeight - ((priceHistory[i + 1] - minPrice) / (maxPrice - minPrice) * chartHeight);

    // Dibujar línea en verde si sube, rojo si baja
    if (priceHistory[i + 1] >= priceHistory[i]) {
      tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
    } else {
      tft.drawLine(x1, y1, x2, y2, TFT_RED);
    }
  }

  // No se dibujan etiquetas de los ejes para un gráfico más limpio
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
  int textWidth = priceDisplay.length() * 24;
  int x = (SCREEN_WIDTH - textWidth) / 2;
  int y = 60;  // Ajustar para que quede arriba de la gráfica
  tft.setCursor(x, y);
  tft.println(priceDisplay);
  
  // Añadir el precio al historial
  priceHistory[priceHistoryIndex] = price.toFloat();
  priceHistoryIndex = (priceHistoryIndex + 1) % MAX_PRICE_HISTORY;
  
  // Dibujar la gráfica
  drawChart();
  
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
  
  // Inicializar el historial de precios
  for (int i = 0; i < MAX_PRICE_HISTORY; i++) {
    priceHistory[i] = 0;
  }
  
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