#define BUFFERSIZE 300  //Anzahl Messwerte für BPM Berechnung
#define INTERVAL 10     //Sample Interval
#define BATTERY_INTERVAL 60000
#define QFAKTORA 100    //Verstärkungsfaktor
#define QFAKTORB 300
#define QTHRESHOLD 1000  //Schwelle für Peakdetektion
// Pinbelegung
#define RESETN P0_18
#define LO_PLUS P0_13
#define LO_MINUS P0_14
#define LEDR P0_15
#define LEDG P0_16
#define LEDB P0_24
#define GPIO7 P0_25
#define GPIO8 P1_0
#define GPIO16_A P0_3
#define GPIO17_A P0_28
#define SCL P0_2
#define SDA P0_31
#define STAT P1_12
#define ALRT P0_29
#define REFOUT P0_30
#define SIGNAL_OUT P0_4
#define SPI_MISO P0_21
#define SPI_MOSI P0_20
#define SPI_CS P0_17
#define SPI_CLK P0_19

// ---- Bluetooth -----------------
#include <ArduinoBLE.h>
#include "MAX17048.h"
MAX17048 pwr_mgmt;

BLEService bleService("4a30d5ac-1d36-11ef-9262-0242ac120002");  // Bluetooth® Low Energy LED Service

// Bluetooth® Low Energy LED Switch Characteristic - custom 128-bit UUID, read and writable by central
BLEFloatCharacteristic Signal(" 62045d8e-1d36-11ef-9262-0242ac120002", BLERead | BLENotify);
BLEFloatCharacteristic BPM_blue("8a405d98-1d36-11ef-9262-0242ac120002", BLERead | BLENotify);
BLEFloatCharacteristic battery("363a2846-1dc7-11ef-9262-0242ac120002", BLERead | BLENotify);
// ---------------------------------

int data_buffer[BUFFERSIZE];
int derivation_buffer[BUFFERSIZE];
long q_buffer[BUFFERSIZE];

unsigned int timestamp = 0;
unsigned int last_timestamp = 0;

unsigned long previousMillis = 0;

float ladestand = 0;

float BPM = 0.0;
int z = 0;

int battery_counter = 0;

void setup() {

  Serial.begin(9600);
  pinMode(SIGNAL_OUT, INPUT);  //Analogsignal EKG Platine
  pinMode(GPIO8, OUTPUT);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  pinMode(STAT,INPUT);
  digitalWrite(GPIO8, HIGH);
  
digitalWrite(LEDR, HIGH);
digitalWrite(LEDG, HIGH);
digitalWrite(LEDB, HIGH);  

   Wire.begin();
   pwr_mgmt.attatch(Wire);
  // ---- Bluetooth -----------------
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  // set advertised local name and service UUID:
  BLE.setLocalName("EKG");
  BLE.setAdvertisedService(bleService);

  // add the characteristic to the service
  bleService.addCharacteristic(Signal);
  bleService.addCharacteristic(BPM_blue);
  bleService.addCharacteristic(battery);

  // add service
  BLE.addService(bleService);

  // set the initial value for the characeristic:
  Signal.writeValue(0);
  BPM_blue.writeValue(0);

  // start advertising
  BLE.advertise();
  // ---------------------------------
}

void loop() {

  BLEDevice central = BLE.central();  //Bluetooth
  unsigned long currentMillis = millis();

  if (central) {

  if(battery_counter >= 1000)
  {          
    ladestand = pwr_mgmt.percent();
    ladestand = float(ladestand) * 1.0; 
    if (central.connected()) {  // ---- Bluetooth -----------------
        battery.writeValue(ladestand);
      }
      battery_counter = 0;
  }
    
    if (currentMillis - previousMillis >= INTERVAL)  //Sampleintervall einhalten
    {
      previousMillis = currentMillis;
      data_buffer[z] = analogRead(SIGNAL_OUT); //Daten einlesen
      z = (z + 1) % BUFFERSIZE;  //Ringbuffer
            if (central.connected()) {  // ---- Bluetooth -----------------
                Signal.writeValue(data_buffer[z]); }
      battery_counter ++;
    }

    if (z == BUFFERSIZE - 1)  //Wenn Ringbuffer voll
    {
      last_timestamp = timestamp;  //Zeitmessung, wie lange füllen des Buffers gedauert hat beenden
      timestamp = millis();

      signalprocessing();  //Ableitungs- und Quadratischen Buffer berechnen

      peakdetection_and_BPM(); //Peaks detektieren und BPM berechnen

      if (central.connected()) {  // ---- Bluetooth -----------------
        BPM_blue.writeValue(BPM);
        battery.writeValue(ladestand);
      }

       timestamp = millis(); //Zeitmessung, wie lange füllen des Buffers wieder starten
    }  //if Ringbuffer voll
} //if central
}//loop


void signalprocessing() {
  for (int i = 0; i < BUFFERSIZE - 1; i++) {
    derivation_buffer[i] = data_buffer[i + 1] - data_buffer[i];  //Ableitung der Messwerte berechnen

    long derivationsquare = derivation_buffer[i] * derivation_buffer[i] * QFAKTORA;  //Quadratische Verstärkung der Ableitung mit Verstärkungsfaktor
    long qfactor = derivationsquare / (QFAKTORB * QFAKTORB);                         //Teilen, durch zweiten Faktor

    q_buffer[i] = derivation_buffer[i] * qfactor;  //Buffer für Quadratische Verstärkung
  }
}


void peakdetection_and_BPM()
{
int peak_buffer[2] = {0,0}; //Ort des Peaks
int peak_count = 0; // Zähler für Anzahl der Peaks

for (int p = 0; p < BUFFERSIZE && peak_count <= 1; p++) //wenn 2 Peaks gefunden oder Buffer vorbei hört es auf
{
  long absolute = abs(q_buffer[p]); 

      if (absolute > QTHRESHOLD) //Vergleich mit Schwellenwert
      {
        peak_buffer[peak_count] = p;
        peak_count ++;
        p = p + 8; //Refraktärzeit, um Peak nicht doppelt zu erkennen
      } }

float T =  (((timestamp - last_timestamp) * (peak_buffer[1]-peak_buffer[0])) / BUFFERSIZE) ; //Periodendauer zwischen zwei Herzschlägen berechnen

if (T != 0)
{     BPM = 60000.0  / T; //Beats per minute berechnen
}
}


//-----------Messwertausgabe zum Debuggen-------------------
//for (int i = 0; i < BUFFERSIZE; i++)  //Messwertausgabe
//      {
//        Serial.print(data_buffer[i]); Serial.print(";"); Serial.print(derivation_buffer[i]); Serial.print(";"); 
//        Serial.print(q_buffer[i]); Serial.print(";"); Serial.print(peak_buffer[0]); Serial.print(";"); 
//        Serial.print(peak_buffer[1]); Serial.print(";"); Serial.print(T); Serial.print(";"); Serial.println(BPM);
//      }
