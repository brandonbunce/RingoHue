#include <dummy.h>
#include <MAKERphone.h>
#include "HTTPClient.h"
#include <elapsedMillis.h>

// Eventually add to config folder
int wifiStatus = WL_DISCONNECTED;
File configFile;
MAKERphone mp;
HTTPClient http;
struct ConfigStruct {
  char ssid[64];
  char wpakey[64];
  char hueapikey[64];
  char huebridgeip[64];
};

const char* configPath = "/RingoHue/config.json";
ConfigStruct configStruct;

void drawStatusMessage(String title = "", String subtitle = "") {
  // Wipe screen
  mp.display.fillScreen(TFT_BLACK);
  // Set font
  //mp.display.setFreeFont(TT1);
  // Larger text for title
  mp.display.setTextSize(2.5);
  // Set datum to middle center
  mp.display.setTextDatum(MC_DATUM);
  mp.display.setTextColor(TFT_WHITE);
  mp.display.setFreeFont(TT1);
  // Draw title
  mp.display.drawString(title, 80, 60);
  // Smaller text for subtitle
  mp.display.setTextSize(2);
  // Draw subtitle
  mp.display.drawString(subtitle, 80, 75);
  // Print what we just made
  mp.update();
  Serial.println(title);
  Serial.println(subtitle);
}

// Prints the content of a file to the Serial
void printConfig() {
  // Open file for reading
  configFile = SD.open(configPath);
  if (!configFile) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (configFile.available()) {
    Serial.print((char)configFile.read());
  }
  Serial.println();

  // Close the file (File's destructor doesn't close the file)
  configFile.close();
}

void loadConfiguration() {
  if (mp.SDinsertedFlag) {
    // The MAKERphone function is inherently broken because it doesnt set the "FILE_WRITE" flag?
    // https://github.com/CircuitMess/CircuitMess-Ringo/pull/50
    // mp.writeFile("/RingoHue/config.json", "test");
    configFile = SD.open(configPath, FILE_READ);
    StaticJsonBuffer<512> jsonBuffer;

    // Parse the root object
    JsonObject& root = jsonBuffer.parseObject(configFile);
    if (!root.success()) {
      Serial.println("Failed to read config.json, falling back to default configuration.");
    }
    // Copy values from the JsonObject to the Config struct
    strlcpy(configStruct.ssid,                       // <- destination
            root["ssid"] | "SSID_HERE",              // <- source
            sizeof(configStruct.ssid));              // <- destination's capacity
    strlcpy(configStruct.wpakey,                     // <- destination
            root["wpakey"] | "WPA_HERE",             // <- source
            sizeof(configStruct.wpakey));            // <- destination's capacity
    strlcpy(configStruct.hueapikey,                  // <- destination
            root["hueapikey"] | "API_KEY_HERE",      // <- source
            sizeof(configStruct.hueapikey));         // <- destination's capacity
    strlcpy(configStruct.huebridgeip,                // <- destination
            root["huebridgeip"] | "BRIDGE_IP_HERE",  // <- source
            sizeof(configStruct.huebridgeip));       // <- destination's capacity

    // Close the file (File's destructor doesn't close the file)
    configFile.close();
  }
}

void saveConfiguration() {

  //Remove existing file or else config will be appended
  SD.remove(configPath);

  configFile = SD.open(configPath, FILE_WRITE);
  if (!configFile) {
    Serial.println(F("Failed to create config.json!"));
    return;
  }

  // Allocate the memory pool on the stack
  // Don't forget to change the capacity to match your JSON document.
  // Use https://arduinojson.org/assistant/ to compute the capacity.
  StaticJsonBuffer<512> jsonBuffer;

  // Parse the root object
  JsonObject& root = jsonBuffer.createObject();

  // Set the values
  root["ssid"] = configStruct.ssid;
  root["wpakey"] = configStruct.wpakey;
  root["hueapikey"] = configStruct.hueapikey;
  root["huebridgeip"] = configStruct.huebridgeip;

  // Serialize JSON to file
  if (root.printTo(configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file (File's destructor doesn't close the file)
  configFile.close();
}

void setup() {
  // put your setup code here, to run once:
  mp.begin();
  Serial.begin(115200);
  delay(1000);
  Serial.println("Welcome to RingoHue!");

  while (!SD.begin(5, SPI, 8000000)) {
    Serial.println(F("Failed to initialize SD library. Cannot continue."));
    drawStatusMessage("-- Error --", "Insert SD card!");
    delay(10000);
  }

  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration();

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  saveConfiguration();

  // Dump config file
  Serial.println(F("Print config file..."));
  printConfig();

  drawScreen();
  connectNetwork();
}


void loop() {
  // put your main code here, to run repeatedly:
  mp.update();
  // When application is resumed from mp's idle state.
  if (mp.exitedLockscreen) {
    connectNetwork();
  }
  if (mp.buttons.pressed(BTN_A)) {
    testCommand(true);
  }
  if (mp.buttons.pressed(BTN_B)) {
    testCommand(false);
  }
  drawScreen();
}

void connectNetwork() {
  // Retrieve wifi status on function called
  wifiStatus = WiFi.status();
  Serial.println("Wifi Status: " + String(wifiStatus));
  // Evaluate what to do based on WiFi status
  switch (wifiStatus) {

    case 255:  // WL_NO_SHIELD
      Serial.println("WiFI not initialized. Reattempting.");
      drawStatusMessage("Connecting...", String(configStruct.ssid));
      // Attempting reconnect.
      WiFi.begin(configStruct.ssid, configStruct.wpakey);
      // Wait 1 seconds
      delay(3000);
      // Check if conditions have changed.
      connectNetwork();
      break;

    case 6:  // WL_WRONG_PASSWORD
      Serial.println("Wifi password incorrect! Please reconfigure RingoHue.");
      drawStatusMessage("Connection Failed", "Check password.");
      Serial.println(String(configStruct.ssid) + "didn't work.");
      delay(2000);
      connectNetwork();
      break;

    case 0:  // WL_IDLE_STATUS
    case 7:  // WL_DISCONNECTED,
    case 5:  //WL_CONNECTION_LOST
      drawStatusMessage("Connection Lost", "Reconnecting...");
      WiFi.begin(configStruct.ssid, configStruct.wpakey);
      connectNetwork();
      break;

    case 4:  //WL_CONNECT_FAILED
      WiFi.begin(configStruct.ssid, configStruct.wpakey);
      drawStatusMessage("Connection Failed!", "Are you in range?");
      connectNetwork();
      break;

    case 3:  //WL_CONNECTED
      {
        drawStatusMessage("Connected", WiFi.localIP().toString());
        // Why in god's name are concatenations done like this.
      }
      break;

    default:
      Serial.println("Initializing WiFi...");
      WiFi.begin(configStruct.ssid, configStruct.wpakey);
      delay(3000);
      connectNetwork();
      break;
  }
}

String PUTCompiler(String state, String hue, String bri) {
  if (state == "0") {
    state = "false";
  } else {
    state = "true";
  }
  String returnString;
  returnString = "{\"on\":" + state + ", \"hue\":" + hue + ", \"bri\":" + bri + ", \"sat\":255}";
  Serial.println("Built PUT request: " + returnString);
  return returnString;
}

String boolToString(bool state) {
  if (String(state) == "0") {
    return "false";
  } else {
    return "true";
  }
}

void testCommand(bool state) {
  //typecasting String() is a memory hog
  http.begin("http://" + String(configStruct.huebridgeip) + "/api/" + String(configStruct.hueapikey) + "/groups/0/action");
  int httpResponseCode = http.PUT("{\"on\":" + boolToString(state) + "}");
  String response = http.getString();
  if (httpResponseCode == 200) {
    Serial.println("Test command successful.");
  }
  Serial.println("Code: " + httpResponseCode);
  Serial.println("HTTP Response: " + response);
  http.end();
}

void drawScreen() {
  // display is 160x128 FYI!
  if (WiFi.isConnected()) {
    mp.display.drawCircle(80, 64, 20, TFT_GREEN);
  }
}
