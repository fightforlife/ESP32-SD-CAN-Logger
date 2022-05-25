//libraries
#include <Arduino.h>
#include <TimeLib.h> //https://github.com/PaulStoffregen/Time
#include <DS3232RTC.h>  //https://github.com/JChristensen/DS3232RTC/
#include <SdFat.h> //https://github.com/greiman/SdFat
#include <CAN.h> //https://github.com/sandeepmistry/arduino-CAN/

#define RTC_GND_PIN 16
#define RTC_VCC_PIN 17
//#define RTC_SDA_PIN 21
//#define RTC_SCL_PIN 22
#define SD_CS_PIN 5
#define SD_FAT_TYPE 0
#define SPI_CLOCK SD_SCK_MHZ(25)
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK, &SD_SPI)
//#define SD_MISO_PIN 19
//#define SD_MOSI_PIN 23
//#define SD_SCK_PIN 18
#define CAN_TX_PIN 32 
#define CAN_RX_PIN 33
#define CAN_BITRATE 500E3
#define ADC_TPS_PIN 4
#define ADC_AFR_PIN 15
#define LED_PIN 2
//#define AFR_STATUS_PIN 
#define LOG_PIN 0

//custom functions declaration
void blockingBlink(int num, int blinkDelay, int pauseDelay);
void initRTC();
void initSD();
void initCAN(long bitrate, bool observe, bool loopback);
void CANCallback(int packetSize);

DS3232RTC rtc;
SPIClass SD_SPI(VSPI);
SdFat sd;
File file;

void setup () {
  //Start Serial
  Serial.begin(250000);
  Serial.println("[Serial] initialized.");
  pinMode(LOG_PIN, INPUT);
  initRTC();
  initSD();
  initCAN(500E3,1,0);

}

void loop () {

  //creating filename
  char fileName[20];
  time_t t = now();
  sprintf(fileName, "%.4d%.2d%.2d_%.2d%.2d%.2d.csv", year(t), month(t), day(t), hour(t), minute(t), second(t));
  Serial.print("[SD] creating logfile: ");
  Serial.println(fileName);

  //creating file
  file = sd.open(fileName, FILE_WRITE);
  while(!file) {
    file = sd.open(fileName, FILE_WRITE);
    Serial.println("[SD] file init failed. Retrying.");
    blockingBlink(3,200,2000);
  }
  Serial.println("[SD] file init success. File opened.");

  Serial.println("[LOG] Press to Start");
  while (digitalRead(LOG_PIN)){}
  delay(500);
  Serial.println("[LOG] started");
  file.println("logId,millis,canId,dlc,B0,B1,B2,B3,B4,B5,B6,B7"); 
  
  while (digitalRead(LOG_PIN)){
    char buf[500];
    sprintf(buf, "%u,%d,%d,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",millis(),11,8,123,123,123,123,123,123,123,123);
    file.println(buf);
  }
  file.close();
  Serial.println("[LOG] stopped");

}


void blockingBlink(int num, int blinkDelay, int pauseDelay){
  pinMode(LED_PIN, OUTPUT);
  for(int i=0;i<num;i++){
    digitalWrite(LED_PIN,1);
    delay(blinkDelay);
    digitalWrite(LED_PIN,0);
    delay(blinkDelay);
  }
  delay(pauseDelay);
}

void initRTC(){
  //provide power to RTC
  pinMode(RTC_GND_PIN,OUTPUT);
  digitalWrite(RTC_GND_PIN,LOW);
  pinMode(RTC_VCC_PIN,OUTPUT);
  digitalWrite(RTC_VCC_PIN,HIGH);

  //Initialization
  Serial.println("[RTC] Init started");
  rtc.begin();  //does not  return anything
  while(!rtc.get()) {
    Serial.println("[RTC] Couldn't get time, no connection");
    blockingBlink(2,200,1000);
  }
  Serial.println("[RTC] connected");

  //Check RTC validitiy
  if(rtc.oscStopped(false)){
    Serial.println("[RTC] lost Power in the past.");
    Serial.println("[RTC] Please enter time in the following format: DD MM YY HH MM SS");
    while(!Serial.available()){
      blockingBlink(2,200,1000);
    }
    //Read Input
    char timeMessage[18];
    int messagePosition=0;
    while (messagePosition<17){
      while(!Serial.available()){};
      timeMessage[messagePosition] = Serial.read();
      messagePosition++;
      timeMessage[messagePosition] = '\0';
    }
    //convert time
    Serial.print("[RTC] Received string: ");
    Serial.println(timeMessage);
    int hr,min,sec,day,mnth,yr;
    if(sscanf(timeMessage, "%d %d %d %d %d %d", &day, &mnth, &yr, &hr, &min, &sec) != 6 || yr<22 || mnth>12 || day>31 || hr>24 || min>60 || sec>60){
      Serial.print("[RTC] String is invalid. ESP32 will now reset. Try again.");
      delay(3000);
      ESP.restart();
    }
    setTime(hr,min,sec,day,mnth,yr);  //set System time
    rtc.set(now());                   //set RTC Time
    Serial.print("[RTC] Set RTC time to: ");
    Serial.println(rtc.get());
    Serial.println("[RTC] init finished. ");
  }

  //print system time
  setTime(rtc.get());
  Serial.print("[RTC] Set system time to: ");
  Serial.println(now());

}

void initSD(){

  //init SD connection
  Serial.println("[SD] connection init started");
  while(!sd.begin(SD_CONFIG)) {
    Serial.println("[SD] connection init failed! Retrying.");
    blockingBlink(3,200,2000);
  }
  Serial.println("[SD] connection init success.");
}

void initCAN(long bitrate, bool observe, bool loopback){
  Serial.println("[CAN] init started.");
  CAN.setPins(CAN_RX_PIN, CAN_TX_PIN);
  
  while(!CAN.begin(bitrate)){
    Serial.println("[CAN] init failed. Retrying.");
    blockingBlink(4,200,2000);
  }
  if(observe){
    CAN.observe();
  }
  if(loopback){
    CAN.loopback();
  }
  //CAN.onReceive(CANCallback);
  Serial.println("[CAN] init success.");
}

/*
void CANCallback(int packetSize) {

  digitalWrite(LED_PIN,1);
  // received a packet
  Serial.print("Received ");

  if (CAN.packetExtended()) {
    Serial.print("extended ");
  }

  if (CAN.packetRtr()) {
    // Remote transmission request, packet contains no data
    Serial.print("RTR ");
  }

  Serial.print("packet with id 0x");
  Serial.print(CAN.packetId(), HEX);

  if (CAN.packetRtr()) {
    Serial.print(" and requested length ");
    Serial.println(CAN.packetDlc());
  } else {
    Serial.print(" and length ");
    Serial.println(packetSize);

    // only print packet data for non-RTR packets
    while (CAN.available()) {
      Serial.print((char)CAN.read());
    }
    Serial.println();
    digitalWrite(LED_PIN,0);
  }

  Serial.println();
}
*/