void taskReadSensor(void *pvParameters);
void taskWriteSD(void *pvParameters);

#include <Arduino.h>
#include "driver/ledc.h"
#include <HX711.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include "max6675.h"
#include "freertos/task.h"   // Biblioteca que proporciona a criação e manuseamento das Tasks
#include "freertos/queue.h"  // Biblioteca que proporciona a criação e manuseamento das filas

#define PWM_PIN 27 // ALTERAR O PINO
#define PWM_CHANNEL 0 // Canal LEDC

#define pinSO  19 
#define pinCS  5 
#define pinCLK 18  
#define pinmosi 23

const int LOADCELL_DOUT_PIN = 35;
const int LOADCELL_SCK_PIN = 18;

// MAX6675 sensorTemp(pinCLK, pinCS, pinSO); 
HX711 scl;
File df;

SemaphoreHandle_t xsmfr;
TaskHandle_t xsnsr, xSD, xtrmpr;

float brt, liquido, temp;

/*void IRAM_ATTR isr() {
  xSemaphoreGiveFromISR(xsmfr, NULL);
  }
*/
SPIClass spi = SPIClass(VSPI);
char nomeConcat[16];

SemaphoreHandle_t xMutex;
QueueHandle_t SDdataQueue;

String data = "";

void setup() {
  xMutex = xSemaphoreCreateMutex(); // cria o objeto do semáforo xMutex
  SDdataQueue = xQueueCreate(100, sizeof(String)); // 100 máximo da fila
  Serial.begin(9600); // 115200
  spi.begin(pinCLK, pinSO,pinmosi, pinCS);
  Serial.println("Chegou aqui");
  ledcSetup(PWM_CHANNEL, 20000000 , 1);
  //PWM
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 1);
  //Célula de carga
  if(!SD.begin(pinCS, spi)){
    Serial.println("Card Mount Failed");
    return;
  }
  scl.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  //Teste antes de setar escala
  // Serial.print("read average: \t\t");
  // Serial.println(scl.read_average(20));  	// print the average of 20 readings from the ADC

  // scl.set_scale(100.0); // Verificar depois o fator de calibracao
  // float zero = scl.get_units(50);
  // scl.tare(zero);

  // Cartão SD
  
  // Algoritmo para ir criando um arquivo toda vez que inicializa
  int n = 1;
  bool parar = false;
  while (!parar)
  {
      sprintf(nomeConcat, "/dataBE%d.txt", n);
      if (SD.exists(nomeConcat))
        n++;
      else
        parar = true;
    }
  df = SD.open(nomeConcat, FILE_WRITE);
  df.close();

 //xsmfr = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
      taskReadSensor,
      "TaskSensor",
      10000,
      NULL,
      1,
      NULL,
      0);
  xTaskCreatePinnedToCore(
      taskWriteSD,
      "TaskSD",
      10000,
      NULL,
      1,
      NULL,
      1);
}

void loop() {
  // Nada precisa ser feito aqui, as tarefas irão executar em paralelo
    vTaskDelay(100);
}

void taskReadSensor(void *pvParameters) {
  //(void)pvParameters;
  float leitura;
  while (true) {
    
      //temp = sensorTemp.readCelsius();
      // Rever leitura do HX711
      //brt = scl.read();
      //liquido = brt - scl.get_offset();
      //leitura=scl.get_units(1);
      data = "Temp:"+ String(temp) + ", Leitura:" +  String(leitura);
      Serial.println(data);
      if (uxQueueSpacesAvailable(SDdataQueue) != 0)
      {
        xQueueSend(SDdataQueue, &data, 0);
      }
  }vTaskDelay(1000);
}
#define SDMAX 20
void taskWriteSD(void *pvParameters) {
  //(void)pvParameters;
  uint32_t contador_sd = 0;
  while (true) {
    if (uxQueueMessagesWaiting(SDdataQueue) > SDMAX){
      while (contador_sd < SDMAX)
      {
        xQueueReceive(SDdataQueue, &data, 0);
        df = SD.open(nomeConcat, FILE_APPEND);
        df.println(data);
        df.close();
        contador_sd++;
      }
    }
  }vTaskDelay(1000);
}