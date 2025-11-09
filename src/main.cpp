#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h> // Adicione esta linha
#include <Preferences.h>
#include <time.h>
#include <esp_system.h> // Adicione no início do arquivo

// Configurações de Wi-Fi
const char* ssid = "ILZAMAGDA"; // Substitua pelo nome da sua rede Wi-Fi
const char* password = "inventaumaai"; // Substitua pela senha da sua rede Wi-Fi

// Pinos e constantes
const int SENSOR_PIN = 32;  // Sensor de umidade de solo no pino D32
const int RELAY_PIN = 33;   // Pino de controle do relé ou transistor
const int MAX_ANALOG_VALUE = 4095; // Valor máximo do ADC do ESP32

// Parâmetros de irrigação
int set_point = 18;   // Umidade ideal (%)
unsigned long irrigate_time = 300;  // Tempo de irrigação (ms)
unsigned long wait_time = 300;  // Intervalo entre leituras (ms)
unsigned long irrigation_wait_time = 1080000;  // Tempo de espera após irrigação (15 minutos)

// Variáveis de controle
unsigned long last_irrigation_time = 0;
bool automatic_irrigation = true;

// Variáveis de leitura
int moisture = 0;
int sensor_analog = 0;

// Filtro de média móvel
const int NUM_READINGS = 20; // Número de leituras para média (aumentado para mais estabilidade)
int readings[NUM_READINGS];  // Array para armazenar leituras
int readIndex = 0;           // Índice atual no array
long total = 0;              // Soma das leituras (long para evitar overflow)
int average = 0;             // Média das leituras

// Servidor Web
AsyncWebServer server(80);

Preferences preferences;
int pump_count_24h = 0;
int last_saved_day = -1;

// Função para conectar ao Wi-Fi
void connectToWiFi() {
  // Configuração de IP estático (opcional - comentar para usar DHCP)
  IPAddress local_IP(192, 168, 18, 100);      // IP fixo desejado
  IPAddress gateway(192, 168, 18, 1);         // Gateway do roteador
  IPAddress subnet(255, 255, 255, 0);         // Máscara de sub-rede
  IPAddress primaryDNS(8, 8, 8, 8);           // DNS primário (Google)
  IPAddress secondaryDNS(8, 8, 4, 4);         // DNS secundário (Google)

  // Configura IP estático antes de conectar
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Falha ao configurar IP estático");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }
  Serial.println("Conectado ao Wi-Fi!");
  Serial.print("IP do ESP32: ");
  Serial.println(WiFi.localIP());
}

// Função para ler a umidade do solo
int readSoilMoisture() {
  // Lê o valor bruto do sensor com pequeno delay para estabilização
  delay(10);
  int raw_value = analogRead(SENSOR_PIN);
  
  // Remove a leitura mais antiga da soma
  total = total - readings[readIndex];
  
  // Adiciona a nova leitura
  readings[readIndex] = raw_value;
  total = total + readings[readIndex];
  
  // Avança o índice (circular)
  readIndex = (readIndex + 1) % NUM_READINGS;
  
  // Calcula a média
  average = total / NUM_READINGS;
  
  // Rejeita valores muito discrepantes - threshold mais rigoroso
  int threshold = 50; // Diferença máxima permitida da média
  if (abs(raw_value - average) > threshold) {
    // Se a leitura estiver muito fora, ignora e mantém a média
    raw_value = average;
  }
  
  // Converte para percentual de umidade
  sensor_analog = average;
  return 100 - ((average / (float)MAX_ANALOG_VALUE) * 100);
}

// Função para salvar a contagem de ativações da bomba
void savePumpCount();
void saveLastIrrigationTime();

// Função para ativar a bomba
void activatePump(unsigned long duration) {
  digitalWrite(RELAY_PIN, HIGH);
  delay(duration);
  digitalWrite(RELAY_PIN, LOW);
}

// Função para configurar as rotas do servidor
void setupServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<style>";
    html += "body{font-family:sans-serif;max-width:480px;margin:auto;padding:10px;background:#f7f7f7;}";
    html += "h1{font-size:1.5em;text-align:center;}";
    html += "form,button,a,p{margin-bottom:12px;}";
    html += "input[type=number]{width:100%;padding:6px;margin:4px 0 8px 0;box-sizing:border-box;}";
    html += "button{width:100%;padding:10px;font-size:1em;background:#2196F3;color:white;border:none;border-radius:4px;}";
    html += "button:hover{background:#1976D2;}";
    html += "a{display:block;text-align:center;color:#2196F3;text-decoration:none;margin:8px 0;}";
    html += "@media (max-width:600px){body{padding:2vw;} h1{font-size:1.2em;}}";
    html += "</style>";
    html += "</head><body>";
    html += "<h1>Controle de Irrigação</h1>";
    html += "<p>Umidade atual: <b id='moistureVal'>" + String(moisture) + "%</b></p>";
    html += "<p>Tempo para próxima irrigação: <b id='nextIrrigation'>--:--</b></p>";
    html += "<form id='updateForm'>";
    html += "Tempo de Irrigação (ms):<input type='number' id='irrigate_time' value='" + String(irrigate_time) + "'>";
    html += "Set Point de Umidade (%):<input type='number' id='set_point' value='" + String(set_point) + "'>";
    html += "Intervalo entre Leituras (ms):<input type='number' id='wait_time' value='" + String(wait_time) + "'>";
    html += "Tempo de Espera Após Irrigação (ms):<input type='number' id='irrigation_wait_time' value='" + String(irrigation_wait_time) + "'>";
    html += "<button type='button' onclick='updateParams()'>Atualizar</button>";
    html += "</form>";
    html += "<button type='button' onclick='manualIrrigation()'>Acionar Irrigação Manual</button>";
    html += "<button type='button' id='autoBtn' onclick='toggleAutoIrrigation()'></button>";
    html += "<a href=\"/update\">Atualizar Firmware (OTA)</a>";
    html += "<p>Pino da bomba (relé): <b>" + String(RELAY_PIN) + "</b></p>";
    html += "<p>Pino do sensor de umidade: <b>" + String(SENSOR_PIN) + "</b></p>";
    html += "<p>Bomba acionada nas últimas 24h: <b>" + String(pump_count_24h) + " vezes</b></p>";
    html += "<button type='button' onclick='resetSystem()' style='background:#e53935;'>Reiniciar Sistema</button>";
    html += "<script>";
    html += "function updateParams() {";
    html += "  var irrigateTime = document.getElementById('irrigate_time').value;";
    html += "  var setPoint = document.getElementById('set_point').value;";
    html += "  var waitTime = document.getElementById('wait_time').value;";
    html += "  var irrigationWaitTime = document.getElementById('irrigation_wait_time').value;";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/set?irrigate_time=' + irrigateTime + '&set_point=' + setPoint + '&wait_time=' + waitTime + '&irrigation_wait_time=' + irrigationWaitTime, true);";
    html += "  xhr.onreadystatechange = function() {";
    html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
    html += "      alert('Parâmetros Atualizados!');";
    html += "    }";
    html += "  };";
    html += "  xhr.send();";
    html += "}";
    html += "function manualIrrigation() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/irrigate', true);";
    html += "  xhr.onreadystatechange = function() {";
    html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
    html += "      alert('Irrigação Concluída!');";
    html += "    }";
    html += "  };";
    html += "  xhr.send();";
    html += "}";
    html += "function toggleAutoIrrigation() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/toggle_auto', true);";
    html += "  xhr.onreadystatechange = function() {";
    html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
    html += "      var enabled = xhr.responseText.includes('Habilitada');";
    html += "      setAutoBtn(enabled);";
    html += "      alert('Irrigação Automática ' + (enabled ? 'Habilitada' : 'Desabilitada'));";
    html += "    }";
    html += "  };";
    html += "  xhr.send();";
    html += "}";
    html += "function setAutoBtn(enabled) {";
    html += "  var btn = document.getElementById('autoBtn');";
    html += "  if (enabled) {";
    html += "    btn.textContent = 'Desabilitar Irrigação Automática';";
    html += "    btn.style.background = '#2196F3';";
    html += "    btn.style.color = 'white';";
    html += "  } else {";
    html += "    btn.textContent = 'Habilitar Irrigação Automática';";
    html += "    btn.style.background = '#FFEB3B';";
    html += "    btn.style.color = '#333';";
    html += "  }";
    html += "}";
    html += "window.onload = function() { setAutoBtn(" + String(automatic_irrigation ? "true" : "false") + "); };";
    html += "setInterval(function() {";
    html += "  var xhr = new XMLHttpRequest();";
    html += "  xhr.open('GET', '/next_irrigation', true);";
    html += "  xhr.onreadystatechange = function() {";
    html += "    if (xhr.readyState == 4 && xhr.status == 200) {";
    html += "      var secs = parseInt(xhr.responseText);";
    html += "      var min = Math.floor(secs/60);";
    html += "      var s = secs%60;";
    html += "      document.getElementById('nextIrrigation').textContent = (min<10?'0':'')+min+':' + (s<10?'0':'')+s;";
    html += "    }";
    html += "  };";
    html += "  xhr.send();";
    html += "}, 2000);";
    html += "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("irrigate_time")) {
      irrigate_time = request->getParam("irrigate_time")->value().toInt();
    }
    if (request->hasParam("set_point")) {
      set_point = request->getParam("set_point")->value().toInt();
    }
    if (request->hasParam("wait_time")) {
      wait_time = request->getParam("wait_time")->value().toInt();
    }
    if (request->hasParam("irrigation_wait_time")) {
      irrigation_wait_time = request->getParam("irrigation_wait_time")->value().toInt();
    }
    request->send(200, "text/plain", "Parâmetros Atualizados");
  });

  server.on("/irrigate", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Irrigação Manual Iniciada...");
    activatePump(irrigate_time);
    last_irrigation_time = millis(); // Atualiza o tempo para agora
    saveLastIrrigationTime();        // Salva na flash
    request->send(200, "text/plain", "Irrigação Concluída");
  });

  server.on("/toggle_auto", HTTP_GET, [](AsyncWebServerRequest *request) {
    automatic_irrigation = !automatic_irrigation;
    String status = automatic_irrigation ? "Habilitada" : "Desabilitada";
    request->send(200, "text/plain", "Irrigação Automática " + status);
  });

  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Reiniciando ESP...");
    delay(200); // Dá tempo de enviar a resposta
    ESP.restart();
  });

  server.on("/moisture", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(moisture));
  });

  server.on("/next_irrigation", HTTP_GET, [](AsyncWebServerRequest *request) {
    unsigned long now = millis();
    long time_left = (long)irrigation_wait_time - (long)(now - last_irrigation_time);
    if (!automatic_irrigation || moisture >= set_point) time_left = 0;
    if (time_left < 0) time_left = 0;
    request->send(200, "text/plain", String(time_left / 1000)); // em segundos
  });

  server.begin();
  ElegantOTA.begin(&server); // Adicione esta linha
}

// Função para sincronizar o horário com NTP
void syncTime() {
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC-3 para Brasil
  Serial.print("Aguardando sincronização NTP...");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("OK");
}

void loadPumpCount() {
  preferences.begin("rega", false);
  pump_count_24h = preferences.getInt("pump_count", 0);
  last_saved_day = preferences.getInt("last_day", -1);
  preferences.end();
}

void savePumpCount() {
  preferences.begin("rega", false);
  preferences.putInt("pump_count", pump_count_24h);
  preferences.putInt("last_day", last_saved_day);
  preferences.end();
}

void saveLastIrrigationTime() {
    preferences.begin("rega", false);
    preferences.putULong("last_irrigation_time", last_irrigation_time);
    preferences.putULong("last_millis", millis());
    preferences.end();
}

// Configuração inicial
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(2, OUTPUT);           // LED no pino 2 como saída
  digitalWrite(2, HIGH);        // Mantém o LED aceso enquanto o sistema está ligado

  // Inicializa o array de leituras do sensor com primeira leitura
  int initial_reading = analogRead(SENSOR_PIN);
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = initial_reading;
  }
  total = initial_reading * NUM_READINGS;
  average = initial_reading;

  connectToWiFi();
  syncTime();
  loadPumpCount();
  preferences.begin("rega", false);
  unsigned long saved_last_irrigation_time = preferences.getULong("last_irrigation_time", 0);
  unsigned long saved_last_millis = preferences.getULong("last_millis", 0);
  preferences.end();

  // Ajusta last_irrigation_time para o novo millis()
  last_irrigation_time = saved_last_irrigation_time + (millis() - saved_last_millis);

  setupServer();
}

// Loop principal
void loop() {
  unsigned long current_time = millis();

  // Checa se o dia virou
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  int today = timeinfo.tm_yday;
  if (last_saved_day != today) {
    pump_count_24h = 0;
    last_saved_day = today;
    savePumpCount();
  }

  // Verifica se é hora de ler o sensor
  static unsigned long last_read_time = 0;
  if (current_time - last_read_time >= wait_time) {
    last_read_time = current_time;
    moisture = readSoilMoisture();
    Serial.print("Umidade do Solo: ");
    Serial.print(moisture);
    Serial.println("%");
  }

  // Verifica se é necessário irrigar automaticamente
  if (automatic_irrigation && moisture < set_point && (current_time - last_irrigation_time) > irrigation_wait_time) {
    Serial.println("Solo seco. Iniciando irrigação automática...");
    activatePump(irrigate_time);
    last_irrigation_time = current_time;
    saveLastIrrigationTime(); // Salva o tempo da última irrigação

    // Incrementa e salva apenas para irrigação automática
    pump_count_24h++;
    savePumpCount();
  }
}
