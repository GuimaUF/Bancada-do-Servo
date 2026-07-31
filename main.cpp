#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP32Servo.h>
#include "HX711.h"

// Pinos I2C do ESP32 (padrão)
#define SDA_PIN 21
#define SCL_PIN 22

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
const unsigned long intervaloLeitura = 15; // ms entre leituras

// Variável para fator de calibração da célula de carga
// Você precisará ajustar esse valor experimentalmente usando um peso conhecido
float coeficiente_angular = 0.0000454405524; // Substitua pelo seu valor (a)
float coeficiente_linear  = -18.505760251396;    // Substitua pelo seu valor (b)

float torque_maximo_ciclo = 0.0;

void lerEImprimirDados(int angulo)
{
  float busVoltage_V = ina219.getBusVoltage_V();       // tensão no barramento (lado da carga)
  float shuntVoltage_mV = ina219.getShuntVoltage_mV(); // queda de tensão no shunt
  float current_mA = ina219.getCurrent_mA();           // corrente
  float power_mW = ina219.getPower_mW();               // potência

  if (current_mA > 300.0 || current_mA < -0.0 || power_mW > 30.0) {
    return; // Encerra a função aqui e não imprime os valores absurdos
  }

  // Tensão total de alimentação = tensão do barramento + queda no shunt
  float loadVoltage_V = busVoltage_V + (shuntVoltage_mV / 1000.0);

  // --- Leitura Mecânica (HX711) ---
  // Lê o valor bruto (ADC) instantâneo, descartando as médias
  long leitura_bruta = balanca.read();

  // Aplica a equação linear para obter a força (assumindo output em Newtons)
  float forca_newtons = (leitura_bruta * coeficiente_angular) + coeficiente_linear;


  // Evita leituras negativas residuais na plotagem do gráfico
if (forca_newtons < 0) {
    forca_newtons = 0;
  }

  float torque_kgcm = (forca_newtons/9.80665) * BRACO_SERVO_CM;

  if (torque_kgcm > torque_maximo_ciclo) {
    torque_maximo_ciclo = torque_kgcm;
  }

  Serial.print("\nVoltagem (V):");
  Serial.print(loadVoltage_V, 3);
  Serial.print("\nCorrente (mA):");
  Serial.print(current_mA, 2);
  Serial.print("\nPotencia (mW):");
  Serial.println(power_mW, 2);
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Inicializa I2C nos pinos definidos
  Wire.begin(SDA_PIN, SCL_PIN);

  // Inicializa o INA219 e verifica se está conectado corretamente
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

  // Configura o servo
  ESP32PWM::allocateTimer(0);
  meuServo.setPeriodHertz(50);           // frequência padrão de servos (50Hz)
  meuServo.attach(SERVO_PIN, 500, 2400); // pulso min/max em microssegundos
}

void loop()
{
  // Movimenta o servo em varredura de 0 a 180 graus
  static int angulo = 0;
  static int passo = 1;

  angulo += passo;

if (angulo >= 70 || angulo <= 0) {
    passo = -passo;
    
    // Se o ângulo chegou a 0, significa que um ciclo completo (ida e volta) terminou
    if (angulo <= 0) {
      Serial.println("\n========================================");
      Serial.print("TORQUE MAXIMO DO CICLO: ");
      Serial.print(torque_maximo_ciclo, 3);
      Serial.println(" kg.cm");
      Serial.println("========================================\n");
      
      // Reseta a variável para registrar o máximo do próximo ciclo
      torque_maximo_ciclo = 0.0;
  }
  
}

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

// #include "HX711.h"

// // Pinos do HX711 (Mantendo o seu padrão de hardware)
// #define HX711_DT_PIN 16
// #define HX711_SCK_PIN 17

// HX711 balanca;

// void setup() {
//   // Mantendo o baud rate alinhado com o seu platformio.ini
//   Serial.begin(115200);
//   delay(1000);
  
//   Serial.println("\n--- Leitura de Dados Brutos (ADC) do HX711 ---");
//   Serial.println("Valores exibidos sao inteiros de 24 bits proporcionais a tensao lida.");
  
//   balanca.begin(HX711_DT_PIN, HX711_SCK_PIN);
// }

// void loop() {
//   if (balanca.is_ready()) {
//     // .read() pega a leitura bruta instantânea exata
//     long leitura_instantanea = balanca.read();
    
//     // .read_average(10) tira a média de 10 leituras brutas (filtra ruídos)
//     long leitura_media = balanca.read_average(10);
    
//     Serial.print("Bruto Instantaneo: ");
//     Serial.print(leitura_instantanea);
//     Serial.print(" | Bruto Medio (10x): ");
//     Serial.println(leitura_media);
//   } else {
//     Serial.println("Aguardando conexao com o HX711...");
//   }
  
//   delay(100); // Atualiza o monitor serial a cada meio segundo
// }

// #include "HX711.h"

// // Pinos do HX711
// #define HX711_DT_PIN 16
// #define HX711_SCK_PIN 17

// HX711 balanca;

// // =========================================================================
// // ⬇️⬇️⬇️ INSIRA AQUI OS SEUS COEFICIENTES DA CALIBRAÇÃO ⬇️⬇️⬇️
// // =========================================================================

// float coeficiente_angular = 0.0000454405524; // Substitua pelo seu valor calculado (a)
// float coeficiente_linear  = -18.505760251396;    // Substitua pelo seu valor calculado (b)

// // =========================================================================
// // ⬆️⬆️⬆️ ================================================== ⬆️⬆️⬆️
// // =========================================================================

// void setup() {
//   // Baud rate alinhado com o seu platformio.ini
//   Serial.begin(115200);
//   delay(1000);
  
//   balanca.begin(HX711_DT_PIN, HX711_SCK_PIN);
  
//   // Cabeçalho para o terminal
//   Serial.println("\n--- Sistema Iniciado ---");
//   Serial.println("Lendo dados do HX711 e calculando Forca em Newtons");
//   Serial.println("--------------------------------------------------");
// }

// void loop() {
//   if (balanca.is_ready()) {
//     // Tira a média de 10 leituras brutas
//     long leitura_bruta = balanca.read_average(10);
    
//     // Aplica a equação da reta
//     float forca_newtons = (leitura_bruta * coeficiente_angular) + coeficiente_linear;
    
//     // Print amigável para leitura no terminal
//     Serial.print("Leitura Bruta (ADC): ");
//     Serial.print(leitura_bruta);
//     Serial.print("   |   Forca: ");
//     Serial.print(forca_newtons, 4); // 4 casas decimais
//     Serial.println(" N");
    
//   } else {
//     Serial.println("Erro: Falha na leitura do HX711. Verifique as conexoes.");
//   }
  
//   delay(100); // 2 leituras por segundo para facilitar a visualizacao no terminal
// }