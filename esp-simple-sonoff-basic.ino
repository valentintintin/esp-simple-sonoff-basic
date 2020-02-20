#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#define DEBUG
#define RELAY 12
#define BUTTON 0

const char indexHtml[] PROGMEM = {"<!doctype html><title>PixelRupteur</title><meta content=\"width=device-width,initial-scale=1\" name=viewport><script src=https://code.jquery.com/jquery-3.3.1.min.js></script><script>$(function(){const ESP=\"/\";const interrupteurElement=$(\"#interrupteur\");interrupteurElement.click(function(){$.post(ESP+\"toggle\",function(state){refreshHtml(state);},\"json\").fail(function(){error();});});function getStatus(){$.post(ESP+\"\",function(state){refreshHtml(state);},\"json\").fail(function(){error();});}function refreshHtml(state){interrupteurElement.removeAttr(\"disabled\");if(state){interrupteurElement.removeClass(\"off\");interrupteurElement.addClass(\"on\");interrupteurElement.html(\"Eteindre\");}else{interrupteurElement.removeClass(\"on\");interrupteurElement.addClass(\"off\");interrupteurElement.text(\"Allumer\");}}function error(){interrupteurElement.removeClass(\"on\");interrupteurElement.removeClass(\"off\");interrupteurElement.html(\"Erreur !\");interrupteurElement.attr(\"disabled\",true);}getStatus();setInterval(function(){getStatus()},1000);});</script><style>body{text-align:center;margin:auto}.on{background:green!important}.off{background:red!important}#interrupteur{margin:auto;display:flex;align-items:center;justify-content:center;width:150px;height:150px;border-radius:35%;color:#fff;font-weight:700;font-size:20px;background:gray;border:0}#interrupteur:hover:enabled{font-size:25px;cursor:pointer}#infos{margin-top:200px}#infos a{display:block}</style><h1>PixelRupteur</h1><button autofocus id=interrupteur type=button></button><details id=infos><summary>Autres commandes</summary><a href=/toggle>Basculer (GET et POST)</a><a href=/on>Allumer (GET et POST)</a><a href=/off>Eteindre (GET et POST)</a><a href=/infos>Memoire + Uptime en JSON (GET)</a><a href=/reset>Reset l'ESP (POST)</a><form method=post action=/update enctype=multipart/form-data><input type=file name=update><input type=submit value=Update disabled></form></detail>"};
const char trueStr[] PROGMEM = {"true"};
const char falseStr[] PROGMEM = {"false"};
char page[2048] = "";
char mimeHtml[] = "text/html";
char mimeJson[] = "application/json";
bool relayState = false;
bool shouldReboot = false;

AsyncWebServer server(80);
DNSServer dns;

void setup() {
  Serial.begin(115200);
  Serial.println(F("Start"));

  pinMode(RELAY, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON, INPUT);
  setOn();

  AsyncWiFiManager wifiManager(&server, &dns);
  if (!wifiManager.autoConnect("PixelRupteur")) {
    Serial.println("No connection, reset !");
    ESP.restart();
  }

  server.onNotFound(handleRequest);
  server.on("/on", HTTP_ANY, handleRequestOn);
  server.on("/off", HTTP_ANY, handleRequestOff);
  server.on("/toggle", HTTP_ANY, handleRequestToggle);
  server.on("/infos", HTTP_GET, handleRequestInfos);
  server.on("/reset", HTTP_POST, handleRequestReset);
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest * request) {
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);
  }, [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      Serial.printf("Update Start: %s\n", filename.c_str());
      Update.runAsync(true);
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });
  //server.on("/update", HTTP_POST, handleRequestReset, handleRequestUploadProgram);
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin();

  Serial.println(F("Started"));
}

void loop() {
  if (ESP.getFreeHeap() < 1000) {
    Serial.println(F("No more memory, reset !"));
    delay(100);
    ESP.restart();
  }
  
  if (shouldReboot) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }

  if (!digitalRead(BUTTON)) {
    toggle();
    delay(500);
  }
}

void notFound(AsyncWebServerRequest *request) {
  request->send_P(200, mimeHtml, indexHtml);
}

void handleRequest(AsyncWebServerRequest *request) {
  if (request->method() == HTTP_POST) {
    request->send_P(200, mimeJson, relayState ? trueStr : falseStr);
  } else {
    notFound(request);
  }
}

void handleRequestOn(AsyncWebServerRequest *request) {
  setOn();
  handleRequest(request);
}

void handleRequestOff(AsyncWebServerRequest *request) {
  setOff();
  handleRequest(request);
}

void handleRequestToggle(AsyncWebServerRequest *request) {
  toggle();
  handleRequest(request);
}

void handleRequestInfos(AsyncWebServerRequest *request) {
  sprintf_P(page, PSTR("{\"mem\":%d,\"uptime\":%lu}"), ESP.getFreeHeap(), millis() / 1000);
  request->send(200, mimeJson, page);
}

void handleRequestReset(AsyncWebServerRequest *request) {
  Serial.println(F("Reset"));
  request->send_P(200, mimeJson, Update.hasError() ? falseStr : trueStr);
  delay(100);
  ESP.restart();
}

void setOn() {
  relayState = true;
  digitalWrite(RELAY, HIGH);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println(F("On"));
}

void setOff() {
  relayState = false;
  digitalWrite(RELAY, LOW);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println(F("Off"));
}

void toggle() {
  Serial.println(F("Toggle"));
  if (relayState) {
    setOff();
  } else {
    setOn();
  }
}
