#include <RTKRoverManager.h>

/*
=================================================================================
                                WiFi
=================================================================================
*/

#pragma region: WIFI

void RTKRoverManager::setupStationMode(const char* ssid, const char* password, const char* deviceName) 
{
  WiFi.mode(WIFI_STA);
  WiFi.begin( ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    // TODO:  - count reboots and stop after 3 times (save in SPIFFS)
    //        - display state
    DEBUG_SERIAL.println("WiFi failed! Reboot in 10 s!");
    delay(10000);
    ESP.restart();
  }
  DEBUG_SERIAL.println();

  if (!MDNS.begin(getDeviceName(DEVICE_TYPE).c_str())) 
  {
    DEBUG_SERIAL.println(F("Error starting mDNS, use local IP instead!"));
  } 
  else 
  {
    DEBUG_SERIAL.printf("Starting mDNS, find me under <http://www.%s.local>\n", WiFi.getHostname());
  }

  DEBUG_SERIAL.print(F("Wifi client started: "));
  DEBUG_SERIAL.println(WiFi.getHostname());
  DEBUG_SERIAL.print(F("IP Address: "));
  DEBUG_SERIAL.println(WiFi.localIP());
}

bool RTKRoverManager::checkConnectionToWifiStation() 
{ 
  bool isConnectedToStation = false;

  if (WiFi.getMode() == WIFI_MODE_STA)
  {
    if (WiFi.status() != WL_CONNECTED) 
    {
      DEBUG_SERIAL.println(F("Reconnecting to access point."));
      DEBUG_SERIAL.print(F("SSID: "));
      WiFi.disconnect();
      isConnectedToStation = WiFi.reconnect();
    } 
    else 
    {
      DEBUG_SERIAL.println(F("WiFi connected."));
      isConnectedToStation = true;
    }
  }

  return isConnectedToStation;
}

void RTKRoverManager::setupAPMode(const char* apSsid, const char* apPassword) 
{
  DEBUG_SERIAL.print("Setting soft-AP ... ");
  WiFi.mode(WIFI_AP);
  DEBUG_SERIAL.println(WiFi.softAP(apSsid, apPassword) ? "Ready" : "Failed!");
  DEBUG_SERIAL.print(F("Access point started: "));
  DEBUG_SERIAL.println(apSsid);
  DEBUG_SERIAL.print(F("IP address: "));
  DEBUG_SERIAL.println(WiFi.softAPIP());
}

bool RTKRoverManager::savedNetworkAvailable(const String& ssid) 
{
  if (ssid.isEmpty()) return false;

  uint8_t nNetworks = (uint8_t) WiFi.scanNetworks();
  DEBUG_SERIAL.print(nNetworks);  
  DEBUG_SERIAL.println(F(" networks found."));
  for (uint8_t i=0; i<nNetworks; i++) 
  {
    if (ssid.equals(String(WiFi.SSID(i)))) 
    {
      DEBUG_SERIAL.print(F("A known network with SSID found: ")); 
      DEBUG_SERIAL.print(WiFi.SSID(i));
      DEBUG_SERIAL.print(F(" (")); 
      DEBUG_SERIAL.print(WiFi.RSSI(i)); 
      DEBUG_SERIAL.println(F(" dB), connecting..."));
      return true;
    }
  }
  return false;
}

#pragma endregion

/*
=================================================================================
                                Web server
=================================================================================
*/
void RTKRoverManager::startServer(AsyncWebServer *server) 
{
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) 
  {
    request->send_P(200, "text/html", INDEX_HTML, processor);
  });

  server->on("/actionUpdateData", HTTP_POST, actionUpdateData);
  server->on("/actionWipeData", HTTP_POST, actionWipeData);
  server->on("/actionRebootESP32", HTTP_POST, actionRebootESP32);

  server->onNotFound(notFound);
  server->begin();
}
  
void RTKRoverManager::notFound(AsyncWebServerRequest *request) 
{
  request->send(404, "text/plain", "Not found");
}

void RTKRoverManager::actionRebootESP32(AsyncWebServerRequest *request) 
{
  DEBUG_SERIAL.println("ACTION actionRebootESP32!");
  request->send_P(200, "text/html", REBOOT_HTML, RTKRoverManager::processor);
  delay(3000);
  ESP.restart();
}

void RTKRoverManager::actionWipeData(AsyncWebServerRequest *request) 
{
  DEBUG_SERIAL.println("ACTION actionWipeData!");
  int params = request->params();
  DEBUG_SERIAL.printf("params: %d\n", params);
  for (int i = 0; i < params; i++) 
  {
    AsyncWebParameter* p = request->getParam(i);
    DEBUG_SERIAL.printf("%d. POST[%s]: %s\n", i+1, p->name().c_str(), p->value().c_str());
    if (strcmp(p->name().c_str(), "wipe_button") == 0) 
    {
      if (p->value().length() > 0) 
      {
        DEBUG_SERIAL.printf("wipe command received: %s",p->value().c_str());
        wipeSpiffsFiles();
      } 
     }
    } 

  DEBUG_SERIAL.print(F("Data in SPIFFS was wiped out!"));
  request->send_P(200, "text/html", INDEX_HTML, processor);
}

void RTKRoverManager::actionUpdateData(AsyncWebServerRequest *request) 
{
  DEBUG_SERIAL.println("ACTION actionUpdateData!");

  int params = request->params();
  for (int i = 0; i < params; i++) 
  {
    AsyncWebParameter* p = request->getParam(i);
    DEBUG_SERIAL.printf("%d. POST[%s]: %s\n", i+1, p->name().c_str(), p->value().c_str());

    if (strcmp(p->name().c_str(), PARAM_WIFI_SSID) == 0) 
    {
      if (p->value().length() > 0) 
      {
        writeFile(SPIFFS, PATH_WIFI_SSID, p->value().c_str());
      } 
    }

    if (strcmp(p->name().c_str(), PARAM_WIFI_PASSWORD) == 0) 
    {
      if (p->value().length() > 0) 
      {
        writeFile(SPIFFS, PATH_WIFI_PASSWORD, p->value().c_str());
      } 
    }

    if (strcmp(p->name().c_str(), PARAM_RTK_CASTER_HOST) == 0) 
    {
      if (p->value().length() > 0) 
      {
        writeFile(SPIFFS, PATH_RTK_CASTER_HOST, p->value().c_str());
      } 
    }

    if (strcmp(p->name().c_str(), PARAM_RTK_CASTER_PORT) == 0) 
    {
      if (p->value().length() > 0) 
      {
        writeFile(SPIFFS, PATH_RTK_CASTER_PORT, p->value().c_str());
      } 
    }

    if (strcmp(p->name().c_str(), PARAM_RTK_CASTER_USER) == 0) 
    {
      if (p->value().length() > 0) 
      {
        writeFile(SPIFFS, PATH_RTK_CASTER_USER, p->value().c_str());
      } 
    }

    if (strcmp(p->name().c_str(), PARAM_RTK_MOINT_POINT) == 0) 
    {
      if (p->value().length() > 0) 
      {
        writeFile(SPIFFS, PATH_RTK_MOINT_POINT, p->value().c_str());
      } 
    }


  }
  DEBUG_SERIAL.println(F("Data saved to SPIFFS!"));
  request->send_P(200, "text/html", INDEX_HTML, RTKRoverManager::processor);
}

// Replaces placeholder with stored values
String RTKRoverManager::processor(const String& var) 
{
  if (var == PARAM_WIFI_SSID) 
  {
    String savedSSID = readFile(SPIFFS, PATH_WIFI_SSID);
    return (savedSSID.isEmpty() ? String(PARAM_WIFI_SSID) : savedSSID);
  }
  else if (var == PARAM_WIFI_PASSWORD) 
  {
    String savedPassword = readFile(SPIFFS, PATH_WIFI_PASSWORD);
    return (savedPassword.isEmpty() ? String(PARAM_WIFI_PASSWORD) : "*******");
  }

  else if (var == PARAM_RTK_CASTER_HOST) 
  {
    String savedCaster = readFile(SPIFFS, PATH_RTK_CASTER_HOST);
    return (savedCaster.isEmpty() ? String(PARAM_RTK_CASTER_HOST) : savedCaster);
  }

  else if (var == PARAM_RTK_CASTER_PORT) 
  {
    String savedCaster = readFile(SPIFFS, PATH_RTK_CASTER_PORT);
    return (savedCaster.isEmpty() ? String(PARAM_RTK_CASTER_PORT) : savedCaster);
  }

  else if (var == PARAM_RTK_CASTER_USER) 
  {
    String savedCaster = readFile(SPIFFS, PATH_RTK_CASTER_USER);
    return (savedCaster.isEmpty() ? String(PARAM_RTK_CASTER_USER) : savedCaster);
  }

  else if (var == PARAM_RTK_MOINT_POINT) 
  {
    String savedCaster = readFile(SPIFFS, PATH_RTK_MOINT_POINT);
    return (savedCaster.isEmpty() ? String(PARAM_RTK_MOINT_POINT) : savedCaster);
  }
 
  else if (var == "next_addr") 
  {
    String savedSSID = readFile(SPIFFS, PATH_WIFI_SSID);
    String savedPW = readFile(SPIFFS, PATH_WIFI_PASSWORD);
    if (savedSSID.isEmpty() || savedPW.isEmpty()) 
    {
      return String(IP_AP);
    } else 
    {
      String clientAddr = getDeviceName(DEVICE_TYPE);
      clientAddr += ".local";
      return clientAddr;
    }
  }
  else if (var == "next_ssid") 
  {
    String savedSSID = readFile(SPIFFS, PATH_WIFI_SSID);
    return (savedSSID.isEmpty() ? getDeviceName(DEVICE_TYPE) : savedSSID);
  }
  return String();
}

/*
=================================================================================
                                SPIFFS
=================================================================================
*/
bool RTKRoverManager::setupSPIFFS(bool formatIfFailed) 
{
  bool success = false;

  #ifdef ESP32
    if (SPIFFS.begin(formatIfFailed)) 
    {
      DEBUG_SERIAL.println("An Error has occurred while mounting SPIFFS");
      success = true;
    }
  #else
    if (!SPIFFS.begin()) 
    {
      DEBUG_SERIAL.println("An Error has occurred while mounting SPIFFS");
      success = false;
      return success;
    }
  #endif

  return success;
}

String RTKRoverManager::readFile(fs::FS &fs, const char* path) 
{
  DEBUG_SERIAL.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");

  if (!file || file.isDirectory()) 
  {
    DEBUG_SERIAL.println("- empty file or failed to open file");
    return String();
  }
  DEBUG_SERIAL.println("- read from file:");
  String fileContent;

  while (file.available()) 
  {
    fileContent += String((char)file.read());
  }
  file.close();
  DEBUG_SERIAL.println(fileContent);

  return fileContent;
}

void RTKRoverManager::writeFile(fs::FS &fs, const char* path, const char* message) 
{
  DEBUG_SERIAL.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, "w");
  if (!file) 
  {
    DEBUG_SERIAL.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) 
  {
    DEBUG_SERIAL.println("- file written");
  } else 
  {
    DEBUG_SERIAL.println("- write failed");
  }
  file.close();
}
void RTKRoverManager::listFiles() 
{
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
 
  while (file) 
  {
      DEBUG_SERIAL.print("FILE: ");
      DEBUG_SERIAL.println(file.name());
      file = root.openNextFile();
  }
  file.close();
  root.close();
}

void RTKRoverManager::wipeSpiffsFiles() 
{
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  DEBUG_SERIAL.println(F("Wiping: "));

  while (file) 
  {
    DEBUG_SERIAL.print("FILE: ");
    DEBUG_SERIAL.println(file.path());
    SPIFFS.remove(file.path());
    file = root.openNextFile();
  }
}