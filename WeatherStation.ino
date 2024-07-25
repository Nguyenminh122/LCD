#include <ESPWiFi.h>
#include <ESPHTTPClient.h>
#include <JsonListener.h> 
#include <time.h>                       
#include <sys/time.h>                   
#include <coredecls.h>                 
#include <RTClib.h> // Thêm thư viện cho module RTC
#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "DHT.h"
#include "Draw.h"
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

bool adjustingHour = false;
unsigned long lastButtonPress = 0;
int alarmHour = 0;
int alarmMinute = 0;
bool alarmSet = false; // Biến báo thức đã được thiết lập

RTC_DS1307 rtc; // Khai báo đối tượng cho module RTC

const char* WIFI_SSID = "iPhone (48)";
const char* WIFI_PWD = "12345678";

#define TZ              6      
#define DST_MN          60      

const int UPDATE_INTERVAL_SECS = 30 * 60; 
const int I2C_DISPLAY_ADDRESS = 0x3C;
#if defined(ESP8266)
const int SDA_PIN = D3;
const int SDC_PIN = D4;
#else
const int SDA_PIN = 4; //D3;
const int SDC_PIN = 5; //D4;
#endif
// Các chân cho các nút nhấn
#define BUZZER_PIN D5
#define buttonSelectPin  D6
#define buttonIncreasePin  D7
#define buttonResetSecondPin  D1
enum TimeUnit { OTHER_MODE,MINUTE, HOUR, DAY, MONTH, YEAR, ALARM };
TimeUnit selectedUnit = OTHER_MODE;
uint8_t second, minute, hour, day, month, year;


String OPEN_WEATHER_MAP_APP_ID = "7f56beffa2de944ebcadbd3468e22bc8";
String OPEN_WEATHER_MAP_LOCATION_ID = "1580578";

String OPEN_WEATHER_MAP_LANGUAGE = "en";
const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = true;

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

 SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

bool readyForWeatherUpdate = false;

String lastUpdate = "--";

long timeSinceLastWUpdate = 0;

FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawForecast, drawTemp, drawHum };
int numberOfFrames = 5;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

DHT dht = DHT(D2, DHT11, 2);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // Khởi tạo module RTC
  rtc.begin();

  // Kiểm tra xem module RTC có kết nối không
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
  }

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

  dht.begin();
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  
  pinMode(buttonSelectPin, INPUT_PULLUP);
  pinMode(buttonIncreasePin, INPUT_PULLUP);
  pinMode(buttonResetSecondPin, INPUT_PULLUP ); // Khởi tạo nút reset giây
  

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("     Setting   ");
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, 1);
  
  int counter=0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawXbm(0,0,wifi_width,wifi_height,WIFI_bits); 
    display.drawString(64, 30, "Connecting to WiFi");
    display.drawXbm(46, 40, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 40, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 40, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);

  
  DateTime now = rtc.now(); // Đọc thời gian từ module RTC
  int hour = now.hour();
  int minute = now.minute();
  int second = now.second();
  int year = now.year();
  int month = now.month();
  int day = now.day();
  int hour1=hour/10;
  int hour2=hour%10;
  int minute1=minute/10;
  int minute2=minute%10;
  int second1=second/10;
  int second2=second%10;
  int day1=day/10;
  int day2=day%10;
  int month1=month/10;
  int month2=month%10;

  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(20, 0, "Time:");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, String(hour1)+String(hour2) + ":" + String(minute1)+String(minute2) + ":" + String(second1)+String(second2));
  display.drawString(20, 10, "Date:");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, String(day1) + String(day2) + "/" + String(month1)+ String(month2) + "/" + String(year));
  display.display();
  counter++;
  checkButtons();

  if (now.hour() == alarmHour && now.minute() == alarmMinute) {
  digitalWrite(BUZZER_PIN, 0);
  delay(5000); // Thời gian báo động
  digitalWrite(BUZZER_PIN, 1);
  bool alarmSet = false; // Tắt báo thức sau khi kích hoạt

  }

  }

  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_UP);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  ui.init();

  Serial.println("");

  updateData(&display);
  
  display.clear();
  display.drawXbm(0,0,logo1_width,logo1_height,logo1_bits);
  display.drawString(85,10,"Landmark 81");
  display.drawString(85,20,"Group 1");
  display.drawString(85,30,"Digital Clock");
  display.display();
  delay(7000);
  
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,"LOADING");
  display.display();  
  delay(3000);
  display.drawString(0,10,"TIME AND DATE");
  display.display();  
  delay(3000);
  display.drawString(0,20,"TEMPERATURE");
  display.display();  
  delay(3000);
  display.drawString(0,30,"HUMIDITY");
  display.display();  
  delay(3000);
  display.drawString(0,40,"FORECAST");
  display.display();  
  delay(3000);
  display.drawString(0,50,"UPLOADING....");
  display.display();  
  delay(3000);
}

void loop() {
  // Kiểm tra trạng thái nút nhấn để chọn đơn vị thời gian
  void checkButtons();
  //Kiem tra bao thuc 
  void checkAlarm();

  if (millis() - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
updateData(&display);
}

int remainingTimeBudget = ui.update();

if (remainingTimeBudget > 0) {
delay(remainingTimeBudget);
}
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  drawProgress(display, 30, "Updating weather...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);
  drawProgress(display, 50, "Updating forecasts...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];
  sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  display->drawString(64 + x, 5 + y, String(buff));
  display->setFont(ArialMT_Plain_24);
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 15 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawTemp(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  float temperature = dht.readTemperature();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 5 + y, "Room Temperature");
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 15 + y, String(temperature,2)+("°C"));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHum(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  int humidity = dht.readHumidity();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 5 + y, "Humidity");
  display->setFont(ArialMT_Plain_24);
  display->drawString(64 + x, 15 + y, String(humidity)+(" %"));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);
  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(51 + x, 5 + y, temp);
  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(23  + x, 0 + y, currentWeather.iconMeteoCon);
}
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);
  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}
// Hàm reset giây
void resetSecond() {
  DateTime now = rtc.now();
  rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), 0));
}


void increaseTimeUnit() {
  DateTime now = rtc.now();
  uint8_t second = now.second();
  uint8_t minute = now.minute();
  uint8_t hour = now.hour();
  uint8_t day = now.day();
  uint8_t month = now.month();
  uint16_t year = now.year();
  bool selectPressed = digitalRead(buttonSelectPin) == LOW;
  bool increasePressed = digitalRead(buttonIncreasePin) == LOW;

  switch (selectedUnit) {
    case MINUTE:
      minute = (minute + 1) % 60;
      lcd.setCursor(0, 1);
      lcd.print(minute < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
      lcd.print(minute);
      break;
    case HOUR:
      hour = (hour + 1) % 24;
      lcd.setCursor(0, 1);
      lcd.print(hour < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
      lcd.print(hour);
      break;
    case DAY:
      day = (day % 31) + 1; // Cần xem xét ngày cuối cùng của tháng
      lcd.setCursor(0, 1);
      lcd.print(day < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
      lcd.print(day);
      break;
    case MONTH:
      month = (month % 12) + 1; // Tháng từ 1 đến 12
      lcd.setCursor(0, 1);
      lcd.print(month < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
      lcd.print(month);
      break;
    case YEAR:
      year++;
      if (year > 2099) { // Giới hạn năm
        year = 2000;
      }
      lcd.setCursor(0, 1);
      lcd.print(year < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
      lcd.print(year);
      break;
    case ALARM:

      if (adjustingHour) {
        lcd.clear();
        alarmHour=(alarmHour +1)%24;
        lcd.setCursor(0, 0);
        lcd.print("     Alarm    ");
        lcd.setCursor(0, 1);
        lcd.print("Hour:");
        lcd.print(alarmHour < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
        lcd.print(alarmHour);
        
      } else {
        lcd.clear();
        alarmMinute=(alarmMinute+1)%60;
        lcd.setCursor(0, 0);
        lcd.print("     Alarm    ");
        lcd.setCursor(0, 1);
        lcd.print("Minute:");
        lcd.print(alarmMinute < 10 ? "0" : ""); // In số 0 đứng đầu nếu cần
        lcd.print(alarmMinute);
      }
      break;
  }


  rtc.adjust(DateTime(year, month, day, hour, minute, second));
}


void checkButtons() {
  if (digitalRead(buttonIncreasePin) == LOW && digitalRead(buttonSelectPin) == LOW) {
    delay(500);
    adjustingHour = !adjustingHour; // Chuyển đổi giữa giờ và phút báo thức
    if(adjustingHour){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("     Alarm    ");
      lcd.setCursor(0, 1);
      lcd.print("Set Hour");
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("     Alarm    ");
      lcd.setCursor(0,1);
      lcd.print("Set Minute");
    }
  }
  // Check button states
  if (digitalRead(buttonSelectPin) == LOW) {
    selectedUnit = static_cast<TimeUnit>((selectedUnit + 1) % 7);
    lcd.clear();
    lcd.setCursor(0, 0);
    switch (selectedUnit) {
      case OTHER_MODE:
      lcd.print("     Setting   ");
      break;
      case MINUTE:
        lcd.print("     Minute    ");
        break;
      case HOUR:
        lcd.print("      Hour     ");
        break;
      case DAY:
        lcd.print("       Day     ");
        break;
      case MONTH:
        lcd.print("      Month    ");
        break;
      case YEAR:
        lcd.print("      Year     ");
        break;
      case ALARM:
        lcd.print("     Alarm    ");
        break;
    }
    delay(1000); // Debounce delay
  }
  if (digitalRead(buttonIncreasePin) == LOW) {
    increaseTimeUnit();
    delay(500);  // Debounce delay
  }
  if (digitalRead(buttonResetSecondPin) == LOW) {
    resetSecond();
    delay(300); // Debounce delay
  }
  
}





