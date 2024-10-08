#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

String serverURL;
String apiKey;
String endpoint;
String userName;

void connectToWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

void loadServerURL() {
  Serial.println("Please send the Ollama server URL file (server_url.txt) over Serial:");
  
  while (!Serial.available());
  serverURL = Serial.readStringUntil('\n');
  serverURL.trim();
  Serial.println("Ollama server URL loaded successfully.");
}

void loadAPIKey() {
  Serial.println("Please send the API key file (api.txt) over Serial:");
  
  while (!Serial.available());
  apiKey = Serial.readStringUntil('\n');
  apiKey.trim();
  endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=" + apiKey;
  Serial.println("API key loaded successfully.");
}

void saveAP(const char* ssid, const char* password) {
  Serial.println("Saving Access Point information...");
  String apData = String(ssid) + "//" + String(password) + "\n";
  Serial.print("Writing to SavedAPs.txt: ");
  Serial.println(apData);
}

bool loadSavedAP(String &ssid, String &password) {
  Serial.println("Loading saved Access Points from SavedAPs.txt...");
  
  while (!Serial.available());
  String apData = Serial.readStringUntil('\n');
  
  int separatorIndex = apData.indexOf("//");
  if (separatorIndex == -1) return false;

  ssid = apData.substring(0, separatorIndex);
  password = apData.substring(separatorIndex + 2);
  
  ssid.trim();
  password.trim();
  
  return true;
}

bool autoConnectToWiFi(const String &networks) {
  Serial.println("Attempting to auto-connect to known networks...");

  int n = WiFi.scanNetworks();
  
  int startIndex = 0;
  while (startIndex < networks.length()) {
    int separatorIndex = networks.indexOf(",", startIndex);
    if (separatorIndex == -1) separatorIndex = networks.length();
    
    String pair = networks.substring(startIndex, separatorIndex);
    pair.trim();

    int ssidPasswordSeparator = pair.indexOf("//");
    if (ssidPasswordSeparator == -1) {
      Serial.println("Invalid format, skipping: " + pair);
      startIndex = separatorIndex + 1;
      continue;
    }

    String ssid = pair.substring(0, ssidPasswordSeparator);
    String password = pair.substring(ssidPasswordSeparator + 2);
    ssid.trim();
    password.trim();

    for (int i = 0; i < n; ++i) {
      if (ssid == WiFi.SSID(i)) {
        Serial.print("Found matching SSID: ");
        Serial.println(ssid);
        connectToWiFi(ssid.c_str(), password.c_str());
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Connected successfully to " + ssid);
          return true;
        } else {
          Serial.println("Failed to connect to " + ssid);
        }
      }
    }

    startIndex = separatorIndex + 1;
  }
  Serial.println("No matching networks found.");
  return false;
}

void manualConnect() {
  Serial.println("Please enter the SSID of the WiFi network you want to connect to:");
  while (!Serial.available());
  String ssid = Serial.readStringUntil('\n');
  ssid.trim();

  Serial.println("Please enter the password for the WiFi network:");
  while (!Serial.available());
  String password = Serial.readStringUntil('\n');
  password.trim();

  String formattedInput = ssid + "//" + password;

  connectToWiFi(ssid.c_str(), password.c_str());

  if (WiFi.status() == WL_CONNECTED) {
    saveAP(ssid.c_str(), password.c_str());
  } else {
    Serial.println("Failed to connect to " + ssid + ". Manual entry required.");
  }
}

void scanNetworks() {
  Serial.println("DEBUG: Starting WiFi scan...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.println("DEBUG: Initiating scan...");
  int n = WiFi.scanNetworks();
  Serial.println("DEBUG: Scan complete. Networks found: " + String(n));
  
  if (n == 0) {
    Serial.println("No networks found");
  } else {
    for (int i = 0; i < n; ++i) {
      Serial.print("NETWORK:");
      Serial.print(WiFi.SSID(i));
      Serial.print(",");
      Serial.println(WiFi.RSSI(i));
      delay(10);
    }
  }
  Serial.println("SCAN_COMPLETE");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect
  }
  delay(1000);
  Serial.println("DEBUG: ESP32 WiFi Scanner Ready");
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    Serial.println("DEBUG: Received command: " + command);

    if (command == "SCAN") {
      scanNetworks();
    } else if (command.startsWith("CONNECT ")) {
      int separatorIndex = command.indexOf(' ', 8);
      if (separatorIndex != -1) {
        String ssid = command.substring(8, separatorIndex);
        String password = command.substring(separatorIndex + 1);
        connectToWiFi(ssid.c_str(), password.c_str());
      }
    } else if (WiFi.status() == WL_CONNECTED) {
      // Handle chat functionality
      HTTPClient http;
      http.begin(serverURL);
      http.addHeader("Content-Type", "application/json");
      
      String payload = "{\"model\":\"mistral\",\"prompt\":\"" + command + "\",\"stream\":false}";
      
      int httpResponseCode = http.POST(payload);
      
      if (httpResponseCode > 0) {
        String response = http.getString();
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, response);
        
        if (!error) {
          const char* text = doc["response"];
          Serial.println("User: \"" + command + "\"");
          Serial.println("Ollama: \"" + String(text) + "\"");
        } else {
          Serial.println("Error parsing JSON");
        }
      } else {
        Serial.println("Error on HTTP request");
      }
      
      http.end();
    } else {
      Serial.println("WiFi not connected");
    }
  }
}