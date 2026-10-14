#include <Arduino.h>

// =============================================================================
// 1. CONFIGURAÇÃO DOS PINOS (Altere aqui para os pinos do seu projeto)
// =============================================================================
#define SERVO_PWM_PIN    18   // Pino de saída PWM enviado para o Servomotor
#define POT_ADC_PIN       4   // Pino de entrada analógica (Usar ADC1: GPIO 1 a 10 no ESP32-S3)
#define TRIGGER_PIN       5   // Pino digital para Trigger do Osciloscópio (HIGH no t=0)

// =============================================================================
// 2. PARÂMETROS DO ENSAIO E DO SERVOMOTOR
// =============================================================================
// Duração do pulso PWM em microsegundos (µs) - Padrão de Servos RC (50 Hz):
// 1000 µs ≈ 0° (Repouso) | 1500 µs ≈ 45° | 2000 µs ≈ 90°
const uint32_t PULSE_INIT_US = 1000; // Posição de repouso (Início do ciclo)
const uint32_t PULSE_STEP_US = 1600; // Posição final após a aplicação do degrau

// Tempos da simulação
const uint32_t STEP_DURATION_MS  = 2000;  // Duração do degrau ativo (2.0 segundos)
const uint32_t REST_DURATION_MS  = 2000;  // Duração do repouso entre ciclos (2.0 segundos)
const uint32_t SAMPLE_INTERVAL_US = 10000; // Amostragem exata a cada 10ms (10.000 µs)

// Configurações do periférico PWM (LEDC)
const uint32_t PWM_FREQ       = 50;    // Frequência padrão de servos: 50 Hz (Período = 20ms)
const uint8_t  PWM_RESOLUTION = 16;    // Resolução de 16 bits (0 a 65535)

// =============================================================================
// FUNÇÕES AUXILIARES DE CONTROLE
// =============================================================================

// Converte largura de pulso em microsegundos (µs) para o duty cycle do LEDC
void setServoPulseUs(uint32_t pulse_us) {
  // Período de 50 Hz = 20.000 µs
  uint32_t duty = (pulse_us * ((1 << PWM_RESOLUTION) - 1)) / 20000;
  
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(SERVO_PWM_PIN, duty); // ESP32 Arduino Core v3.x
  #else
    ledcWrite(0, duty);             // ESP32 Arduino Core v2.x
  #endif
}

void setup() {
  // Inicialização da Comunicação Serial
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Aguarda conexão USB Serial no ESP32-S3

  // Configuração do Pino de Trigger para o Osciloscópio
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);

  // Configuração do ADC (Leitura do Potenciômetro)
  analogReadResolution(12);             // Resolução de 12 bits (0 a 4095)
  analogSetAttenuation(ADC_11db);       // Faixa de leitura até ~3.1V - 3.3V
  pinMode(POT_ADC_PIN, INPUT);

  // Configuração do Canal PWM do Servomotor
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(SERVO_PWM_PIN, PWM_FREQ, PWM_RESOLUTION); // Core v3.x
  #else
    ledcSetup(0, PWM_FREQ, PWM_RESOLUTION);               // Core v2.x
    ledcAttachPin(SERVO_PWM_PIN, 0);
  #endif

  // Garante o posicionamento no ponto de repouso inicial
  setServoPulseUs(PULSE_INIT_US);
  
  Serial.println("\n--- ESP32-S3: Gerador de Degrau e Captura de Dados ---");
  Serial.println("Tempo_ms,ADC_Bruto,Tensao_V");
  delay(1000);
}

void loop() {
  // ---------------------------------------------------------------------------
  // FASE 1: Preparação e Estabilização em Repouso
  // ---------------------------------------------------------------------------
  setServoPulseUs(PULSE_INIT_US);
  digitalWrite(TRIGGER_PIN, LOW);
  delay(REST_DURATION_MS); // Aguarda o sistema mecânico parar totalmente

  // ---------------------------------------------------------------------------
  // FASE 2: Aplicação do Degrau e Amostragem (0 a 2.0s)
  // ---------------------------------------------------------------------------
  
  // 1. Dispara o Trigger do Osciloscópio e o Sinal PWM do Degrau no mesmo instante (t=0)
  digitalWrite(TRIGGER_PIN, HIGH);
  setServoPulseUs(PULSE_STEP_US);

  uint32_t t_start_us = micros();
  uint32_t next_sample_us = t_start_us;
  uint32_t current_us = t_start_us;
  uint32_t step_duration_us = STEP_DURATION_MS * 1000;

  // Loop de amostragem determinístico por 2000 ms
  while ((current_us - t_start_us) <= step_duration_us) {
    current_us = micros();

    // Executa a leitura rigorosamente a cada 10.000 µs (10 ms)
    if (current_us >= next_sample_us) {
      uint32_t elapsed_ms = (current_us - t_start_us) / 1000;
      
      // Leitura do pino ADC
      int adc_val = analogRead(POT_ADC_PIN);
      float voltage = (adc_val / 4095.0) * 3.3;

      // Imprime na Serial (compatível com Serial Plotter e scripts Python)
      Serial.print(elapsed_ms);
      Serial.print(",");
      Serial.print(adc_val);
      Serial.print(",");
      Serial.println(voltage, 3);

      next_sample_us += SAMPLE_INTERVAL_US;
    }
  }

  // ---------------------------------------------------------------------------
  // FASE 3: Encerramento do Degrau
  // ---------------------------------------------------------------------------
  digitalWrite(TRIGGER_PIN, LOW); // Finaliza o pulso do osciloscópio ao fim dos 2s
  
  // Imprime uma linha em branco para separar os ciclos na Serial
  Serial.println();
}