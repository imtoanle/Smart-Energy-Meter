#include <Arduino.h>
#include <ArduinoOTA.h>
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>


#if defined(ESP32)
  #error "Software Serial is not supported on the ESP32"
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "node-1"
#endif

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
  #define PZEM_RX_PIN D5
  #define PZEM_TX_PIN D6
#endif

// #if !defined(PZEM_SERIAL)
// #define PZEM_SERIAL Serial1
// #endif

SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSWSerial);

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "Fat Space"
// WiFi password
#define WIFI_PASSWORD "hoituivoich"
// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "https://influxdb2.fat-space.com"
// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN ""
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG "Fat Space"
// InfluxDB v2 bucket name (Use: InfluxDB UI ->  Data -> Buckets)
#define INFLUXDB_BUCKET "smart-energy-meter"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time: "PST8PDT"
//  Eastern: "EST5EDT"
//  Japanesse: "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "Asia/Ho_Chi_Minh"

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data point
Point sensor1("pzem-004t");
// Point sensor2("pzem-004t");

float voltage;
float current;
float power;
float energy;
float frequency;
float power_factor;
unsigned long last_push = 0;

void setup() {
  Serial.begin(115200);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  // WiFi.setHostname(hostname.c_str());
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Add tags
  sensor1.addTag("device", DEVICE);
  // sensor2.addTag("device", "node-2");

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // client.setWriteOptions(WriteOptions().batchSize(2));

  ArduinoOTA.setHostname("smart-energy-meter-1");
  ArduinoOTA.setPassword("Ludih4J2cTAGpb");
  ArduinoOTA.begin();
}

float getVoltage() {
  voltage = pzem.voltage();
  if(isnan(voltage)){
    Serial.println("Error reading voltage");
  }
  return voltage;
}

float getAmpe() {
  current = pzem.current();
  if(isnan(current)){
    Serial.println("Error reading current");
  }
  return current;
}

float getPower() {
  power = pzem.power();
  if(isnan(power)){
    Serial.println("Error reading power");
  }
  return power;
}

float getFrequency() {
  frequency = pzem.frequency();
  if(isnan(frequency)){
    Serial.println("Error reading frequency");
  }
  return frequency;
}

float getTotalEnergyGenerated() {
  energy = pzem.energy();
  if(isnan(energy)){
    Serial.println("Error reading energy");
  }
  return energy;
}

float getPowerFactor() {
  power_factor = pzem.pf();
  if(isnan(power_factor)){
    Serial.println("Error reading power factor");
  }
  return power_factor;
}

void loadSensor() {
  getVoltage();
  getAmpe();
  getPower();
  getFrequency();
  getTotalEnergyGenerated();
  getPowerFactor();
}

void pushSensorValues() {
  // Clear fields for reusing the point. Tags will remain untouched
  sensor1.clearFields();
  // sensor2.clearFields();

  // Store measured value into point
  // Report RSSI of currently connected network
  sensor1.addField("voltage", voltage);
  sensor1.addField("current", current);
  sensor1.addField("power", power);
  sensor1.addField("frequency", frequency);
  sensor1.addField("total_energy_generated", energy);
  sensor1.addField("power_factor", power_factor);

  // sensor2.addField("voltage", getVoltage());
  // sensor2.addField("current", getAmpe());
  // sensor2.addField("power", getPower());
  // sensor2.addField("frequency", getFrequency());
  // sensor2.addField("total_energy_generated", getTotalEnergyGenerated());
  // sensor2.addField("power_factor", getPowerFactor());

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensor1));
  // Serial.println(client.pointToLineProtocol(sensor2));

  if (!client.writePoint(sensor1)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  ArduinoOTA.handle();

  // If no Wifi signal, try to reconnect it
  if ((WiFi.RSSI() == 0) && (wifiMulti.run() != WL_CONNECTED)) {
    Serial.println("Wifi connection lost");
  }

  loadSensor();

  if (millis() - last_push >= 10000) {
    pushSensorValues();
    last_push = millis();
  }

  //Wait 10s
  // Serial.println("Wait 10s");
  delay(1000);
}