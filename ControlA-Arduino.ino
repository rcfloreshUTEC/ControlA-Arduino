#include <Wire.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

// Definir el servidor donde se encuentra la API
const char* serverName = "http://192.168.68.115:5000/verify";

// Inicialización del PN532 en I2C
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);

// Inicialización del display 1602A en I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Asegúrate de que la dirección I2C es correcta; 0x27 es común

// Pines del buzzer
#define BUZZER_PIN 15  // Pin conectado a I/O del buzzer (D15 en ESP32)

// Configuración de WiFi
const char* ssid = "Marvel";
const char* password = "DrStrange";

// Nombre del Aula
String classroom = "BJ001";

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

void enviarDatos(String codStudent, String classroom) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // Crear la carga útil (payload)
    String jsonPayload = "{\"CodStudent\":\"" + codStudent + "\", \"Classroom\":\"" + classroom + "\"}";

    // Hacer una solicitud POST
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error en la solicitud: ");
      Serial.println(httpResponseCode);
    }

    // Cerrar la conexión
    http.end();
  } else {
    Serial.println("Error: No conectado a WiFi");
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
  digitalWrite(BUZZER_PIN, HIGH);  // Asegurarse de que el buzzer esté apagado al inicio
}

void loop(void) {  
  lcd.clear();  // Limpiar pantalla antes de mostrar el nuevo mensaje
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
        String codstudent = "";
        for (int j = 3; j < payloadLength; j++) {
          codstudent += (char)payload[j];
        }

        if (codstudent.length() > 0) {
          // Mostrar el texto en el display
          lcd.clear();
          lcd.print("Codigo carnet:");
          lcd.setCursor(0, 1);
          lcd.print(codstudent);  // Muestra el texto en la segunda línea
          
          Serial.println("CodStudent: " + codstudent);
          Serial.println("Classroom: " + classroom);
          
          // Enviar datos a la API
          enviarDatos(codstudent, classroom);
          
          // Emitir dos pitidos cortos
          for (int i = 0; i < 2; i++) {
            digitalWrite(BUZZER_PIN, LOW);
            delay(100);
            digitalWrite(BUZZER_PIN, HIGH);
            delay(100);
          }
        } else {
          // Si codstudent está vacío, mostrar un mensaje de error
          lcd.clear();
          lcd.print("Error: Codigo");
          lcd.setCursor(0, 1);
          lcd.print("invalido");

          Serial.println("Error: Codigo de estudiante vacio.");
          
          // Emitir tres pitidos cortos
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
    Serial.println("No se detectó ninguna tarjeta.");
     
    for (int i = 0; i < 3; i++) {
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
    }
  }

  delay(1000);
}
