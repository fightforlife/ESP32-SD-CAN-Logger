// libraries
#include <Arduino.h>
#include <TimeLib.h>   //https://github.com/PaulStoffregen/Time
#include <DS3232RTC.h> //https://github.com/JChristensen/DS3232RTC/
#include <ESP32_TWAI_Arduino.h> 
#include "SdFat.h"


#define RTC_SDA_PIN 21
#define RTC_SCL_PIN 22
#define SD_CS_PIN 5
//#define SD_MISO_PIN 2
//#define SD_MOSI_PIN 15
//#define SD_SCK_PIN 14
#define SD_SPI_MHZ 4
#define CAN_TX_PIN 27
#define CAN_RX_PIN 26
#define CAN_BAUDRATE 500E3
//#define ADC_TPS_PIN 4
//#define ADC_AFR_PIN 15
#define LED_PIN 2
//#define AFR_STATUS_PIN
#define BUTTON_PIN 0

// custom functions declaration
void blockingBlink(int num, int blinkDelay, int pauseDelay);
void initRTC();
void initSD();

DS3232RTC RTC;
ESP32TWAI CAN;
SPIClass SD_SPI(VSPI);
SdFat sd;
SdFile file;


void setup()
{
  // Start Serial
  Serial.begin(115200);
  Serial.println("[Serial] initialized.");
  pinMode(BUTTON_PIN, INPUT);
  initRTC();
  initSD();


  CAN.setPins(CAN_RX_PIN,CAN_TX_PIN);
  CAN.setMode(TWAI_MODE_LISTEN_ONLY);
  CAN.setBauadrate(CAN_BAUDRATE);
  CAN.begin();
  CAN.dumpConfig();

}

void loop()
{

  Serial.println("[LOG] Press to Start");
  while (digitalRead(BUTTON_PIN))
  {
  }
  while (!digitalRead(BUTTON_PIN))
  {
  }
    // creating filename
  char fileName[20];
  time_t t = now();
  sprintf(fileName, "%.4d%.2d%.2d_%.2d%.2d%.2d.csv", year(t), month(t), day(t), hour(t), minute(t), second(t));
  Serial.print("[SD] creating logfile: ");
  Serial.println(fileName);

  //creating file
  while(!file.open(fileName, FILE_WRITE))
  {
    sd.errorPrint(&Serial);
    Serial.println("[SD] file init failed. Retrying.");
    blockingBlink(3, 200, 2000);
  }
  Serial.println("[SD] file init success. File opened.");
  file.println("canId,dlc,B0,B1,B2,B3,B4,B5,B6,B7");
  Serial.println("[LOG] started");

  while (digitalRead(BUTTON_PIN))
  {
    if (CAN.availableMessages()>0)
    {
      
      CAN.receiveMessage();
      char logLine[100]="";
      sprintf(logLine, "%X,%d,%.16X", CAN.rxMessage.identifier, CAN.rxMessage.data_length_code, CAN.rxMessage.data);
      file.println(logLine);
      Serial.println(logLine);
    }
  }


while (!digitalRead(BUTTON_PIN))
{
}
file.close();
Serial.println("[LOG] stopped and file saved");
}

void blockingBlink(int num, int blinkDelay, int pauseDelay)
{
  pinMode(LED_PIN, OUTPUT);
  for (int i = 0; i < num; i++)
  {
    digitalWrite(LED_PIN, 1);
    delay(blinkDelay);
    digitalWrite(LED_PIN, 0);
    delay(blinkDelay);
  }
  delay(pauseDelay);
}

void initRTC()
{

  // Initialization
  Serial.println("[RTC] Init started");
  RTC.begin(); // does not  return anything
  while (!RTC.get())
  {
    Serial.println("[RTC] Couldn't get time, no connection");
    blockingBlink(2, 200, 1000);
  }
  Serial.println("[RTC] connected");

  // Check RTC validitiy
  if (RTC.oscStopped(false))
  {
    Serial.println("[RTC] lost Power in the past.");
    Serial.println("[RTC] Please enter time in the following format: DD MM YY HH MM SS");
    while (!Serial.available())
    {
      blockingBlink(2, 200, 1000);
    }
    // Read Input
    char timeMessage[18];
    int messagePosition = 0;
    while (messagePosition < 17)
    {
      while (!Serial.available())
      {
      };
      timeMessage[messagePosition] = Serial.read();
      messagePosition++;
      timeMessage[messagePosition] = '\0';
    }
    // convert time
    Serial.print("[RTC] Received string: ");
    Serial.println(timeMessage);
    int hr, min, sec, day, mnth, yr;
    if (sscanf(timeMessage, "%d %d %d %d %d %d", &day, &mnth, &yr, &hr, &min, &sec) != 6 || yr < 22 || mnth > 12 || day > 31 || hr > 24 || min > 60 || sec > 60)
    {
      Serial.print("[RTC] String is invalid. ESP32 will now reset. Try again.");
      delay(3000);
      ESP.restart();
    }
    setTime(hr, min, sec, day, mnth, yr); // set System time
    RTC.set(now());                       // set RTC Time
    Serial.print("[RTC] Set RTC time to: ");
    Serial.println(RTC.get());
    Serial.println("[RTC] init finished. ");
  }

  // print system time
  setTime(RTC.get());
  Serial.print("[RTC] Set system time to: ");
  Serial.println(now());
}

void initSD()
{
  // init SD connection
  Serial.println("[SD] connection init started");
  while(!sd.begin(SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(SD_SPI_MHZ),&SD_SPI)))
  {
    Serial.println("[SD] connection init failed! Retrying.");
    blockingBlink(3, 200, 2000);
  }

}


