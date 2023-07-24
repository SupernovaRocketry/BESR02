void taskReadSensor(void *pvParameters);
void taskWriteSD(void *pvParameters);

#include <HX711.h>
#include <SD.h>

const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;
const int PWM_PIN = 32;
const int PWM_FREQ = 20000000;
const int PWM_CHANNEL = 1;
const int PWM_RESOLUTION = 8;

HX711 scl;
File df;

SemaphoreHandle_t xsmfr;
TaskHandle_t xsnsr, xSD;

float brt, liquido;

void IRAM_ATTR isr() {
  xSemaphoreGiveFromISR(xsmfr, NULL);
}

void setup() {
  Serial.begin(9600);
  pinMode(PWM_PIN, OUTPUT);
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  scl.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scl.set_scale(100.0);
  scl.tare();
  SD.begin();
  xsmfr = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
      taskReadSensor,
      "TaskSensor",
      10000,
      NULL,
      1,
      &xsnsr,
      1);
  xTaskCreatePinnedToCore(
      taskWriteSD,
      "TaskSD",
      10000,
      NULL,
      2,
      &xSD,
      0);
}

void loop() {
  // Nada precisa ser feito aqui, as tarefas irão executar em paralelo
}

void taskReadSensor(void *pvParameters) {
  (void)pvParameters;
  while (true) {
    if (xSemaphoreTake(xsmfr, portMAX_DELAY) == pdTRUE) {
      ledcWrite(PWM_CHANNEL, 255);
      delayMicroseconds(25);
      ledcWrite(PWM_CHANNEL, 0);
      brt = scl.read();
      liquido = brt - scl.get_offset();
      xTaskNotifyGive(xSD);
    }
  }
}

void taskWriteSD(void *pvParameters) {
  (void)pvParameters;
  while (true) {
    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 0) {
      df = SD.open("Teste_estatico.txt", FILE_WRITE);
      df.print(brt);
      df.print(", ");
      df.println(liquido);
      df.close();
    }
  }
}