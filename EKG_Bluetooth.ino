#define BUFFERSIZE 500  //Anzahl Messwerte für BPM Berechnung
#define INTERVAL 20     //Sample Interval
#define QFAKTORA 100    //Verstärkungsfaktor
#define QFAKTORB 300
#define QTHRESHOLD 1000  //Schwelle für Peakdetektion

// ---- Bluetooth -----------------
#include <ArduinoBLE.h>

BLEService bleService("4a30d5ac-1d36-11ef-9262-0242ac120002");  // Bluetooth® Low Energy LED Service

// Bluetooth® Low Energy LED Switch Characteristic - custom 128-bit UUID, read and writable by central
BLEFloatCharacteristic Signal(" 62045d8e-1d36-11ef-9262-0242ac120002", BLERead | BLENotify);
BLEFloatCharacteristic BPM_blue("8a405d98-1d36-11ef-9262-0242ac120002", BLERead | BLENotify);
// ---------------------------------

int data_buffer[BUFFERSIZE];
int derivation_buffer[BUFFERSIZE];
long q_buffer[BUFFERSIZE];


unsigned int timestamp = 0;
unsigned int last_timestamp = 0;

unsigned long previousMillis = 0;

float BPM = 0.0;
int z = 0;

void setup() {

  Serial.begin(9600);
  pinMode(A0, INPUT);  //Analogsignal EKG Platine


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
    if (currentMillis - previousMillis >= INTERVAL)  //Sampleinterval einstellen
    {
      previousMillis = currentMillis;
      data_buffer[z] = analogRead(A0);
      //  Serial.println(data_buffer[z]);
      z = (z + 1) % BUFFERSIZE;  //Ringbuffer
            if (central.connected()) {  // ---- Bluetooth -----------------
        Signal.writeValue(data_buffer[z]);
      }
    }

    if (z == BUFFERSIZE - 1)  //Wenn Ringbuffer voll
    {
      last_timestamp = timestamp;  //Zeitmessung, wie lange füllen des Buffers gedauert hat
      timestamp = millis();

      signalprocessing();  //Ableitungs- und Quadratischen Buffer berechnen


      int peak_buffer[2] = { 0, 0 };
      int peak_count = 0;
      short peak_gefunden[BUFFERSIZE] = { 0 };

      for (int p = 0; p < BUFFERSIZE && peak_count <= 1; p++)  //wenn 2 Peaks gefunden oder Buffer vorbei hört es auf
      {
        long absolute = abs(q_buffer[p]);

        //Serial.print(absolute); Serial.print(";"); Serial.print(q_buffer[p]); Serial.print(";");

        if (absolute > QTHRESHOLD) {
          peak_gefunden[p] = 1000;
          peak_buffer[peak_count] = p;
          peak_count++;
          p = p + 8;  //Refraktärzeit, um Peak nicht doppelt zu erkennen
        }

        else {
          peak_gefunden[p] = 0;
        }

        //timestamp = millis();
      }


      float T = (((timestamp - last_timestamp) * (peak_buffer[1] - peak_buffer[0])) / BUFFERSIZE);  //Periodendauer zwischen zwei Herzschlägen berechnen

      if (T != 0) {
        BPM = 60000.0 / T;  //Beats per minute berechnen
      }

      if (central.connected()) {  // ---- Bluetooth -----------------
        BPM_blue.writeValue(BPM);
      }

      for (int i = 0; i < BUFFERSIZE; i++)  //Messwertausgabe
      {

        Serial.print(data_buffer[i]);
        Serial.print(";");
        Serial.print(derivation_buffer[i]);
        Serial.print(";");
        Serial.print(peak_gefunden[i]);
        Serial.print(";");
        Serial.print(q_buffer[i]);
        Serial.print(";");
        Serial.print(peak_buffer[0]);
        Serial.print(";");
        Serial.print(peak_buffer[1]);
        Serial.print(";");
        Serial.print(T);
        Serial.print(";");
        Serial.println(BPM);
      }

    }  //if Ringbuffer voll
  }
}  //loop


void signalprocessing() {
  for (int i = 0; i < BUFFERSIZE - 1; i++) {
    derivation_buffer[i] = data_buffer[i + 1] - data_buffer[i];  //Ableitung der Messwerte berechnen

    long derivationsquare = derivation_buffer[i] * derivation_buffer[i] * QFAKTORA;  //Quadratische Verstärkung der Ableitung mit Verstärkungsfaktor
    long qfactor = derivationsquare / (QFAKTORB * QFAKTORB);                         //Teilen, durch zweiten Faktor

    q_buffer[i] = derivation_buffer[i] * qfactor;  //Buffer für Quadratische Verstärkung
  }
}



//Serial.print(derivation_buffer[i]);
//Serial.print(" ");
//Serial.print(derivationsquare); Serial.print(" ");  Serial.print(qfactor); Serial.print(" ");
//Serial.print(q_buffer[i]);
//Serial.print( ( (float) derivation_buffer[i] / QFAKTORB) );
//Serial.println(" ");