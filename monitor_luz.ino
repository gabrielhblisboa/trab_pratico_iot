#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const int LDR_PIN = 34;
const int LIGHT_THRESHOLD = 3000;

WebServer server(80);

// Variáveis de estado
bool lightDetected = false;
unsigned long lastTriggerTime = 0;
int sensorValue = 0;
bool wifiConnected = false;
IPAddress localIP;

void setup() {
  Serial.begin(115200);
  delay(3000);  // Delay para inicialização
  
  Serial.println("\n=== Iniciando Monitor de Luz ESP32 ===");
  
  // Configurar LED interno (GPIO2)
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  
  // Piscar LED 3 vezes rápido para indicar início
  for (int i = 0; i < 3; i++) {
    digitalWrite(2, HIGH);
    delay(200);
    digitalWrite(2, LOW);
    delay(200);
  }
  
  // Teste inicial do sensor
  pinMode(LDR_PIN, INPUT);
  sensorValue = analogRead(LDR_PIN);
  
  Serial.print("Leitura inicial do sensor: ");
  Serial.println(sensorValue);
  
  // Conectar ao Wi-Fi com timeout
  Serial.print("Conectando ao WiFi ");
  WiFi.begin(ssid, password);
  
  unsigned long wifiStartTime = millis();
  bool wifiTimeout = false;
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(2, !digitalRead(2));  // Piscar LED durante tentativa
    
    delay(500);
    
    // Timeout após 30 segundos
    if (millis() - wifiStartTime > 30000) {
      wifiTimeout = true;
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    localIP = WiFi.localIP();
    Serial.println("\nConectado ao WiFi!");
    Serial.print("Endereço IP: ");
    Serial.println(localIP);
    
    // Piscar IP no LED
    blinkIP(localIP);
  } else {
    Serial.println("\nFALHA na conexão WiFi!");
    if (wifiTimeout) Serial.println("Timeout excedido (30s)");
    Serial.println("Verifique SSID e senha");
    
    // Piscar erro no LED (3 longas)
    for (int i = 0; i < 3; i++) {
      digitalWrite(2, HIGH);
      delay(1000);
      digitalWrite(2, LOW);
      delay(500);
    }
  }
  
  // Configurar rotas do servidor web
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/ip", [](){
    server.send(200, "text/plain", wifiConnected ? localIP.toString() : "Não conectado");
  });
  
  server.begin();
  Serial.println("Servidor HTTP iniciado!");
}

void loop() {
  server.handleClient();
  
  // Ler sensor a cada 500ms
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 500) {
    int rawValue = analogRead(LDR_PIN);
    
    // Se o valor for 0 ou 4095, provável erro de conexão
    if (rawValue == 0) {
      Serial.println("ERRO: Valor inválido do sensor! Verifique as conexões.");
    }
    
    sensorValue = rawValue;  // Usaremos valor bruto para simplificar
    
    // Detecção de luz
    bool newDetection = (sensorValue > LIGHT_THRESHOLD);
    
    if (newDetection && !lightDetected) {
      lightDetected = true;
      lastTriggerTime = millis();
      Serial.print("Alerta: Luz detectada! Valor: ");
      Serial.println(sensorValue);
    } else if (!newDetection && lightDetected) {
      lightDetected = false;
      Serial.print("Estado normal. Valor: ");
      Serial.println(sensorValue);
    }
    
    lastRead = millis();
  }
  
  // Piscar LED lentamente quando conectado
  static unsigned long lastBlink = 0;
  if (wifiConnected && millis() - lastBlink > 2000) {
    digitalWrite(2, !digitalRead(2));
    lastBlink = millis();
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Monitor de Luz</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; margin: 40px; }";
  html += ".alert { color: red; font-weight: bold; font-size: 24px; }";
  html += ".normal { color: green; font-size: 24px; }";
  html += ".debug { background: #f0f0f0; padding: 15px; margin: 20px auto; width: 80%; text-align: left; }";
  html += ".error { color: white; background: red; padding: 10px; }";
  html += "</style></head><body>";
  html += "<h1>Monitor de Luz ESP32</h1>";
  
  // Estado atual
  if (sensorValue == 0) {
    html += "<p class='error'>ERRO: VALOR INVÁLIDO DO SENSOR! VERIFIQUE AS CONEXÕES</p>";
  }
  
  if (lightDetected) {
    html += "<p class='alert'>⚠️ ALERTA: LUZ DETECTADA!</p>";
  } else {
    html += "<p class='normal'>Estado Normal</p>";
  }
  
  html += "<p>Valor do sensor: " + String(sensorValue) + "</p>";
  
  // Informações de conexão
  html += "<div class='debug'>";
  html += "<h3>Informações de Rede</h3>";
  
  if (wifiConnected) {
    html += "<p>Conectado ao WiFi: " + String(ssid) + "</p>";
    html += "<p>Endereço IP: <a href='http://" + localIP.toString() + "'>" + localIP.toString() + "</a></p>";
    html += "<p>MAC: " + WiFi.macAddress() + "</p>";
  } else {
    html += "<p class='error'>NÃO CONECTADO AO WIFI!</p>";
    html += "<p>Verifique as configurações de rede</p>";
  }
  
  html += "<h3>Informações do Sensor</h3>";
  html += "<p>Pino: GPIO" + String(LDR_PIN) + "</p>";
  html += "<p>Limiar: " + String(LIGHT_THRESHOLD) + "</p>";
  html += "<p>Última leitura: " + String(millis()) + " ms</p>";
  
  html += "<p><a href='/status'>Status Completo (JSON)</a></p>";
  html += "<p><a href='/ip'>Obter IP</a></p>";
  html += "</div>";
  
  html += "<script>setTimeout(function(){ location.reload(); }, 3000);</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"status\":\"" + String(lightDetected ? "ALERTA" : "NORMAL") + "\",";
  json += "\"sensor_value\":" + String(sensorValue) + ",";
  json += "\"sensor_error\":" + String((sensorValue == 0 || sensorValue == 4095) ? "true" : "false") + ",";
  json += "\"wifi_connected\":" + String(wifiConnected ? "true" : "false") + ",";
  
  if (wifiConnected) {
    json += "\"ip_address\":\"" + localIP.toString() + "\",";
    json += "\"mac_address\":\"" + WiFi.macAddress() + "\",";
  }
  
  json += "\"free_heap\":" + String(esp_get_free_heap_size()) + ",";
  json += "\"uptime_ms\":" + String(millis());
  json += "}";
  
  server.send(200, "application/json", json);
}

// Função para piscar o IP no LED
void blinkIP(IPAddress ip) {
  // Pausa inicial
  delay(2000);
  
  for (int octet = 0; octet < 4; octet++) {
    // Piscar cada dígito do octeto
    int num = ip[octet];
    
    // Piscar para cada dígito (centena, dezena, unidade)
    for (int digit = 0; digit < 3; digit++) {
      int value = num % 10;
      num /= 10;
      
      // Piscar o valor
      for (int i = 0; i < value; i++) {
        digitalWrite(2, HIGH);
        delay(300);
        digitalWrite(2, LOW);
        delay(300);
      }
      
      // Pausa entre dígitos
      delay(1000);
    }
    
    // Pausa longa entre octetos
    delay(3000);
  }
}
