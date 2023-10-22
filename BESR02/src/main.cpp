#include <Arduino.h>
#include "driver/ledc.h"
#include <HX711.h>
#include <SD.h>
#include <SPI.h>
#include "max6675.h"
#include "freertos/task.h"   // Biblioteca que proporciona a criação e manuseamento das Tasks
#include "freertos/queue.h"  // Biblioteca que proporciona a criação e manuseamento das filas

// Definição de pinos
#define PWM_PIN 27 // ALTERAR O PINO
#define PWM_CHANNEL 0 // Canal LEDC
#define pinSO 19
#define pinCS 5
#define pinCLK 18
#define pinmosi 23
const int LOADCELL_DOUT_PIN = 35;
const int LOADCELL_SCK_PIN = 14;

// Declarações globais
SPIClass spi = SPIClass(VSPI); // Variável global para SPI
HX711 scl;
File df;
char nomeConcat[16];
SemaphoreHandle_t xMutex;
QueueHandle_t SDdataQueue;
String data = "";

// Protótipos de função
void taskReadSensor(void *pvParameters);
void taskWriteSD(void *pvParameters);

void setup() {
  // Criação de objetos e configurações iniciais
  xMutex = xSemaphoreCreateMutex();
  SDdataQueue = xQueueCreate(100, sizeof(String));
  Serial.begin(115200);
  spi.begin(pinCLK, pinSO, pinmosi, pinCS); // Inicialização do SPI
  Serial.println("Chegou aqui");
  ledcSetup(PWM_CHANNEL, 20000000, 1);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 1);

  // Inicialização da célula de carga
  if (!SD.begin(pinCS, spi)) {
    Serial.println("Card Mount Failed");
  }
  scl.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN,128);
  delay(3000);
  scl.set_scale(1400); // Verificar depois o fator de calibração
  float zero = scl.get_units(50);
  Serial.println(zero);
  scl.tare(50);
  // Algoritmo para criar um arquivo toda vez que inicializa
  int n = 1;
  bool parar = false;
  while (!parar) {
    sprintf(nomeConcat, "/data%d.txt", n);
    Serial.println(nomeConcat);
    if (SD.exists(nomeConcat))
      n++;
    else
      parar = true;
  }
  df = SD.open("/data1.txt", FILE_WRITE);
  Serial.println(df);
  df.close();

  // Criação de tarefas
  // xTaskCreatePinnedToCore(
  //     taskReadSensor,
  //     "TaskSensor",
  //     10000,
  //     NULL,
  //     1,
  //     NULL,
  //     0);
  // xTaskCreatePinnedToCore(
  //     taskWriteSD,
  //     "TaskSD",
  //     10000,
  //     NULL,
  //     0,
  //     NULL,
  //     1);
}

float leitura,tempo;
void loop() {
  // Nada precisa ser feito aqui, as tarefas irão executar em paralelo
    if(scl.is_ready()){
      leitura = scl.get_units(1);
      if(leitura<0) leitura=0;

      tempo=millis()/1000.0;
      data =String(tempo)+ ", " + String(leitura);
      Serial.println(data);
      df = SD.open(nomeConcat, FILE_APPEND);
      df.println(data);
      df.close();
      Serial.println("Escreveu no SD");
    }
  
}

void taskReadSensor(void *pvParameters) {
  float leitura;
  while (true) {
    // Leitura do sensor e formatação de dados
    leitura = scl.get_units(1);
    data = "Leitura:" + String(leitura);
    Serial.println(data);
    if (uxQueueSpacesAvailable(SDdataQueue) != 0) {
      xQueueSend(SDdataQueue, &data, 0);
    }
  }
}

#define SDMAX 20

void taskWriteSD(void *pvParameters) {
  uint32_t contador_sd = 0;
  while (true) {
    if (uxQueueMessagesWaiting(SDdataQueue) > SDMAX) {
      while (contador_sd < SDMAX) {
        // Gravação de dados no cartão SD
        xQueueReceive(SDdataQueue, &data, 0);
        df = SD.open(nomeConcat, FILE_APPEND);
        df.println(data);
        df.close();
        contador_sd++;
      }
      contador_sd = 0;
      Serial.println("Escreveu no SD");
    }
    vTaskDelay(100);
  }vTaskDelay(500);
}
