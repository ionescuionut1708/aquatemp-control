#include <OneWire.h>
#include <DallasTemperature.h>

#define TDS_SENSOR_PIN A0
#define ONE_WIRE_BUS 2
#define CALIBRATION_VALUE 297
#define RPWM_PIN 8
#define LPWM_PIN 9
#define R_EN_PIN 7
#define L_EN_PIN 6

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

String inputString = "";
boolean stringComplete = false;

float setpoint = 25.0; //implicit value [celsius degrees]
float u_de_k = 0;
float epsilon_de_k = 0;
float epsilon_de_k_minus_1 = 0;
float u_de_k_minus_1 = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  sensors.begin();
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
}

void loop() {
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  sensors.requestTemperatures();

  while (Serial1.available()) {
    char inChar = (char)Serial1.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }

  if (stringComplete) {
    inputString.trim();
    setpoint = inputString.toFloat();
    Serial1.println("OK");
    inputString = "";
    stringComplete = false;
  }

  float temperature = sensors.getTempCByIndex(0);

  // Implementation of the PI algorithm
  epsilon_de_k = setpoint - temperature;

  if (epsilon_de_k < 0) {
    u_de_k = epsilon_de_k * 1.49;
  } else {
    u_de_k = epsilon_de_k * 1.49 - epsilon_de_k_minus_1 * 1.29 + u_de_k_minus_1;
  }

  if (u_de_k > 160) u_de_k = 160;  // limit to avoid damaging the water heating element
  else if (u_de_k < 0) u_de_k = 0;

  Serial.println(u_de_k);
  Serial.println("\n");

  analogWrite(RPWM_PIN, u_de_k);

  epsilon_de_k_minus_1 = epsilon_de_k;
  u_de_k_minus_1 = u_de_k;

  int rawTDSValue = analogRead(TDS_SENSOR_PIN);
  float calibratedTDSValue = map(rawTDSValue, 0, CALIBRATION_VALUE, 0, 500);

  float temperaturePercentage = (temperature / setpoint) * 100;
  temperaturePercentage = constrain(temperaturePercentage, 0, 100);

  Serial1.print(temperature);
  Serial1.print(",");
  Serial1.print(calibratedTDSValue);
  Serial1.print(",");
  Serial1.print(temperaturePercentage);
  Serial1.print(",");
  Serial1.println(u_de_k);

  delay(500);
}
