#define BUFFERSIZE 500
#define INTERVAL 20

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

short data_buffer[BUFFERSIZE];
float correlation_buffer[BUFFERSIZE];

unsigned int timestamp = 0;
unsigned int last_timestamp = 0;

unsigned long previousMillis = 0;

float BPM = 0.0;
int z = 0;

void setup() {
  
Serial.begin(9600);
pinMode(SIGNAL_OUT,INPUT); //Analogsignal EKG Platine


}


void loop() {

unsigned long currentMillis = millis();

if (currentMillis - previousMillis >= INTERVAL) 
{
  previousMillis = currentMillis;
  data_buffer[z] = analogRead(SIGNAL_OUT);
  Serial.println(data_buffer[z]);
  z = (z+1) % BUFFERSIZE; 
}

  if(z==499)
  {
    last_timestamp = timestamp;
    timestamp = millis();

    autocorrelation(data_buffer, mean(data_buffer, BUFFERSIZE), correlation_buffer, BUFFERSIZE, 0);
    
    int p = 40;

    while(!(correlation_buffer[p] > 0.25 && correlation_buffer[p-1] < correlation_buffer[p] && correlation_buffer[p+1] < correlation_buffer[p]) && p<245)
    {
      p++;
    }
    
    
  if(p<245) 
    {
     BPM = 60000 * BUFFERSIZE / ((timestamp - last_timestamp) * p);
    }
    
   for(int i = 0; i<250; i++)
  {
    //Serial.print(data_buffer[i]); Serial.print(";"); Serial.print(correlation_buffer[i]);Serial.print(";"); Serial.println(BPM);
    //Serial.println(data_buffer[i]); //Serial.print(",");
  }

  }
}


void ringbuffer_add_element (short * buf, int laenge, int daten) //Ringbuffer -> Variablen: Array, Länge des Arrays, einzugebende Daten
{
  buf[z] = daten;
  z = (z+1) % laenge;
  return;
}

float mean( short *d, unsigned int len)
{
  int sum = 0;
  for (unsigned int i = 0; i < len ; i++)
  {
    sum += d[i];
  }
  return sum * 1.0 / len;
}

void autocorrelation (short * x, int mean, float * r, unsigned int len, unsigned int startindex)
{
  //x: array der Sensordaten
  //r: array mit Speicherplatz für Ergebnis
  //mean: Mittelwert der Sensordaten
  //len: Anzahl der Arrayeinträge des Sensorsignals s
  
 for (int t = 0; t < len / 2; t ++)
 {
   int n = 0; // Numerator
   int d = 0; // Denominator
   for (int i = 0; i < len; i ++)
   {
     int xim = x[(i + startindex) % len] - mean;
     n += xim * (x[(i + startindex + t) % len] - mean);
     d += xim * xim;
   }

   r[t] = n * 1.0 / d;
 }
}

float locate_peak_parabola(unsigned int x, int y0, int y1, int y2)
{
  if ((y0 < y1) && (y2 < y1) && ((2.0 * y1 - y0 - y2)) != 0) // plausibilität
    return x + 0.5 * (y2 - y0) / (2.0 * y1 - y0 - y2);
  else
    return x;
}
