// Seeeduino LoRaWAN ------------------------------------------------------------
#define PIN_GROVE_POWER 38
#define SerialUSB Serial
 
// LoRaWAN -----------------------------------------------------------------------
#include <LoRaWan.h>

//air quality sensor
#include "Air_Quality_Sensor.h"
AirQualitySensor aqsensor(A0);

//Temperature Sensor
#include "DHT.h"
#include <Wire.h>
#include "rgb_lcd.h"
#define DHTTYPE DHT22   // DHT 22  (AM2302)
#define DHTPIN A2     // what pin we're connected to（DHT10 and DHT20 don't need define it）
DHT dht(DHTPIN, DHTTYPE);   //   DHT11 DHT21 DHT22

//Particulate Matter PM Sensor
#include <Seeed_HM330X.h>
HM330X sensor;
uint8_t buf[30];

const char* str[] = 
{"sensor num: ", "PM1.0 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
"PM2.5 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
"PM10 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
"PM1.0 concentration(Atmospheric environment,unit:ug/m3): ",
"PM2.5 concentration(Atmospheric environment,unit:ug/m3): ",
"PM10 concentration(Atmospheric environment,unit:ug/m3): ",
};


HM330XErrorCode print_result(const char* str, uint16_t value) {
  if (NULL == str) {
  return ERROR_PARAM;
  }
  Serial.print(str);
  Serial.println(value);
  return NO_ERROR;
}

void parse_result(uint8_t* data, uint8_t* values) {
  uint8_t value;
  if (NULL == data) {
    Serial.println("Error occured in PM Sensor");
  }
  for (int i = 1; i < 8; i++) {
    value = (uint8_t) data[i * 2] << 8 | data[i * 2 + 1];
    if(i>4){
      values[i-5] = value;
    }
  }
}



HM330XErrorCode parse_result_value(uint8_t* data) {
    if (NULL == data) {
        return ERROR_PARAM;
    }
    // for (int i = 0; i < 28; i++) {
    //     Serial.print(data[i], HEX);
    //     Serial.print("  ");
    //     if ((0 == (i) % 5) || (0 == i)) {
    //         Serial.println("");
    //     }
    // }
    uint8_t sum = 0;
    for (int i = 0; i < 28; i++) {
        sum += data[i];
    }
    if (sum != data[28]) {
        Serial.println("wrong checkSum!!");
    }
    Serial.println("");
    return NO_ERROR;
}

 //Temperature, Pressure, Humidity and Gas sensor
 #include "seeed_bme680.h"
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define IIC_ADDR  uint8_t(0x76)
Seeed_BME680 bme680(IIC_ADDR); /* IIC PROTOCOL */

// Put your LoRa keys here - Pradeesh
#define DevEUI ""   //defining keys for controller, where data goes to
#define AppEUI ""
#define AppKey ""

// CayenneLPP --------------------------------------------------------------------
#include <CayenneLPP.h>  // Include Cayenne Library
CayenneLPP lpp(51);      // Define the buffer size: Keep as small as possible
 // we should calculate the size, ex: 6 bits. 51 is the number of bytes.
// SETUP -------------------------------------------------------------------------

// vars
rgb_lcd lcd;
const int colorR = 0;
const int colorG = 255;
const int colorB = 0;

char buffer[256];
void setup(void)
{
//LCD
Wire.begin();
lcd.begin(16, 2);  
lcd.setRGB(colorR, colorG, colorB);
lcd.print("Monitoring...");

// Setup Serial connection
Serial.begin(115200);

// Powerup Seeeduino LoRaWAN Grove connectors
pinMode(PIN_GROVE_POWER, OUTPUT);
digitalWrite(PIN_GROVE_POWER, 1);

// Config LoRaWAN
lora.init();

memset(buffer, 0, 256);
lora.getVersion(buffer, 256, 1);
if (Serial) {
  Serial.print(buffer);
}

memset(buffer, 0, 256);
lora.getId(buffer, 256, 1);
if (Serial) {
  Serial.print(buffer);
}

lora.setId(NULL, DevEUI, AppEUI);
lora.setKey(NULL, NULL, AppKey);
lora.setDeciveMode(LWOTAA);
lora.setDataRate(DR0, EU868);      // DR5 = SF7, DR0 = SF 12
lora.setAdaptiveDataRate(true);

//Setting channels for data transfer
lora.setChannel(0, 868.1);
lora.setChannel(1, 868.3);
lora.setChannel(2, 868.5);
lora.setChannel(3, 867.1);
lora.setChannel(4, 867.3);
lora.setChannel(5, 867.5);
lora.setChannel(6, 867.7);
lora.setChannel(7, 867.9);

lora.setDutyCycle(false);
lora.setJoinDutyCycle(false);
lora.setPower(14);
lora.setPort(33);

//Joining LoRaWan network
unsigned int nretries;
nretries = 0;
while (!lora.setOTAAJoin(JOIN, 20)) {
  nretries++;
  if (Serial) {
    Serial.println((String)"Join failed, retry: " + nretries);
  }
}
Serial.println("Join successful!");


while (!Serial);
Serial.println("Waiting sensor to init...");
delay(2000);

// Initilizing air quality sensor
if (aqsensor.init()) {
  Serial.println("Sensor ready.");
} else {
  Serial.println("Sensor ERROR!");
}

// Initilizing Temperature, Pressure, Humidity and Gas sensor
while (!bme680.init()) {
  Serial.println("bme680 init failed ! can't find device!");
  delay(1000);
}

// Initilizing Particulate Matter PM Sensor
if (sensor.init()) {
  Serial.println("HM330X init failed!!");
  while (1);
}


delay(1000);
dht.begin();
}

// LOOP --------------------------------------------------------------------
unsigned int nloops = 0;
void loop(void) {
  

  // Turn off the display:
  lcd.noDisplay();
  
  if (!bme680.read_sensor_data() )
  {
    //air quality
  int quality = aqsensor.slope();
  int airQualityValue = aqsensor.getValue();
  String airPollutionValue;
  int airpollutionNumber ;

  // The air pollution readings are mapped to an integer for ease of transmission through cayenne protocol
  if (quality == AirQualitySensor::FORCE_SIGNAL) {
    airPollutionValue = "High pollution! Force signal active.";
    airpollutionNumber = 1;
  } else if (quality == AirQualitySensor::HIGH_POLLUTION) {
    airPollutionValue = "High pollution!";
    airpollutionNumber = 2;
  } else if (quality == AirQualitySensor::LOW_POLLUTION) {
    airPollutionValue = "Low pollution!";
    airpollutionNumber = 3;
  } else if (quality == AirQualitySensor::FRESH_AIR) {
    airPollutionValue = "Fresh air.";
    airpollutionNumber = 4;
  }

  //Temperature, Pressure, Humidity and Gas sensor
  float temperature = bme680.sensor_result_value.temperature;
  float pressure = bme680.sensor_result_value.pressure / 1000.0;
  float humidity = bme680.sensor_result_value.humidity;
  float gas = bme680.sensor_result_value.gas / 1000.0;


  if (sensor.read_sensor_value(buf, 29)) {
    Serial.println("HM330X read result failed!!");
  }
  // parse_result_value and parse_result are functions that parse the sensor readings into 'ppb' values
  parse_result_value(buf);
  uint8_t values[3]; // An array to store 3 values, for PM1, PM2.5 and PM10 respectively
  parse_result(buf,values);

  bool result = false;
  // Reset Cayenne buffer and add new data
  lpp.reset();                             // Resets the Cayenne buffer
  lpp.addTemperature(1, temperature);
  lpp.addRelativeHumidity(2,humidity);           // encodes the temperature value 22.5 on channel 1 in Cayenne format. Pick an integer between 1-100 and remember which integer corresponds to which data type
  lpp.addAnalogOutput(3, pressure);
  lpp.addAnalogOutput(4, gas);
  lpp.addAnalogOutput(5, values[0]);  //PM 1
  lpp.addAnalogOutput(6, values[1]);  //PM 2.5
  lpp.addAnalogOutput(7, values[2]); // PM 10
  lpp.addAnalogOutput(8,quality);
  lpp.addAnalogOutput(9,airpollutionNumber);
  result = lora.transferPacket(lpp.getBuffer(), lpp.getSize(), 5);                  // sends the Cayenne encoded data packet (n bytes) with a default timeout of 5 secs

  if (result) {
    short length;
    short rssi;

    // Receive LoRaWAN package (LoraWAN Class A)
    char rx[256];
    length = lora.receivePacket(rx, 256, &rssi);

    // Check, if a package was received
    if (length)
    {
      if (Serial) {
        Serial.print("Length is: ");
        Serial.println(length);
        Serial.print("RSSI is: ");
        Serial.println(rssi);
        Serial.print("Data is: ");

        // Print received data as HEX
        for (unsigned char i = 0; i < length; i ++)
        {
          Serial.print("0x");
          Serial.print(rx[i], HEX);
          Serial.print(" ");
        }

        // Convert received package to int
        int rx_data_asInteger = atoi(rx);

        Serial.println();
        Serial.println("Received data: " + String(rx_data_asInteger));
      }
    }
  }
    
  nloops++;
  if (Serial) {
    int eval = 0;   //to evaluate functions
    Serial.println((String)"Temperature: " + temperature +"C° "+ "Pressure: " + pressure+" KPa" + ", Relative Humidity: "+humidity + " %" + "Gas: "+ gas+" Kohms"+" \n");
    Serial.println((String)"PM1.0 concentration(Atmospheric environment,unit:ug/m3): "+ values[0]+",PM2.5 concentration(Atmospheric environment,unit:ug/m3): "+ values[1]+",PM10 concentration(Atmospheric environment,unit:ug/m3): "+  values[2]+",\n");
    Serial.println((String)"Air quality sensor value: "+ airQualityValue +", Rating: " + airpollutionNumber + " Pollution Status: "+airPollutionValue+" \n");
    Serial.println((String)"Loop " + nloops + "...done!\n");
    // In each loop the readings of a single sensor is displayed in sequence so to not delay the loop process
    //Display the Temperature, Pressure, Humidity and Gas Sensor values in the LCD Display
    if(nloops % 3 == 0){
    lcd.setCursor(0, 0);
    lcd.print("T:");
    int t1 = (int) temperature;
    lcd.print(t1);
    lcd.print("C RH:");
    lcd.print(humidity);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("P:");
    int p1 = (int) pressure;
    lcd.print(p1);
    lcd.print("KPa VOC:");
    int g1 = (int) gas;
    float g2 =  (gas-g1)*10;
    int g3 = (int) g2;
    lcd.print(g1);
    lcd.print(".");
    lcd.print(g3);
    delay(1500);
    }
    //Display the Particulate Matter Sensor readings in the LCD Display
    else if (nloops % 2 == 0){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PM1:");
      lcd.print(values[0]);
      lcd.setCursor(0, 1);
      lcd.print("PM2.5:");
      lcd.print(values[1]);
      lcd.print(" PM10:");
      lcd.print(values[2]);
      delay(1500);
    } 
    //Display the Air Quality Sensor readings in the LCD Display
    else {
    lcd.setCursor(0, 0);
    lcd.print("AQI: ");
    lcd.print(airQualityValue);
    lcd.setCursor(0, 1);
    if(airpollutionNumber == 1 || airpollutionNumber == 2){
      lcd.print("High Pollution!!");
    }
      else if(airpollutionNumber == 3){
      lcd.print("Low Pollution!!");
    }
    else {
      lcd.print("Fresh Air :)");
    }
    }


    //Print Error Message on LCD Screen
    if(temperature > 26 ){
      eval = printError("Temp high");
    } 
    else if (temperature < 16 ){
      eval = printError("Temp low");
    }
    if (values[1] > 15 ){
      eval = printError("High PM2.5");
    }
    if (values[2] > 45 ){
      eval = printError("High PM10");
    }
    if (humidity > 65 ){
      eval = printError("High Humidity");
    }
    else if (humidity < 20 ){
      eval = printError("Low Humidity");
    }
    if ( airpollutionNumber < 3 ){
      eval = printError("High pollution");
    }
  }
  } else {
        Serial.println("Failed to get temperature and humidity value.");
    }
    delay(2000);   // time between two packages,

}

//Function to Print Warning Messages
int printError(String message) {
  // Turn on the display:
  lcd.display();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(" WARNING !!!");
  lcd.setCursor(0,1);
  // printing the error message on the second row
  lcd.print(message);
  delay(1000);
  return 0;
}

