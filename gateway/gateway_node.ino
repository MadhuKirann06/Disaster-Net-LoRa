#include <SoftwareSerial.h>
#include <LoRa.h>

// --- Configuration ---
#define GSM_TX_PIN 2
#define GSM_RX_PIN 3
SoftwareSerial gsmSerial(GSM_TX_PIN, GSM_RX_PIN);

#define LORA_SS_PIN    10
#define LORA_FREQUENCY 433E6 

// --- APN for GSM Web Browsing ---
// IMPORTANT: Change this to your SIM card's APN
const char* APN = "airtelgprs.com"; 

enum Mode { UNKNOWN, GSM, LORA };
Mode currentMode = UNKNOWN;

unsigned long lastStatusCheck = 0;
const long statusCheckInterval = 10000;

void setup() {
  Serial.begin(9600);
  gsmSerial.begin(9600);
  
  LoRa.setPins(LORA_SS_PIN);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LORA_STATUS:Init Failed");
  } else {
    Serial.println("LORA_STATUS:Init OK");
  }

  pinMode(LED_BUILTIN, OUTPUT);
  checkGsmStatus();
}

void loop() {
  // Check for commands from the Python GUI
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    processGuiCommand(command);
  }

  // Check for status updates or incoming calls from GSM
  if (gsmSerial.available()) {
    String response = gsmSerial.readString();
    // Forward RING for incoming calls
    if (response.indexOf("RING") != -1) {
       Serial.println("CALL_STATUS:Incoming Call");
    }
    // Forward incoming SMS notifications
    else if (response.indexOf("+CMTI:") != -1) {
       Serial.println("GSM_RECV:" + response);
    }
  }

  // Check for incoming LoRa packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) {
      received += (char)LoRa.read();
    }
    Serial.println("LORA_RECV:" + received);
    if (received.startsWith("WEB_PKT:")) {
      int firstColon = received.indexOf(':');
      int firstSlash = received.indexOf('/');
      if (firstColon != -1 && firstSlash != -1 && firstSlash > firstColon) {
        String packetNumStr = received.substring(firstColon + 1, firstSlash);
        sendLoRaPacket("ACK:" + packetNumStr);
      }
    }
  }

  // Periodically check GSM status to switch modes
  if (millis() - lastStatusCheck > statusCheckInterval) {
    checkGsmStatus();
    lastStatusCheck = millis();
  }
}

void processGuiCommand(String command) {
    String cmd, data;
    int separatorIndex = command.indexOf(':');
    if (separatorIndex != -1) {
        cmd = command.substring(0, separatorIndex);
        data = command.substring(separatorIndex + 1);
    } else { cmd = command; }

    if (cmd == "CALL") makeCall(data);
    else if (cmd == "HANGUP") hangUp();
    else if (cmd == "ACCEPT") acceptCall(); // New command
    else if (cmd == "REJECT") hangUp();     // Reject is the same as hanging up
    else if (cmd == "SMS") sendSms(data);
    else if (cmd == "WEB") fetchWebpage(data);
}

// --- Action Functions ---

void makeCall(String number) {
  if (currentMode == GSM) {
    gsmSerial.println("ATD" + number + ";");
    Serial.println("CALL_STATUS:Dialing via GSM");
  } else {
    Serial.println("CALL_STATUS:Failed (Voice calls not supported in LoRa mode)");
  }
}

void hangUp() {
  if (currentMode == GSM) {
    gsmSerial.println("ATH");
    Serial.println("CALL_STATUS:Hung Up");
  }
}

void acceptCall() {
  if (currentMode == GSM) {
    gsmSerial.println("ATA");
    Serial.println("CALL_STATUS:Connected");
  }
}

void sendSms(String data) {
  int firstColon = data.indexOf(':');
  String recipient = data.substring(0, firstColon);
  String message = data.substring(firstColon + 1);
  if (currentMode == GSM) {
    gsmSerial.println("AT+CMGF=1"); delay(100);
    gsmSerial.println("AT+CMGS=\"" + recipient + "\""); delay(100);
    gsmSerial.print(message); delay(100);
    gsmSerial.write(26);
    Serial.println("SMS_STATUS:Sent via GSM");
  } else {
    sendLoRaPacket("SMS_FWD:" + recipient + ":" + message);
    Serial.println("SMS_STATUS:Sent via LoRa");
  }
}

void fetchWebpage(String url) {
  if (currentMode == LORA) {
    sendLoRaPacket("WEB_REQ:" + url);
    Serial.println("WEB_STATUS:Requesting via LoRa...");
  } else if (currentMode == GSM) {
    Serial.println("WEB_STATUS:Requesting via GSM...");
    
    // --- GSM HTTP GET SEQUENCE ---
    // Make sure GPRS is not already active
    gsmSerial.println("AT+SAPBR=0,1"); delay(1000);
    
    // 1. Set connection type to GPRS
    gsmSerial.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""); delay(500);
    readGsmResponse(); // Clear buffer

    // 2. Set the APN
    gsmSerial.println("AT+SAPBR=3,1,\"APN\",\"" + String(APN) + "\""); delay(500);
    readGsmResponse();

    // 3. Enable GPRS
    gsmSerial.println("AT+SAPBR=1,1"); delay(2000);
    readGsmResponse();

    // 4. Initialize HTTP service
    gsmSerial.println("AT+HTTPINIT"); delay(1000);
    readGsmResponse();

    // 5. Set HTTP parameters
    gsmSerial.println("AT+HTTPPARA=\"CID\",1"); delay(500);
    readGsmResponse();
    gsmSerial.println("AT+HTTPPARA=\"URL\",\"" + url + "\""); delay(500);
    readGsmResponse();

    // 6. Start GET session
    gsmSerial.println("AT+HTTPACTION=0"); 
    // Wait for action response, could take a while
    delay(5000); 
    readGsmResponse();

    // 7. Read the data
    gsmSerial.println("AT+HTTPREAD");
    delay(2000);
    String webContent = readGsmResponse();
    
    // 8. Terminate HTTP and GPRS
    gsmSerial.println("AT+HTTPTERM"); delay(500);
    readGsmResponse();
    gsmSerial.println("AT+SAPBR=0,1"); delay(1000);
    readGsmResponse();

    // Find the start of the actual content
    int contentStart = webContent.indexOf("\n");
    if (contentStart != -1) {
      webContent = webContent.substring(contentStart + 1);
    }
    // Truncate for simplicity
    if (webContent.length() > 250) {
      webContent = webContent.substring(0, 250);
    }
    webContent.trim();
    
    Serial.println("WEB_RESP:" + webContent);
    Serial.println("WEB_STATUS:GSM Complete");
  }
}

String readGsmResponse() {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < 2000) { // 2 second timeout
    if (gsmSerial.available()) {
      response += (char)gsmSerial.read();
    }
  }
  return response;
}

void sendLoRaPacket(String packet) {
  delay(20); 
  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();
  if (!packet.startsWith("ACK:")) {
    Serial.println("LORA_SENT:" + packet);
  }
  delay(20);
  LoRa.receive();
}

void checkGsmStatus() {
  gsmSerial.println("AT+CREG?");
  String response = readGsmResponse();
  if (response.indexOf("+CREG: 0,1") != -1 || response.indexOf("+CREG: 0,5") != -1) {
    if (currentMode != GSM) {
      currentMode = GSM;
      Serial.println("STATUS:GSM,Registered");
      digitalWrite(LED_BUILTIN, HIGH);
    }
  } else {
    if (currentMode != LORA) {
      currentMode = LORA;
      Serial.println("STATUS:LORA,Active (GSM Failed)");
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}
