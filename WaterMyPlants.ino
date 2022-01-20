// Code can be downloaded to a ESP32
// ESP32,  developed by Espressif Systems,  is a series of low-cost, low-power system on a chip microcontrollers with integrated Wi-Fi
// Assumes there are 3 pumps

#include <EEPROM.h>
#include <NTPClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
#include <WiFi.h>
#include "HTTPClient.h"


// Sleep settings
const int uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */

// Wifi settings
const char* ssid = "INSERT_SSID";
const char* pwd  = "INSERT_SSID_PWD";
unsigned long check_wifi = 30000;

// Pump URL
String WEATHER_API = "INSERT_WEATHER_API";

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600 * 8);

// Digital Pin
const int ledPin1 = 27; // IN1
const int ledPin2 = 33; // IN2
const int ledPin3 = 15; // IN3
int pins[3] = {ledPin1, ledPin2, ledPin3};
int nopins = sizeof(pins)/sizeof(int);

// Water settings
const int WATER_DURATION = 3000;
const int SWITCH_DELAY = 500;
int WATERING_FREQUENCY = 20;  // seconds // 86400;
int WATERING_FREQUENCY_MAX = 100;


int PUMP_1 = WATER_DURATION;
int PUMP_2 = WATER_DURATION;
int PUMP_3 = WATER_DURATION;
int PUMPS[3] = {PUMP_1, PUMP_2, PUMP_3};
int nopumps = sizeof(PUMPS)/sizeof(int);

// EEPROM Address
const int ADDRESS_YEAR = 0;
const int ADDRESS_MONTH = 2;
const int ADDRESS_DAY = 4;
const int ADDRESS_HOUR = 6;
const int ADDRESS_MIN = 8;
const int ADDRESS_SEC = 10;

const int ADDRESS_PUMP_1 = 12;
const int ADDRESS_PUMP_2 = 14;
const int ADDRESS_PUMP_3 = 16;

const int ADDRESS_WATERING_FREQUENCY = 18;

const int EEPROM_SIZE = 128;


// Helper functions
void EEPROMWritelong(int address, int value)
{
  byte second = (value & 0xFF);
  byte first = ((value >> 8) & 0xFF);
  
  EEPROM.write(address, second);
  EEPROM.commit();
  EEPROM.write(address + 1, first);
  EEPROM.commit();   
}

int EEPROMReadlong(int address)
{
  byte second = EEPROM.read(address);
  byte first = EEPROM.read(address + 1);

  return ((second << 0) & 0xFF) + ((first << 8) & 0xFFFF);
}

void UpdateEEPROM(struct tm * curr_time) 
{
  EEPROMWritelong(ADDRESS_YEAR, curr_time->tm_year);
  EEPROMWritelong(ADDRESS_MONTH, curr_time->tm_mon);
  EEPROMWritelong(ADDRESS_DAY, curr_time->tm_mday);

  EEPROMWritelong(ADDRESS_HOUR, curr_time->tm_hour);
  EEPROMWritelong(ADDRESS_MIN, curr_time->tm_min);
  EEPROMWritelong(ADDRESS_SEC, curr_time->tm_sec);

  // Pumps
  EEPROMWritelong(ADDRESS_PUMP_1, PUMPS[0]);
  EEPROMWritelong(ADDRESS_PUMP_2, PUMPS[1]);
  EEPROMWritelong(ADDRESS_PUMP_3, PUMPS[2]);

  Serial.println("-------------------------------------------");
  Serial.printf("Writing Pumps to EEPROM - %d:%d:%d\n", PUMPS[0], PUMPS[1], PUMPS[2]);
  Serial.println("-------------------------------------------");
  
  // WATERING_FREQUENCY
  EEPROMWritelong(ADDRESS_WATERING_FREQUENCY, WATERING_FREQUENCY);
  Serial.println("-------------------------------------------");
  Serial.printf("Writing WATERING_FREQUENCY to EEPROM - %d\n", WATERING_FREQUENCY);
  Serial.println("-------------------------------------------");
}


void readEEPROM(struct tm * prev_time) 
{
  memset(prev_time, 0, sizeof(*prev_time));
  
  // Read EEPROM
  prev_time->tm_year = EEPROMReadlong(ADDRESS_YEAR);
  prev_time->tm_mon = EEPROMReadlong(ADDRESS_MONTH);
  prev_time->tm_mday = EEPROMReadlong(ADDRESS_DAY);

  prev_time->tm_hour = EEPROMReadlong(ADDRESS_HOUR);
  prev_time->tm_min = EEPROMReadlong(ADDRESS_MIN);
  prev_time->tm_sec = EEPROMReadlong(ADDRESS_SEC);

  // PUMPS
  PUMPS[0] = EEPROMReadlong(ADDRESS_PUMP_1);
  PUMPS[1] = EEPROMReadlong(ADDRESS_PUMP_2);
  PUMPS[2] = EEPROMReadlong(ADDRESS_PUMP_3);  

  Serial.println("-------------------------------------------");
  Serial.printf("Reading Pumps from EEPROM - %d:%d:%d\n", PUMPS[0], PUMPS[1], PUMPS[2]);
  Serial.println("-------------------------------------------");

  // WATERING_FREQUENCY
  WATERING_FREQUENCY = EEPROMReadlong(ADDRESS_WATERING_FREQUENCY);
  if (WATERING_FREQUENCY > WATERING_FREQUENCY_MAX)
     WATERING_FREQUENCY = WATERING_FREQUENCY_MAX;
  Serial.println("-------------------------------------------");
  Serial.printf("Reading Watering Frequency from EEPROM - %d\n", WATERING_FREQUENCY);
  Serial.println("-------------------------------------------");
}


int getDiffTimeSeconds(struct tm * curr_time, struct tm * prev_time)
{ 
  time_t rawtime = timeClient.getEpochTime();
  curr_time = localtime(&rawtime);

  readEEPROM(prev_time);  

  time_t start_time, end_time;
  start_time = mktime(prev_time);
  end_time = mktime(curr_time);
  

  int datediff = difftime(end_time, start_time);

  return datediff;
}

bool timeToWater(struct tm * curr_time)
{

  Serial.println("timeToWater");

  Serial.println("timeClient.update()");
  
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  
  Serial.println("After timeClient.update()");
  
  struct tm prev_time;
  int datediff = getDiffTimeSeconds(curr_time, &prev_time);


  Serial.println("*********************** WATERING INFO **********************");
   Serial.printf("                Current date : %d-%02d-%02d %02d:%02d:%02d\n", curr_time->tm_year+1900, curr_time->tm_mon+1, curr_time->tm_mday, curr_time->tm_hour, curr_time->tm_min, curr_time->tm_sec);
   Serial.printf("         Last recorded water : %d-%02d-%02d %02d:%02d:%02d\n", prev_time.tm_year+1900, prev_time.tm_mon+1, prev_time.tm_mday, prev_time.tm_hour, prev_time.tm_min, prev_time.tm_sec);
   Serial.printf("Elapsed time since (seconds) : %d\n", datediff);
  Serial.println("************************************************************");


  // If EEPROM has no info
  if (datediff < 0) {
    return true;
  }

  // If time elapsed was more than a day ago
  if (datediff > WATERING_FREQUENCY) {
    return true;
  }

  return false;
}



// String handling
struct s_list {
  char arr[10][100];
  int i;
};

struct s_list parse(char* str, char* delim)
{
   char temp_str[100];
   strcpy(temp_str, str);

   s_list list;

   char* token = strtok(temp_str, delim);
   int i = 0;
   while (token)
   {
       strcpy(list.arr[i], token);
       token = strtok(NULL, delim);
       i = i + 1;
   }

   // store the actual size
   list.i=i;

   return list;
}



// Store info on whether pumps are active
// Store watering frequency
void Store(struct s_list list2)
{
  for (int i=0; i<list2.i; i++) {
   
    if (strcmp(list2.arr[i],"pump1")==0) {
        PUMPS[0] = atoi(list2.arr[i+1]);
        Serial.printf("pumps0 = %d\n", PUMPS[0]);
    }
    if (strcmp(list2.arr[i],"pump2")==0) {
        PUMPS[1] = atoi(list2.arr[i+1]);
        Serial.printf("pumps1 = %d\n", PUMPS[1]);
    }
    if (strcmp(list2.arr[i],"pump3")==0) {
        PUMPS[2] = atoi(list2.arr[i+1]);
        Serial.printf("pumps2 = %d\n", PUMPS[2]);
    }
    if (strcmp(list2.arr[i],"watering_frequency")==0) {
        WATERING_FREQUENCY = atoi(list2.arr[i+1]);
        if (WATERING_FREQUENCY > WATERING_FREQUENCY_MAX)
            WATERING_FREQUENCY = WATERING_FREQUENCY_MAX;
        Serial.printf("watering freq = %d\n", WATERING_FREQUENCY);
    }

    i=i+1;
  }
}

// Stores key value pairs
void StoreInfo(char* payload) 
{
  char* eoline = "\r\n";
  char* delim = "=";
  char str[100] = "";
  strcpy(str, payload);
  struct s_list list = parse(str, eoline);
  
  Serial.println("\n===StoreInfo===\n");

  s_list list2;
  for (int i=0; i<list.i; i++) {
      list2 = parse(list.arr[i], delim);
      Store(list2);
  }
  
  Serial.println("\n===StoreInfo===\n");
}




// Main program
void setup() {
  // No Code before this line
  // **** DO NOT REMOVE ****    
  Serial.begin(115200);  
  delay(3000);
  // **************************************************
  // Do not modify above
  // **************************************************


  Serial.println("+----------------+");
  Serial.println("| Entering setup |");
  Serial.println("+----------------+");


  WiFi.begin(ssid, pwd);

}

void Init() {

    // setup pins as digital output pins
    for (int a=0; a<3; a++) {
      pinMode (pins[a], OUTPUT);
    }
  
    timeClient.begin();
  
  
    // +==================================
    // | EEPROM INITIALIZATION 
    // | No initialization above this line
    // +==================================
    if (!EEPROM.begin(EEPROM_SIZE))
    {
      Serial.println("> Failed to initialise EEPROM");
    }
    else 
    {
      Serial.println("> EEPROM initialized successfully!");
    }
  
    // **** DO NOT REMOVE ****
    // Need to initialize to high
    for (int a=0; a<3; a++) {
       digitalWrite (pins[a], HIGH);
    }
    // **** DO NOT REMOVE ****  
}


// Main Loop
void loop() 
{
    Serial.println("+---------------+");
    Serial.println("| Entering loop |");
    Serial.println("+---------------+");

    while (WiFi.status() !=  WL_CONNECTED) {
      Serial.println("looping no wifi yet...");
      delay(5000);
    }
    
    Serial.println("WiFi connected...");


    Serial.println("Init...");
    
    Init();

    
    // get pump settings
    Serial.println("Getting pump settings....");

    // Get Current Time      
    time_t t = time(NULL);
    struct tm * curr_time = localtime(&t);
    

    
    // HTTPClient    
    HTTPClient http;
    http.begin(WEATHER_API);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StoreInfo((char*)payload.c_str());

      // Update EEPROM
      UpdateEEPROM(curr_time);
    }
    else {        
      Serial.printf("HTTP Error Code = %s", http.errorToString(httpCode));
    }
    http.end();
    

   
    Serial.println("> About to water plants...");
    if (timeToWater(curr_time)) {  

      Serial.println("> Watering plants...");
      
      for (int a=0; a<nopins; a++) {

        Serial.printf("> Activating switch for %d ms : %d\n", PUMPS[a], a+1);

        // **** Low first then High ****
        digitalWrite (pins[a], LOW);
        delay(PUMPS[a]);
        digitalWrite (pins[a], HIGH);
        delay(SWITCH_DELAY);
        // *****************************
      }
      
      Serial.println("> Watered plants...");

      UpdateEEPROM(curr_time);
    }
    else 
    {
      Serial.println("> Not time yet to water plants...");
    }

    /*Serial.println("HTTP end...");
    http.end();*/

    // go to sleep    
    Serial.printf("> Going to deep sleep for %d seconds...", WATERING_FREQUENCY);
    esp_sleep_enable_timer_wakeup(WATERING_FREQUENCY * uS_TO_S_FACTOR);
    esp_deep_sleep_start();

  
}
