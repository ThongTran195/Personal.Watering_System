#include <Arduino.h>
#include "RTClib.h"
#include "ADS1X15.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <time.h>


const char* ssid     = "Hung_Thong";
const char* password = "Hungthong123@";

const char* timezone = "<+07>-7";



// Time that the daily task runs in 24 hour format
const int taskHour = 2;   // Hour example in 24 hour format: 16 = 4 PM
const int taskMinute = 45;  // 5 minutes

// Store the day when the task last ran to ensure it only runs once per day
int lastRunDay = -1;

unsigned long lastNTPUpdate = 0; // Timestamp for the last NTP sync
const unsigned long ntpSyncInterval = 30 * 60 * 1000; // Sync every 30 minutes (in ms)

void syncTime() {
  Serial.print("Synchronizing time with NTP server...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC offset set to 0
  time_t now = time(nullptr);
  while (now < 24 * 3600) { // Wait until time is valid
    delay(100);
    now = time(nullptr);
  }
  Serial.println(" Time synchronized!");
  
  // Set timezone
  setenv("TZ", timezone, 1);
  tzset();

  lastNTPUpdate = millis(); // Record the time of the last sync
}







#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);




#define SEALEVELPRESSURE_HPA (1013.25)
RTC_DS3231 rtc;
ADS1115 ADS(0x48);
Adafruit_BME280 bme;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};





volatile bool RDY = false;
uint8_t channel = 0;
int16_t val[4] = { 0, 0, 0, 0 };




int rain_sensor = 18; //  GPIO pin for rain sensor
int pump_pin = 17; //  GPIO pin for pump control

void set_time_now();
void sensor_measurement(); 
void get_temp_moisture_pressure();
void dailyTask(); 
int get_rain_sensor(); 

int get_rain_sensor()
{
  int rain = digitalRead(rain_sensor);
  if (rain == LOW) {
    Serial.println("Rain detected!");
    return 1; // Rain detected
  } else {
    Serial.println("No rain detected.");
    return 0; // No rain
  }
}



void sensor_measurement(int x)
{
  const int AirValue = 12500;

  ADS.setGain(1);

  int16_t val = ADS.readADC(x); 

  int moisture_amount =(18303 - val) /10.0; //  convert to percentage
  if (moisture_amount < 0) moisture_amount = 0; //  clamp to 0
  if (moisture_amount > 100) moisture_amount = 100; //  clamp to 100


  float f = ADS.toVoltage(1);  //  voltage factor

  Serial.print("\tAnalog: "); Serial.print(x);Serial.print(val); Serial.print('\t'); //Serial.println(val * f, 3);
  Serial.print("\tMoisture: "); Serial.print(moisture_amount); Serial.print("%\t");
  Serial.println();
}



void get_time_now()
{
  DateTime now = rtc.now();
  
  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC); 
  String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
  String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  String dayOfWeek = daysOfTheWeek[now.dayOfTheWeek()];

  
  String formattedTime = dayOfWeek + ", " + yearStr + "-" + monthStr + "-" + dayStr + " " + hourStr + ":" + minuteStr + ":" + secondStr;

  Serial.println(formattedTime);

  Serial.print(rtc.getTemperature());
  Serial.println("ºC");

  Serial.println();
}


void get_temp_moisture_pressure()
{
  Serial.print("Temperature = ");
  Serial.print(bme.readTemperature());
  Serial.println(" *C");
  //  read temperature in Celsius 
  
  Serial.print("Pressure = ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.println(" hPa");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.print("Humidity = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.println();
}


int displayScreenNum = 0;
int displayScreenNumMax = 3;


unsigned long lastTimer = 0;
unsigned long timerDelay = 15000;

unsigned char temperature_icon[] ={
  0b00000001, 0b11000000, //        ###      
  0b00000011, 0b11100000, //       #####     
  0b00000111, 0b00100000, //      ###  #     
  0b00000111, 0b11100000, //      ######     
  0b00000111, 0b00100000, //      ###  #     
  0b00000111, 0b11100000, //      ######     
  0b00000111, 0b00100000, //      ###  #     
  0b00000111, 0b11100000, //      ######     
  0b00000111, 0b00100000, //      ###  #     
  0b00001111, 0b11110000, //     ########    
  0b00011111, 0b11111000, //    ##########   
  0b00011111, 0b11111000, //    ##########   
  0b00011111, 0b11111000, //    ##########   
  0b00011111, 0b11111000, //    ##########   
  0b00001111, 0b11110000, //     ########    
  0b00000111, 0b11100000, //      ######     
};

unsigned char humidity_icon[] ={
  0b00000000, 0b00000000, //                 
  0b00000001, 0b10000000, //        ##       
  0b00000011, 0b11000000, //       ####      
  0b00000111, 0b11100000, //      ######     
  0b00001111, 0b11110000, //     ########    
  0b00001111, 0b11110000, //     ########    
  0b00011111, 0b11111000, //    ##########   
  0b00011111, 0b11011000, //    ####### ##   
  0b00111111, 0b10011100, //   #######  ###  
  0b00111111, 0b10011100, //   #######  ###  
  0b00111111, 0b00011100, //   ######   ###  
  0b00011110, 0b00111000, //    ####   ###   
  0b00011111, 0b11111000, //    ##########   
  0b00001111, 0b11110000, //     ########    
  0b00000011, 0b11000000, //       ####      
  0b00000000, 0b00000000, //                 
};

unsigned char arrow_down_icon[] ={
  0b00001111, 0b11110000, //     ########    
  0b00011111, 0b11111000, //    ##########   
  0b00011111, 0b11111000, //    ##########   
  0b00011100, 0b00111000, //    ###    ###   
  0b00011100, 0b00111000, //    ###    ###   
  0b00011100, 0b00111000, //    ###    ###   
  0b01111100, 0b00111110, //  #####    ##### 
  0b11111100, 0b00111111, // ######    ######
  0b11111100, 0b00111111, // ######    ######
  0b01111000, 0b00011110, //  ####      #### 
  0b00111100, 0b00111100, //   ####    ####  
  0b00011110, 0b01111000, //    ####  ####   
  0b00001111, 0b11110000, //     ########    
  0b00000111, 0b11100000, //      ######     
  0b00000011, 0b11000000, //       ####      
  0b00000001, 0b10000000, //        ##       
};

unsigned char sun_icon[] ={
  0b00000000, 0b00000000, //                 
  0b00100000, 0b10000010, //   #     #     # 
  0b00010000, 0b10000100, //    #    #    #  
  0b00001000, 0b00001000, //     #       #   
  0b00000001, 0b11000000, //        ###      
  0b00000111, 0b11110000, //      #######    
  0b00000111, 0b11110000, //      #######    
  0b00001111, 0b11111000, //     #########   
  0b01101111, 0b11111011, //  ## ######### ##
  0b00001111, 0b11111000, //     #########   
  0b00000111, 0b11110000, //      #######    
  0b00000111, 0b11110000, //      #######    
  0b00010001, 0b11000100, //    #   ###   #  
  0b00100000, 0b00000010, //   #           # 
  0b01000000, 0b10000001, //  #      #      #
  0b00000000, 0b10000000, //         #       
};




// Create display marker for each screen
void displayIndicator(int displayNumber) {
  int xCoordinates[4] = {44, 54, 64, 74};
  for (int i =0; i<4; i++) {
    if (i == displayNumber) {
      display.fillCircle(xCoordinates[i], 60, 2, WHITE);
    }
    else {
      display.drawCircle(xCoordinates[i], 60, 2, WHITE);
    }
  }
}

//SCREEN NUMBER 0: DATE AND TIME
void displayLocalTime(){

  DateTime now = rtc.now();

  
  String yearStr = String(now.year(), DEC);
  String monthStr = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String dayStr = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  String hourStr = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC); 
  String minuteStr = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
  String secondStr = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  String dayOfWeek = daysOfTheWeek[now.dayOfTheWeek()];


  //Display Date and Time on OLED display
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(19,5);
  display.print(hourStr);
  display.print(":");
  display.print(minuteStr);
  display.setTextSize(1);
  display.setCursor(16,40);
  display.print(dayOfWeek);
  display.print(", ");
  display.print(dayStr);
  display.print(" ");
  display.print(monthStr);
  display.print(" ");
  display.print(yearStr);
  displayIndicator(displayScreenNum);
  display.display();
}

// SCREEN NUMBER 1: TEMPERATURE
void displayTemperature(){
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(15, 5, temperature_icon, 16, 16 ,1);
  display.setCursor(35, 5);
  float temperature = bme.readTemperature();
  display.print(temperature);
  display.cp437(true);
  display.setTextSize(1);
  display.print(" ");
  display.write(167);
  display.print("C");
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Humidity: ");
  display.print(bme.readHumidity());
  display.print(" %");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Pressure: ");
  display.print(bme.readPressure()/100.0F);
  display.print(" hpa");
  displayIndicator(displayScreenNum);
  display.display();
  
  
}

// SCREEN NUMBER 2: HUMIDITY
void displayHumidity(){
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(15, 5, humidity_icon, 16, 16 ,1);
  display.setCursor(35, 5);
  float humidity = bme.readHumidity();
  display.print(humidity);
  display.print(" %");
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Temperature: ");
  display.print(bme.readTemperature());
  display.cp437(true);
  display.print(" ");
  display.write(167);
  display.print("C");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Pressure: ");
  display.print(bme.readPressure()/100.0F);
  display.print(" hpa");
  displayIndicator(displayScreenNum);
  display.display();
  
}

// SCREEN NUMBER 3: PRESSURE
void displayPressure(){
  display.clearDisplay();
  display.setTextSize(2);
  display.drawBitmap(0, 5, arrow_down_icon, 16, 16 ,1);
  display.setCursor(20, 5);
  display.print(bme.readPressure()/100.0F);
  display.setTextSize(1);
  display.print(" hpa");
  display.setCursor(0, 34);
  display.setTextSize(1);
  display.print("Temperature: ");
  display.print(bme.readTemperature());
  display.cp437(true);
  display.print(" ");
  display.write(167);
  display.print("C");
  display.setCursor(0, 44);
  display.setTextSize(1);
  display.print("Humidity: ");
  display.print(bme.readHumidity());
  display.print(" hpa");
  displayIndicator(displayScreenNum);
  display.display();
  
}



// Display the right screen accordingly to the displayScreenNum
void updateScreen() {
  if (displayScreenNum == 0){
    displayLocalTime();
  }
  else if (displayScreenNum == 1) {
    displayTemperature();
  }
  else if (displayScreenNum ==2){
    displayHumidity();
  }
  else{
    displayPressure();
  }
}






void setup () {
  Serial.begin(115200);


Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");

  syncTime();


  pinMode(rain_sensor, INPUT_PULLUP); //  rain sensor pin as input with pull-up resistor
  pinMode(pump_pin, OUTPUT); //  pump control pin as output
  digitalWrite(pump_pin, LOW); //  ensure pump is off at startup

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    

  
}

  
Serial.println(__FILE__);
  Serial.print("ADS1X15_LIB_VERSION: ");
  Serial.println(ADS1X15_LIB_VERSION);

  Wire.begin();
  ADS.begin();

  bme.begin(0x76); 

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    for (;;);
  }

  // Clear the display
  display.clearDisplay();
  display.setTextColor(WHITE); 

}

void loop () {

  




  if ((millis() - lastTimer) > timerDelay) {
    updateScreen();
    Serial.println(displayScreenNum);
    if(displayScreenNum < displayScreenNumMax) {
      displayScreenNum++;
    }
    else {
      displayScreenNum = 0;
    }
    lastTimer = millis();
  }
  
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Current time and date

  // Check if it's time to run the daily task
  if (timeinfo.tm_hour == taskHour && timeinfo.tm_min == taskMinute && lastRunDay != timeinfo.tm_mday) {
    dailyTask();
    // Set the day to ensure it only runs once per day
    lastRunDay = timeinfo.tm_mday;
  }

  // Resynchronize with NTP every 30 minutes
  if (millis() - lastNTPUpdate > ntpSyncInterval) {
    syncTime();
  }
  
}


unsigned long previousMillis = 0; 

const long interval = 1000;  

void dailyTask() {
  Serial.println("#########\nDoing daily task...\n#########");

  digitalWrite(pump_pin, HIGH); //  turn on the pump
  delay(5000); //  run the pump for 5 seconds
  digitalWrite(pump_pin, LOW); //  turn off the pump
  Serial.println("Pump turned off.");
  
}

