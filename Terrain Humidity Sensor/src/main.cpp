#include <Wire.h>
#include <PCF8574.h>
#include <DHT.h>
#include <Arduino.h>
#include <WiFiNINA.h>
#include <WiFiClient.h>

// --------- KONFIGURACJA ---------
#define DHTTYPE     DHT11
#define DHTPIN      12
#define SENSORH_PIN 13
#define LDR_PIN     A1
#define SENSOR_PIN  A0
const float R_REF = 9770.0;
const float vcc   = 5.0;

// Wi-Fi
const char* ssid     = "Domoticz";
const char* password = "Domoticz";

// Domoticz
const char* serverUrl    = "192.168.1.140";
const uint16_t serverPort = 8080;

// IDX urządzeń
const int IDX_TEMP_HUMID = 1;
const int IDX_LUX        = 2;
const int IDX_SOIL       = 3;
const int IDX_DOOR       = 4;
const int IDX_KEY        = 5;

// Klawiatura 4×4
typedef char KeyMap[4][4];
KeyMap keys = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[4] = {0,1,2,3};
byte colPins[4] = {4,5,6,7};
const char correctCode[] = "676A2C";
char enteredCode[7] = "";
byte codeIndex = 0;

// Obiekty
PCF8574 pcf(0x20);
DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;

char getKeypadKey() {
  static unsigned long lastPress = 0;
  for (byte c = 0; c < 4; c++) {
    for (byte i = 0; i < 4; i++) pcf.write(colPins[i], HIGH);
    pcf.write(colPins[c], LOW);
    for (byte r = 0; r < 4; r++) {
      if (pcf.read(rowPins[r]) == LOW && millis() - lastPress > 200) {
        lastPress = millis();
        return keys[r][c];
      }
    }
  }
  return 0;
}

String urlEncode(const String& str) {
  String encoded = "";
  char c;
  char code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      code0 = ((c >> 4) & 0xf) + '0';
      if (((c >> 4) & 0xf) > 9) code0 = ((c >> 4) & 0xf) - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}

// ✅ Poprawiona funkcja HTTP z prawidłowym nagłówkiem Authorization
void sendToDomoticzHTTP(int idx, const String& svalue) {

  String encodedValue = urlEncode(svalue);

  String path = "/json.htm?type=command&param=udevice";
  path += "&idx=" + String(idx);
  path += "&nvalue=0&svalue=" + encodedValue;;

  if (!wifiClient.connect(serverUrl, serverPort)) {
    Serial.println("! Conn failed");
    return;
  }

  wifiClient.print("GET " + path + " HTTP/1.1\r\n");
  wifiClient.print("Accept-Charset: utf-8\r\n");
  wifiClient.print("Host: "); wifiClient.print(serverUrl); wifiClient.print(":" + String(serverPort) + "\r\n");
  wifiClient.print("Authorization: Basic Y29zbW86Y29zbW8xMjM= \r\n"); // <-- poprawione
  wifiClient.print("Connection: close\r\n");
  wifiClient.print("Accept: */*\r\n");
  wifiClient.print("\r\n");

  delay(200);

  String response = "";
  while (wifiClient.available()) {
    response = wifiClient.readStringUntil('\n');
    Serial.println(response);
  }

  if (response.indexOf("HTTP/1.1 200 OK") == -1) {
    Serial.println("Error: No successful response from Domoticz.");
  } else {
    Serial.println("Data successfully sent to Domoticz.");
  }


    while (wifiClient.available()) {
    String line = wifiClient.readStringUntil('\r');
    Serial.print(line);
  }

  wifiClient.stop();
}

void sendContactToDomoticz(int idx, bool isOpen) {
  String path = "/json.htm?type=command&param=udevice";
  path += "&idx=" + String(idx);
  path += "&nvalue=";
  path += (isOpen ? "0" : "1"); // 1 = Open, 0 = Closed

  if (!wifiClient.connect(serverUrl, serverPort)) {
    Serial.println("! Conn failed");
    return;
  }

  wifiClient.print("GET " + path + " HTTP/1.1\r\n");
  wifiClient.print("Accept-Charset: utf-8\r\n");
  wifiClient.print("Host: "); wifiClient.print(serverUrl); wifiClient.print(":" + String(serverPort) + "\r\n");
  wifiClient.print("Authorization: Basic Y29zbW86Y29zbW8xMjM= \r\n");
  wifiClient.print("Connection: close\r\n");
  wifiClient.print("Accept: */*\r\n");
  wifiClient.print("\r\n");

  delay(200);

  String response = "";
  while (wifiClient.available()) {
    response = wifiClient.readStringUntil('\n');
    Serial.println(response);
  }

  if (response.indexOf("HTTP/1.1 200 OK") == -1) {
    Serial.println("Error: No successful response from Domoticz.");
  } else {
    Serial.println("Contact state successfully sent to Domoticz.");
  }

  wifiClient.stop();
}


void setup() {
  Serial.begin(9600);
  Wire.begin();
  pcf.begin();
  for (byte i = 0; i < 4; i++) pcf.write(colPins[i], HIGH);
  pinMode(SENSORH_PIN, INPUT);
  pinMode(2, INPUT_PULLUP);
  dht.begin();

  Serial.print("Łączenie z WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.print("\nPołączono! IP=");
  Serial.println(WiFi.localIP());
  Serial.println("Wprowadź kod i zatwierdź *");
}

void loop() {
  static unsigned long lastSend = 0;

  if (char key = getKeypadKey()) {
    if (key == '#') {
      if (codeIndex > 0) {
        codeIndex--;
        enteredCode[codeIndex] = '\0';
        sendToDomoticzHTTP(IDX_KEY, String(enteredCode));
        Serial.print("Wprowadzony kod: ");
        Serial.println(enteredCode);
      }
    } else if (key == '*') {
      if (codeIndex == 6) {
        bool ok = (strcmp(enteredCode, correctCode) == 0);
        sendToDomoticzHTTP(IDX_KEY, ok ? "Kod poprawny" : "Kod niepoprawny");
        Serial.print("Wprowadzony kod: ");
        Serial.println(enteredCode);
        if (ok) Serial.println("Kod poprawny.");
        else Serial.println("Kod niepoprawny.");
      } else {
        sendToDomoticzHTTP(IDX_KEY, "Kod za krótki");
        Serial.println("Kod za krótki.");
      }
      codeIndex = 0;
      enteredCode[0] = '\0';
    } else if (codeIndex < 6) {
      enteredCode[codeIndex++] = key;
      enteredCode[codeIndex] = '\0';
      sendToDomoticzHTTP(IDX_KEY, String(enteredCode));
      Serial.print("Wprowadzony kod: ");
      Serial.println(enteredCode);
    }
  }

  if (millis() - lastSend > 10000) {
    lastSend = millis();

    bool isOpen = digitalRead(2) == LOW;
    float temp   = dht.readTemperature();
    float humid  = dht.readHumidity();
    int   v      = analogRead(LDR_PIN);
    float volt = v * vcc / 1023.0;
    float R_photo = R_REF * (vcc / volt - 1.0);
    float pre_lux = 1000.0 / (R_photo / 1000.0);
    float lux = 0.0005 * pow(pre_lux, 2.0);
    int   soilRaw= analogRead(SENSOR_PIN);
    float soil   = map(soilRaw, 1023, 0, 0, 100);

    Serial.print("Temp: "); Serial.println(temp);
    Serial.print("Humidity: "); Serial.println(humid);
    Serial.print("LUX: "); Serial.println(lux);
    Serial.print("Soil: "); Serial.println(soil);
    Serial.print("Door: "); Serial.println(isOpen ? "Closed" : "Open");

    String tempHumid = String(temp, 2) + "°C" + " , " + String(humid, 2) + "%";
    sendToDomoticzHTTP(IDX_TEMP_HUMID, tempHumid);
    sendToDomoticzHTTP(IDX_LUX,  String(lux,  2));
    sendToDomoticzHTTP(IDX_SOIL, String(soil, 2));
    sendContactToDomoticz(IDX_DOOR, isOpen);
  }
}
