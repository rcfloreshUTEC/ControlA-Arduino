#include <Wire.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Definir el servidor donde se encuentra la API
const char* serverName = "http://192.168.68.107:8000/api/check_student/";

// Definir API Key
const char* apiKey = "662f1d11-6daa-42bd-9e70-c4114bacf43e";



// Inicialización del PN532 en I2C
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);

// Inicialización del display 1602A en I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  

// Pines del buzzer
#define BUZZER_PIN 15  // Pin conectado a I/O del buzzer (D15 en ESP32)

// Configuración de WiFi
const char* ssid = "Marvel";
const char* password = "DrStrange";

// Nombre del Aula
String aula = "BJ-403";

void setup_wifi() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado a WiFi");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFallo en la conexión WiFi");
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("WiFi no disponible");
    while (true);  // Detener el programa aquí si no se conecta a WiFi
  }
}

int enviarDatos(String carnet, String aula) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", apiKey);  // Agregar la API Key en el encabezado

    // Crear la carga útil (payload)
    String jsonPayload = "{\"aula\":\"" + aula + "\", \"carnet\":\"" + carnet + "\"}";
    
    // Imprimir el payload JSON en la consola
    Serial.println("Enviando datos a la API:");
    Serial.println(jsonPayload);

    // Hacer una solicitud POST
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println("Respuesta de la API:");
      Serial.println(response);

      // Analizar la respuesta JSON para verificar el valor de RSMDB
      StaticJsonDocument<512> doc;  // Ajusta el tamaño según la respuesta esperada
      DeserializationError error = deserializeJson(doc, response);

      if (!error) {
        bool rsmdb = doc["RSMDB"];
        
        // Mostrar carnet en el formato deseado
        lcd.clear();
        lcd.print(carnet);
        lcd.setCursor(0, 1);

        // Si RSMDB es true, mostrar "Registro OK"; si es false, mostrar "Error de registro"
        if (rsmdb) {
          lcd.print("Registro OK");
          Serial.println("Registro OK");

          // Emitir dos pitidos cortos para éxito
          for (int i = 0; i < 2; i++) {
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
          }
        } else {
          lcd.print("Error de registro");
          Serial.println("Error de registro");

          // Emitir tres pitidos cortos para error en el registro
          for (int i = 0; i < 3; i++) {
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
          }
        }
      } else {
        Serial.println("Error al analizar la respuesta JSON");
      }
    } else {
      Serial.print("Error en la solicitud: ");
      Serial.println(httpResponseCode);
    }

    // Cerrar la conexión
    http.end();
    return httpResponseCode;
  } else {
    Serial.println("Error: No conectado a WiFi");
    return -1;  // Código especial para indicar fallo de conexión
  }
}

void setup(void) {
  Serial.begin(115200);
  
  // Configuración de WiFi
  setup_wifi();
    
  // Inicializar el módulo PN532
  nfc.begin();
  
  // Inicializar el display 1602A
  lcd.init();
  lcd.backlight();
  
  Serial.println("PN532 listo para leer tarjetas en I2C.");

  // Configurar el pin del buzzer como salida
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); 
}

void loop(void) {
  static unsigned long lastErrorSoundTime = 0;
  const unsigned long errorSoundInterval = 1800000; // cada treinta minutos.
  
  lcd.clear();
  lcd.print("Esperando NFC...");
  Serial.println("Esperando una tarjeta NFC...");

  if (nfc.tagPresent()) {
    NfcTag tag = nfc.read();
    tag.print();
    
    if (tag.hasNdefMessage()) {
      NdefMessage message = tag.getNdefMessage();
      for (int i = 0; i < message.getRecordCount(); i++) {
        NdefRecord record = message.getRecord(i);

        // Leer el payload del registro (esto es el texto en bruto)
        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);

        // El texto real empieza después del encabezado de 3 bytes
        String carnet = "";
        for (int j = 3; j < payloadLength; j++) {
          carnet += (char)payload[j];
        }

        if (carnet.length() > 0) {
          // Enviar datos a la API y obtener el código de respuesta
          int httpResponseCode = enviarDatos(carnet, aula);
        } else {
          // Si carnet está vacío, mostrar un mensaje de error
          lcd.clear();
          lcd.print("Error: Codigo");
          lcd.setCursor(0, 1);
          lcd.print("invalido");

          Serial.println("Error: Codigo de estudiante vacio.");
          
          // Emitir tres pitidos cortos para error
          for (int i = 0; i < 3; i++) {
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
          }
        }
        delay(2000);
      }
    }
  } else {
    lcd.clear();
    lcd.print("Error al leer");

    // Emitir sonido de error solo si ha pasado el intervalo de tiempo
    unsigned long currentMillis = millis();
    if (currentMillis - lastErrorSoundTime >= errorSoundInterval) {
      Serial.println("No se detectó ninguna tarjeta.");
      for (int i = 0; i < 3; i++) {
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
      }
      lastErrorSoundTime = currentMillis;
    }
  }

  delay(1000);  // Tiempo de espera para el siguiente ciclo
}