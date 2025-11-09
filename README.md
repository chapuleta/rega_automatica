# Sistema de Irrigação Automática com ESP32

Sistema inteligente de controle de irrigação automática baseado em ESP32 com interface web responsiva.

## 🌱 Funcionalidades

- **Irrigação Automática**: Controle baseado em set point de umidade do solo
- **Interface Web Responsiva**: Controle via navegador em smartphone, tablet ou PC
- **Atualização OTA**: Atualize o firmware remotamente sem cabo USB
- **Monitoramento em Tempo Real**: Visualize umidade do solo e contagem de irrigações
- **Histórico de Irrigações**: Contabiliza irrigações automáticas nas últimas 24h
- **Persistência de Dados**: Mantém configurações e histórico mesmo após reinicialização
- **Reset Remoto**: Reinicie o sistema via interface web
- **Filtro de Leituras**: Média móvel com rejeição de outliers para leituras estáveis

## 🔧 Hardware Necessário

- **ESP32 DevKit V1**
- **Sensor de Umidade do Solo HD-38** (capacitivo)
- **Módulo Relé 5V** (para controle da bomba)
- **Bomba d'água** (compatível com relé)
- **Fonte de alimentação adequada**

## 📌 Conexões

```
HD-38 Sensor:
  VCC  → ESP32 3.3V
  GND  → ESP32 GND
  AO   → ESP32 GPIO 32

Relé:
  VCC  → ESP32 5V (ou fonte externa)
  GND  → ESP32 GND
  IN   → ESP32 GPIO 33

LED Indicador:
  GPIO 2 (built-in LED)
```

## ⚙️ Configuração

1. **Configure o WiFi** em `src/main.cpp`:
   ```cpp
   const char* ssid = "SEU_WIFI";
   const char* password = "SUA_SENHA";
   ```

2. **Compile e faça upload** via PlatformIO:
   ```bash
   pio run -t upload
   ```

3. **Monitore o Serial** para obter o IP do ESP32:
   ```bash
   pio device monitor
   ```

4. **Acesse a interface web**: `http://<IP_DO_ESP32>/`

## 🌐 Interface Web

A interface permite:
- Visualizar umidade atual do solo em tempo real
- Configurar set point de umidade (%)
- Definir tempo de irrigação (ms)
- Ajustar intervalo entre leituras (ms)
- Configurar tempo de espera entre irrigações (ms)
- Acionar irrigação manual
- Habilitar/desabilitar irrigação automática
- Ver contagem de irrigações nas últimas 24h
- Ver tempo restante para próxima irrigação
- Atualizar firmware via OTA (`/update`)
- Reiniciar o sistema remotamente

## 📚 Bibliotecas Utilizadas

- `ESPAsyncWebServer` - Servidor web assíncrono
- `ElegantOTA` - Atualização OTA via navegador
- `Preferences` - Armazenamento persistente na flash
- `WiFi` - Conexão WiFi
- `time.h` - Sincronização de horário via NTP

## 🚀 Recursos Avançados

### Média Móvel com Filtro de Outliers
O sistema usa um filtro de média móvel (10 leituras) para suavizar variações do sensor e rejeita valores discrepantes, garantindo leituras estáveis e confiáveis.

### Persistência de Dados
- Configurações de irrigação
- Tempo da última irrigação (mantido após reset)
- Contagem de irrigações por dia
- Sincronização automática com servidores NTP

### Design Responsivo
Interface otimizada para dispositivos móveis com botões grandes e campos de fácil toque.

## 📝 Licença

Este projeto é de código aberto. Sinta-se livre para usar e modificar.

## 👤 Autor

Desenvolvido com ❤️ para automação residencial
