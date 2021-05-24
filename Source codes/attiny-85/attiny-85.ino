
const int Rx = 3;
const int Tx = 4;
const byte Ad_in = 1;
#include <SoftwareSerial.h>
SoftwareSerial softSerial(Rx, Tx);

byte r = 0;
char incoming;
char receivedData[8];

char start_symbol = '#';
int address_in;
char start_symbol_in;

char upperByte = 0;
char lowerByte = 0;

int intArray[6];
int n = 6;
char address = 3;
bool received = false;

void setup() {
  softSerial.begin(9600);
  pinMode(Ad_in, INPUT);
  delay(10);
}

void loop() {
  if (softSerial.available() > 0) {
    delay(7);

    while (softSerial.available() > 0) {

      incoming = softSerial.read();
      receivedData[r] = incoming;
      r++;
      received = true;
    }
    r = 0;
  }
  if (received == true) {
    received = false;
    start_symbol_in = receivedData[0];
    address_in = receivedData[1];

    if (start_symbol_in == '#') {
      if (address_in == 3) {
        for (int i = 0; i < 6; i++) {
          intArray[i] = analogRead(Ad_in);
          delay(2);
        }
        sort(intArray, n);
        int value = round(Find_median(intArray, n));
        lowerByte = value & 0xFF;
        upperByte = value >> 8;
        softSerial.print(start_symbol);
        softSerial.print(address);
        softSerial.print(upperByte);
        softSerial.print(lowerByte);
      }
    }
  }
}

void swap(int *p, int *q) {
  int t;
  t = *p;
  *p = *q;
  *q = t;
}

void sort(int pole[], int n) {
  int i, j, temp;
  for (i = 0; i < n - 1; i++) {
    for (j = 0; j < n - i - 1; j++) {
      if (pole[j] > pole[j + 1])
        swap(&pole[j], &pole[j + 1]);
    }
  }
}

float Find_median(int pole[] , int n)
{
  float median = 0;
  if (n % 2 == 0)
    median = (pole[(n - 1) / 2] + pole[n / 2]) / 2.0;
  else
    median = pole[n / 2];
  return median;
}
