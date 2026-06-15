#include <OneWire.h>
#include <DallasTemperature.h>
#include <PID_v1.h>

// Definirea pinului pentru senzorul de temperatură și pinului pentru senzorul TDS
#define TEMPERATURE_SENSOR_PIN A0
#define ONE_WIRE_BUS 2

// Valoarea de calibrare pentru senzorul TDS (senzor calibrat cu NaCL și apă distilată)
#define CALIBRATION_VALUE 297

// Definirea pinilor pentru controlul driverului BTS7960B
#define RPWM_PIN 8
#define LPWM_PIN 9
#define R_EN_PIN 7
#define L_EN_PIN 6

// Temperatura de referință
#define REFERENCE_TEMPERATURE 50.0

// Parametrii PID
double Kp = 2.0, Ki = 0.5, Kd = 1.0;

// Variabile PID
double input, output, setpoint;
PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

// Inițializarea obiectelor OneWire și DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

String inputString = "";
boolean stringComplete = false;

void setup() {
  // Inițializarea comunicării seriale pentru monitorizare
  Serial.begin(115200);
  
  // Inițializarea comunicării seriale cu ESP-01S
  Serial1.begin(115200);
  
  // Pornirea senzorului de temperatură
  sensors.begin();
  
  // Configurarea pinilor pentru controlul driverului BTS7960B
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  
  // Setarea temperaturii de referință pentru PID
  setpoint = REFERENCE_TEMPERATURE;
  
  // Activarea regulatorului PID
  myPID.SetMode(AUTOMATIC);
  
  // Setarea frecvenței PWM la 1kHz pentru pinii 8 și 9
  TCCR1B = TCCR1B & B11111000 | B00000010;
}

void loop() {
  // Cererea valorii de temperatură de la senzor
  sensors.requestTemperatures();
  
  // Citirea valorii de temperatură
  float temperature = sensors.getTempCByIndex(0);
  
  // Actualizarea valorii de intrare pentru PID
  input = temperature;
  
  // Calcularea ieșirii PID
  myPID.Compute();
  
  // Controlul încălzitorului de apă în funcție de ieșirea PID
  if (output > 0) {
    // Pornirea încălzitorului cu un semnal PWM proporțional cu ieșirea PID
    digitalWrite(R_EN_PIN, HIGH);
    digitalWrite(L_EN_PIN, HIGH);
    analogWrite(RPWM_PIN, output);
    analogWrite(LPWM_PIN, 0);
  } else {
    // Oprirea încălzitorului
    digitalWrite(R_EN_PIN, LOW);
    digitalWrite(L_EN_PIN, LOW);
    analogWrite(RPWM_PIN, 0);
    analogWrite(LPWM_PIN, 0);
  }
  
  // Citirea valorii brute a senzorului TDS și calibrarea acesteia
  int rawTDSValue = analogRead(TEMPERATURE_SENSOR_PIN);
  float calibratedTDSValue = map(rawTDSValue, 0, CALIBRATION_VALUE, 0, 500);
  
  // Verificăm dacă sunt date disponibile de la ESP8266
  while (Serial1.available()) {
    char inChar = (char)Serial1.read();
    inputString += inChar;
    if (inChar == '\n') {
      stringComplete = true;
    }
  }

  // Procesăm datele primite de la ESP8266
  if (stringComplete) {
    inputString.trim();
    setpoint = inputString.toFloat();
    Serial1.print("OK");
    inputString = "";
    stringComplete = false;
  }

  // Calculăm procentul de atingere a temperaturii de referință
  float temperaturePercentage = (temperature / setpoint) * 100;

  // Trimitem valorile către ESP8266
  Serial1.print(temperature);
  Serial1.print(",");
  Serial1.print(calibratedTDSValue);
  Serial1.print(",");
  Serial1.println(temperaturePercentage);
  
  // Afișarea valorilor în monitorul serial
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Valoarea TDS: ");
  Serial.print(calibratedTDSValue, 0);
  Serial.println(" ppm");
  Serial.print("Ieșire PID: ");
  Serial.print(output);
  Serial.println("\n");
  
  // Așteptare pentru a nu suprasolicita serialul
  delay(1000);
}