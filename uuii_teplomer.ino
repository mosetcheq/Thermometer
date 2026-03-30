#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#define BUTTON_GPIO 2

OneWire oneWire(16);
DallasTemperature sensors (&oneWire);

LiquidCrystal_I2C lcd(0x27, 20, 4);
File configTxt;

// nastaveni pro posilani do cloudu
char configSSID[64] = "";
char configPassword[64] = "";
char configApiKey[64] = "";
char configURL[64] = "";

//  definice segmentu pro LCD displej
byte segments[6][8] = {
  {
    0b11011, 0b11011, 0, 0b11000, 0b11000, 0, 0b11000, 0b11000
  },
  {
    0b11000, 0b11000, 0, 0b11000, 0b11000, 0, 0b11011, 0b11011
  },
  {
    0b11011, 0b11011, 0, 0, 0, 0, 0, 0
  },
  {
    0, 0, 0, 0, 0, 0, 0b11011, 0b11011
  },
  {
    0b11000, 0b11000, 0, 0b11000, 0b11000, 0, 0b11000, 0b11000
  },
  {
    0, 0, 0, 0, 0, 0, 0b11000, 0b11000,
  }
};

// definice znaku ze segmentu
byte chars[10][16] = {
  {0, 2, 2, 4,  4, 32, 32, 4,  4, 32, 32, 4,  1, 3, 3, 4}, // 0
  {32, 32, 32, 4,  32, 32, 32, 4,  32, 32, 32, 4,  32, 32, 32, 4}, // 1
  {2, 2, 2, 4,  3, 3, 3, 4,  4, 32, 32, 32,  1,  3,  3,  5}, // 2
  {2, 2, 2, 4,  3, 3, 3, 4,  32, 32, 32, 4,  3, 3, 3, 4}, // 3
  {4, 32, 32, 4,  1, 3, 3, 4,  32, 32, 32, 4,  32, 32, 32, 4}, // 4
  {0, 2, 2, 32,  1, 3, 3, 5,  32, 32, 32, 4,  3, 3, 3, 4}, // 5
  {0, 2, 2, 32,  1, 3, 3, 5,  4, 32, 32, 4,  1, 3, 3, 4}, // 6
  {2, 2, 2, 4,  32, 32, 32, 4,  32, 32, 32, 4,  32, 32, 32, 4}, // 7
  {0, 2, 2, 4,  1, 3, 3, 4,  4, 32, 32, 4,  1, 3, 3, 4}, // 8
  {0, 2, 2, 4,  1, 3, 3, 4,  32, 32, 32, 4,  3, 3, 3, 4} // 9
};

int c, numSensors = 0, displayMode = 0, hh = 88, mm = 88;
bool light = true, wifiMode = false;
DeviceAddress thermometer[8];
float temperatures[8];
String payload = "";
long timer = 0, sensorRead = 0, sending = 0, debounce = 0;

WiFiClient network;
HTTPClient http;

void setup() {
  // put your setup code here, to run once:

  pinMode(BUTTON_GPIO, INPUT_PULLUP);

  Wire.begin();
  Serial.begin(19200);
  Serial.println("");

  lcd.init();

  for(c = 0; c < 6; c++) {
    lcd.createChar(c, segments[c]);
  }

  lcd.backlight();
  lcd.on();
  lcd.setCursor(0, 0);
  lcd.print("Nacitam SD kartu");
  lcd.setCursor(0, 1);

  if(!SD.begin(15)) {
    lcd.print("Chyba cteni karty");
    delay(2000);
  } else {
    lcd.print("Karta nactena");

    if(SD.exists("config.txt")) {

      configTxt = SD.open("config.txt");
      readConfiguration(configTxt);
      configTxt.close();
    
    } else {
      lcd.clear();
      lcd.home();
                
      lcd.print("Vyjmete kartu,");
      lcd.setCursor(0, 1);
      lcd.print("doplnte data do");
      lcd.setCursor(0, 2);
      lcd.print("\"config.txt\", vlozte");
      lcd.setCursor(0, 3);
      lcd.print("kartu a restartujte.");

      configTxt = SD.open("config.txt", FILE_WRITE);
      configTxt.println("WIFI");
      configTxt.println("");
      configTxt.println("URL");
      configTxt.println("KEY");
      configTxt.close();

      delay(10000);

    }
  
  }


  if(wifiMode) {
    lcd.clear();
    lcd.home();
            
    lcd.print("Pripojuji k WIFI");
    lcd.setCursor(0, 1);
    lcd.print("SSID: ");
    lcd.print(configSSID);
    lcd.setCursor(0, 2);

    WiFi.mode(WIFI_STA);
    WiFi.begin(configSSID, configPassword);
    c = 0;
    while(WiFi.status() != WL_CONNECTED) {
      if(c == 20) {
        c = 0;
        lcd.setCursor(0, 2);
        lcd.print("                    ");
        lcd.setCursor(0, 2);
      }
      c++;
      lcd.write(255);
      delay(500);
    }
    int percentage = map(WiFi.RSSI(), -100, -50, 0, 100);
    lcd.setCursor(0, 2);
    lcd.print("Pripojeno           ");
    lcd.setCursor(10,2);
    lcd.print(percentage);
    lcd.print('%');
    lcd.setCursor(0, 3);
    lcd.print("IP: ");
    lcd.print(WiFi.localIP());
    delay(2000);
  }

  sensors.begin();
  sensors.setResolution(11);
  numSensors = sensors.getDeviceCount();
  for(c = 0; c < numSensors; c++) {
    sensors.getAddress(thermometer[c], c);
  }

  lcd.clear();
  timer = millis();

}

void loop() {
  // put your main code here, to run repeatedly:


  if(!digitalRead(2) && (millis() - debounce > 500)) {
    int range = millis() - timer;
    debounce = timer = millis();
    Serial.println(range);
    if(!light) {
      // pokud displej nesviti tak ho pouze rozneme
      lcd.backlight();
      light = true;
    } else {
      // displej sviti - muzeme menit rezim zobrazeni
      displayMode++;
      if(displayMode > numSensors) {
        displayMode = 0;
      }
      lcd.clear();
      sensorRead = 0;
    }
  }

  if((millis() - timer > 60000) && light) {
    light = false;
    lcd.noBacklight();
  }


  if(millis() - sensorRead > 5000) {
    sensorRead = millis();
    lcd.setCursor(19, 1);
    lcd.write(127);

    // nacteni teplot ze senzoru
    sensors.requestTemperatures();
    for(c = 0; c < numSensors; c++) {
      temperatures[c] = sensors.getTempCByIndex(c);
    }
  
    lcd.setCursor(19, 1);
    lcd.write(32);

    if(displayMode < numSensors) {
      lcd.setCursor(18, 0); lcd.write(223); lcd.write(67);
      lcd.setCursor(19, 3); lcd.write(displayMode + 49);
      writeNumber(0, temperatures[displayMode]);
    } else {
      if(wifiMode) {
        writeInt(2, hh, false);
        writeDoubleDot(10);
        writeInt(11, mm, true);
      } else {
        lcd.setCursor(0, 1);
                // 01234567890123456789
        lcd.print("Hodiny jsou dostupne");
        lcd.setCursor(0, 2);
        lcd.print("    pouze online    ");
      }
    }
  }


  if((millis() - sending > 30000) && wifiMode) {
    sending = millis();
    lcd.setCursor(19, 2);
    lcd.write(126);
    // odesilani dat do cloudu
    payload = "[";
    for(c = 0; c < numSensors; c++) {
      payload+= "{\"sensor\": \"DS18B20\", \"sensor_id\": \"" + getHexAddress(thermometer[c]) + "\", \"type\": \"temperature\", \"value\": " + String(temperatures[c]) + "},\n";
    }
    payload+= "{\"uptime\": " + String(millis()) + "}]";

    http.begin(network, configURL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Device-Key", configApiKey);
    int responseCode = http.POST(payload);
    Serial.println(responseCode);
    if(responseCode == 200) {
      String data = http.getString();
      Serial.println(data);
      hh = data.substring(8, 10).toInt();
      mm = data.substring(10, 12).toInt();
    }
    http.end();
    lcd.setCursor(19, 2);
    lcd.write(responseCode != 200 ? 69 : 32);
  }

}

// vypise cislo
void writeNumber(uint8_t pos, float number) {
  int num;
  number = constrain(number, -99.9, 99.9);
  pos = writeSign(pos, (number < 0));
  number = abs(number);
  if(number >= 10) {
    num = number / 10;
    pos = writeDigit(pos, num);
    number = number - (num * 10);
  }
  num = number;
  pos = writeDigit(pos, num);
  number = number - num;
  pos = writeDot(pos);
  num = number * 10;
  pos = writeDigit(pos, num);
}

// vypise cislo
void writeInt(uint8_t pos, int number, bool zeroes) {
  int num;
  if(number >= 10 || zeroes) {
    num = number / 10;
    pos = writeDigit(pos, num);
    number = number - (num * 10);
  } else {
    pos = pos + 4;
  }
  num = number;
  pos = writeDigit(pos, num);
}

// vypise cislici na displej
uint8_t writeDigit(uint8_t position, int digit) {
  uint8_t x, y;
  for(y = 0; y < 4; y++) {
    lcd.setCursor(position, y);
    for(x = 0; x < 4; x++) {
      lcd.write(chars[digit][y * 4 + x]);
    }
  }
  return(position + 4);
}

uint8_t writeDot(uint8_t position) {
  uint8_t y;
  for(y = 0; y < 4; y++) {
    lcd.setCursor(position, y);
    lcd.write(y == 3 ? 5 : 32);
  }
  return(position + 1);
}

uint8_t writeDoubleDot(uint8_t position) {
  uint8_t y;
  for(y = 0; y < 4; y++) {
    lcd.setCursor(position, y);
    lcd.write(y & 1 ? 5 : 32);
  }
  return(position + 1);
}

uint8_t writeSign(uint8_t position, bool sign) {
  uint8_t y;
  for(y = 0; y < 4; y++) {
    lcd.setCursor(position, y);
    if((y == 1) && sign) {
      lcd.write(3); lcd.write(3); lcd.write(3);
    } else {
      lcd.print("   ");
    }
  }
  return(position + 4);
}


String getHexAddress(DeviceAddress param) {
  uint8_t cc;
  String out = "";
  for(cc = 0; cc < 8; cc++) {
    if(param[c] < 16) {
      out+= String("0");
    }
    out+= String(param[cc], HEX);
  }
  return(out);
}


// Nacitani konfiguracniho souboru z SD kcharty
void readConfiguration(File config) {
  String cLine = "";
  while(config.available()) {
    cLine = readLine(config);

    if(cLine == String("WIFI")) {
        cLine = readLine(config);
        cLine.toCharArray(configSSID, cLine.length() + 1);

        cLine = readLine(config);
        cLine.toCharArray(configPassword, cLine.length() + 1);
    }
  
    if(cLine == String("URL")) {
        cLine = readLine(config);
        cLine.toCharArray(configURL, cLine.length() + 1);
        wifiMode = true;

    }
 
    if(cLine == String("KEY")) {
        cLine = readLine(config);
        cLine.toCharArray(configApiKey, cLine.length() + 1);
    }
  }
}

String readLine(File srcFile) {
  String line = srcFile.readStringUntil(10);
  line.trim();
  return(line);
}
