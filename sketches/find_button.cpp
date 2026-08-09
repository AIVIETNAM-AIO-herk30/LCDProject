#include <Arduino.h>

const int candidates[] = {0, 1, 3, 4, 5, 8, 9};
const int NUM = 7;

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < NUM; i++) {
    pinMode(candidates[i], INPUT_PULLUP);
  }
  Serial.println("Bam switch va xem chan nao in ra LOW...");
}

void loop() {
  for (int i = 0; i < NUM; i++) {
    if (digitalRead(candidates[i]) == LOW) {
      Serial.printf("GPIO %d -> LOW  (day la chan switch!)\n", candidates[i]);
      delay(300);
    }
  }
}
