#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP32Servo.h>
#include "HX711.h"

// Pinos I2C do ESP32 (padrão)
#define SDA_PIN 22
#define SCL_PIN 21

// Pino do sinal PWM do servo
#define SERVO_PIN 4

// --- Pinos do Módulo HX711
#define HX711_DT_PIN 16  // RX2
#define HX711_SCK_PIN 17 // TX2

const float BRACO_SERVO_CM = 1.5;

Adafruit_INA219 ina219;
Servo meuServo;
HX711 balanca;

unsigned long ultimaLeitura = 0;
const unsigned long intervaloLeitura = 200; // ms entre leituras

// Variável para fator de calibração da célula de carga
// Você precisará ajustar esse valor experimentalmente usando um peso conhecido
float fator_calibracao = 420.0;

void lerEImprimirDados(int angulo)
{
  float busVoltage_V = ina219.getBusVoltage_V();       // tensão no barramento (lado da carga)
  float shuntVoltage_mV = ina219.getShuntVoltage_mV(); // queda de tensão no shunt
  float current_mA = ina219.getCurrent_mA();           // corrente
  float power_mW = ina219.getPower_mW();               // potência

  // Tensão total de alimentação = tensão do barramento + queda no shunt
  float loadVoltage_V = busVoltage_V + (shuntVoltage_mV / 1000.0);

  // --- Leitura Mecânica (HX711) ---
  // Lê o peso atual em kg (assumindo que o fator de calibração converte para kg)
  float forca_kg = balanca.get_units(3); // Tira média de 3 leituras para estabilizar
  
  // Como o fio de nylon puxa a célula de carga, calculamos o torque:
  // Torque (kg.cm) = Força (kg) * Distância (cm)
  float torque_kgcm = forca_kg * BRACO_SERVO_CM;

  // Evita leituras negativas residuais na plotagem do gráfico
  if(torque_kgcm < 0) torque_kgcm = 0;

  Serial.print("tempo (ms):");
  Serial.print(millis());
  Serial.print(", angulo (°):");
  Serial.print(angulo);
  Serial.print(", Voltagem (V):");
  Serial.print(loadVoltage_V, 3);
  Serial.print(", Corrente (mA):");
  Serial.print(current_mA, 2);
  Serial.print(", Potencia (mW):");
  Serial.println(power_mW, 2);
  Serial.print(", torque (kg.cm):");
  Serial.println(torque_kgcm, 3);
}


void setup()
{
  Serial.begin(9600);
  delay(1000);

  // Inicializa I2C nos pinos definidos
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inicializa o INA219
  if (!ina219.begin())
  {
    Serial.println("Falha ao encontrar o INA219. Verifique as ligacoes!");
    while (1)
    {
      delay(10);
    }
  }

  // Opcional: ajustar faixa de calibração conforme a corrente esperada
  // ina219.setCalibration_32V_2A();   // padrão, boa para servos comuns
  // ina219.setCalibration_16V_400mA(); // mais precisão para correntes baixas

  // Inicializa o HX711
  balanca.begin(HX711_DT_PIN, HX711_SCK_PIN);
  balanca.set_scale(fator_calibracao); 
  balanca.tare(); // Zera a balança. Certifique-se de que o nylon não está tensionado ao ligar!

  // Configura o servo
  ESP32PWM::allocateTimer(0);
  meuServo.setPeriodHertz(50);           // frequência padrão de servos (50Hz)
  meuServo.attach(SERVO_PIN, 500, 2400); // pulso min/max em microssegundos

  Serial.println("tempo_ms, angulo, tensao_V, corrente_mA, potencia_mW, torque_kgcm");
}

void loop()
{
  // Movimenta o servo em varredura de 0 a 180 graus
  static int angulo = 0;
  static int passo = 1;

  angulo += passo;
  if (angulo >= 180 || angulo <= 0)
    passo = -passo;

  meuServo.write(angulo);

  // Faz a leitura do INA219 periodicamente
  unsigned long agora = millis();
  if (agora - ultimaLeitura >= intervaloLeitura)
  {
    ultimaLeitura = agora;
    lerEImprimirDados(angulo);
  }

  delay(15); // pequeno delay entre passos do servo
}