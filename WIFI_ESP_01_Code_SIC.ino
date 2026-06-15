#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "ESP8266-AP";
const char* password = "12345678";

AsyncWebServer server(80);
AsyncEventSource events("/events");

String inputString = "";
boolean stringComplete = false;

float temperature;
float tdsValue;
float temperaturePercentage;
float pwmValue;
float setpoint = 25.0; // Valoare implicită

void setup() {
  Serial.begin(115200);
  
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Adresa IP a AP: ");
  Serial.println(IP);
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", generateWebPage());
  });

  server.on("/set_temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("temperature")) {
      setpoint = request->getParam("temperature")->value().toFloat();
      Serial.println(setpoint);
      request->send(200, "text/plain", "Temperatura de referință trimisă cu succes!");
    }
  });
  
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconectat! Ultimul ID de mesaj recepționat: %u\n", client->lastId());
    }
    client->send(generateSensorData().c_str(),"new_readings",millis());
  });
  server.addHandler(&events);
  
  server.begin();
}

void loop() {
  if (Serial.available()) {
    String inputString = Serial.readStringUntil('\n');
    int commaIndex1 = inputString.indexOf(',');
    int commaIndex2 = inputString.indexOf(',', commaIndex1 + 1);
    int commaIndex3 = inputString.indexOf(',', commaIndex2 + 1);
    if (commaIndex1 != -1 && commaIndex2 != -1 && commaIndex3 != -1) {
      temperature = inputString.substring(0, commaIndex1).toFloat();
      tdsValue = inputString.substring(commaIndex1 + 1, commaIndex2).toFloat();
      temperaturePercentage = inputString.substring(commaIndex2 + 1, commaIndex3).toFloat();
      pwmValue = inputString.substring(commaIndex3 + 1).toFloat();
      events.send(generateSensorData().c_str(),"new_readings",millis());
    }
  }

  if (stringComplete) {
    String response = inputString;
    inputString = "";
    stringComplete = false;
    Serial.println(response);
  }
}

String generateSensorData() {
  String jsonData = "{\"temperature\":";
  jsonData += String(temperature, 2);
  jsonData += ",\"tdsValue\":";
  jsonData += String(tdsValue, 0);
  jsonData += ",\"temperaturePercentage\":";
  jsonData += String(temperaturePercentage, 2);
  jsonData += ",\"pwmValue\":";
  jsonData += String(pwmValue, 2);
  jsonData += "}";
  return jsonData;
}

String generateWebPage() {
  String webpage = "<!DOCTYPE html><html>";
  webpage += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  webpage += "<link rel=\"icon\" href=\"data:,\">";
  webpage += "<script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/3.7.0/chart.min.js'></script>";
  webpage += "<style>body { text-align: center; font-family: \"Trebuchet MS\", Arial;}";
  webpage += "table { border-collapse: collapse; width:35%; margin-left:auto; margin-right:auto; }";
  webpage += "th { padding: 12px; background-color: #0043af; color: white; }";
  webpage += "tr { border: 1px solid #ddd; padding: 12px; }";
  webpage += "tr:hover { background-color: #bcbcbc; }";
  webpage += "td { border: none; padding: 12px; }";
  webpage += ".sensor { color:white; font-weight: bold; background-color: #bcbcbc; padding: 1px; }";
  webpage += "</style>";
  webpage += "<script>";
  webpage += "console.log('Chart.js loaded:', typeof Chart !== 'undefined');"; // Verificare încărcare Chart.js
  webpage += "var chart;";
  webpage += "function initChart() {";
  webpage += "  console.log('Initializing chart...');"; // Mesaj de depanare
  webpage += "  var ctx = document.getElementById('myChart').getContext('2d');";
  webpage += "  chart = new Chart(ctx, {";
  webpage += "    type: 'line',";
  webpage += "    data: {";
  webpage += "      labels: [],";
  webpage += "      datasets: [{";
  webpage += "        label: 'Temperatura',";
  webpage += "        data: [],";
  webpage += "        borderColor: 'rgb(255, 99, 132)',";
  webpage += "        yAxisID: 'y',";
  webpage += "      }, {";
  webpage += "        label: 'PWM',";
  webpage += "        data: [],";
  webpage += "        borderColor: 'rgb(54, 162, 235)',";
  webpage += "        yAxisID: 'y1',";
  webpage += "      }]";
  webpage += "    },";
  webpage += "    options: {";
  webpage += "      scales: {";
  webpage += "        y: { type: 'linear', display: true, position: 'left' },";
  webpage += "        y1: { type: 'linear', display: true, position: 'right', grid: { drawOnChartArea: false } }";
  webpage += "      }";
  webpage += "    }";
  webpage += "  });";
  webpage += "  console.log('Chart initialized');"; // Mesaj de depanare
  webpage += "}";
  webpage += "var source = new EventSource('/events');";
  webpage += "source.addEventListener('new_readings', function(e) {";
  webpage += "  console.log(\"New readings!\", e.data);"; // Mesaj de depanare
  webpage += "  var data = JSON.parse(e.data);";
  webpage += "  document.getElementById(\"temperatureValue\").innerHTML = data.temperature.toFixed(2);";
  webpage += "  document.getElementById(\"tdsValue\").innerHTML = data.tdsValue.toFixed(0);";
  webpage += "  document.getElementById(\"temperaturePercentage\").innerHTML = data.temperaturePercentage.toFixed(2);";
  webpage += "  document.getElementById(\"pwmValue\").innerHTML = data.pwmValue.toFixed(2);";
  webpage += "  if (chart) {";
  webpage += "    var now = new Date().toLocaleTimeString();";
  webpage += "    chart.data.labels.push(now);";
  webpage += "    chart.data.datasets[0].data.push(data.temperature);";
  webpage += "    chart.data.datasets[1].data.push(data.pwmValue);";
  webpage += "    if (chart.data.labels.length > 20) {";
  webpage += "      chart.data.labels.shift();";
  webpage += "      chart.data.datasets[0].data.shift();";
  webpage += "      chart.data.datasets[1].data.shift();";
  webpage += "    }";
  webpage += "    chart.update();";
  webpage += "    console.log('Chart updated');"; // Mesaj de depanare
  webpage += "  } else {";
  webpage += "    console.log('Chart not initialized');"; // Mesaj de depanare
  webpage += "  }";
  webpage += "}, false);";
  webpage += "function sendTemperature() {";
  webpage += "  var temperature = document.getElementById('temperature').value;";
  webpage += "  var xhr = new XMLHttpRequest();";
  webpage += "  xhr.onreadystatechange = function() {";
  webpage += "    if (xhr.readyState === XMLHttpRequest.DONE) {";
  webpage += "      if (xhr.status === 200) {";
  webpage += "        document.getElementById('temperatureNotification').innerHTML = xhr.responseText;";
  webpage += "      }";
  webpage += "    }";
  webpage += "  };";
  webpage += "  xhr.open('GET', '/set_temperature?temperature=' + temperature, true);";
  webpage += "  xhr.send();";
  webpage += "}";
  webpage += "</script>";
  webpage += "</head><body onload='initChart()'><h1>AquaTemp Control</h1>";
  webpage += "<table><tr><th>Masuratoare</th><th>Valoare</th></tr>";
  webpage += "<tr><td>Temperatura</td><td><span id=\"temperatureValue\" class=\"sensor\">" + String(temperature, 2) + "</span> &deg;C</td></tr>";
  webpage += "<tr><td>Valoare TDS</td><td><span id=\"tdsValue\" class=\"sensor\">" + String(tdsValue, 0) + "</span> ppm</td></tr>";
  webpage += "<tr><td>PWM</td><td><span id=\"pwmValue\" class=\"sensor\">" + String(pwmValue, 2) + "</span></td></tr>";
  webpage += "</table>";
  webpage += "<br><label for='temperature'>Temperatura de referinta:</label>";
  webpage += "<input type='number' id='temperature' name='temperature' step='0.1' value='" + String(setpoint, 1) + "'>";
  webpage += "<button onclick='sendTemperature()'>Trimite</button>";
  webpage += "<div id='temperatureNotification'></div>";
  webpage += "<p>Procentul de atingere a temperaturii de referinta: <span id='temperaturePercentage'>" + String(temperaturePercentage, 2) + "</span>%</p>";
  webpage += "<canvas id='myChart' width='400' height='200'></canvas>";
  webpage += "</body></html>";
  return webpage;
}