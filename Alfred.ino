#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed.
                                         
Adafruit_CC3000_Client client = NULL;

#define WLAN_SSID       "BlackSoul"           // cannot be longer than 32 characters!
#define WLAN_PASS       "Ch@pl@nd"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  3000      // Amount of time to wait (in milliseconds) with no data 
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.

// What page to grab!
#define SERVER_HOST               "alfred.glitch.me"
#define API_REGISTER              "/api/register"
#define API_GET_INSTRUCTIONS      "/api/instructions"

#define PIN_7 7
#define PIN_8 8
#define PIN_9 9

#define LOOP_DELAY 5000



/**************************************************************************/
/*!
    @brief  Sets up the HW and the CC3000 module (called automatically
            on startup)
*/
/**************************************************************************/

uint32_t ip;

uint8_t pin7State;
uint8_t pin8State;
uint8_t pin9State;

String accessToken;


void setup(void) { 
  // Pin Setup
  pinMode(PIN_7, OUTPUT);
  pinMode(PIN_8, OUTPUT);
  pinMode(PIN_9, OUTPUT);

  // Serial Port Setup
  Serial.begin(115200);

  Serial.println(F("Hello, Master!\n")); 

  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);

  // Other needed setup
  // 1. Connection Setup
  // 2. API Ping and App configuration setup

  setupConnection();
}

void loop(void) {
  // Store current time
  // Make request to server for new instructions
  // Execute instructions
  // Find time elapsed
  // If the time is less, create a delay else allow loop.

  // Exceptions to be handled
  // Invalid Connection / Connection Failed
  // Invalid Instruction Code
  unsigned long startTime = millis();
  
  if(!cc3000.checkConnected()) {
    cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);
    Serial.println("Connection was lost but successfully retrived");
  }

  if(!client.connected()) {
    client = cc3000.connectTCP(ip, 80);
    Serial.println("Client connection lost but successfully retrived");
  }

  fetchAndExecuteInstructions();

  unsigned long elapsedTime = millis() - startTime;
  
  if(elapsedTime < LOOP_DELAY) {
    Serial.println("Delaying");
    delay(LOOP_DELAY - elapsedTime);
  }
}

void setupConnection(void) { 
  /* Initialise the module */
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin()) {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  Serial.print(F("\nAttempting to connect to SSID: ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP()) {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  
  
  /* Display the IP address DNS, Gateway, etc. */  
  while (! displayConnectionDetails()) {
    delay(1000);
  }
  
  ip = 0;
  // Try looking up the website's IP address
  Serial.print(SERVER_HOST); Serial.print(F(" -> "));
  while (ip == 0) {
    if (! cc3000.getHostByName(SERVER_HOST, &ip)) {
      Serial.println(F("Couldn't resolve!"));
    }
    delay(500);
  }
  
  cc3000.printIPdotsRev(ip);
  
  // Optional: Do a ping test on the website
  /*
  Serial.print(F("\n\rPinging ")); cc3000.printIPdotsRev(ip); Serial.print("...");  
  int replies = cc3000.ping(ip, 5);
  Serial.print(replies); Serial.println(F(" replies"));
  */
  
  /* Try connecting to the website.
     Note: HTTP/1.1 protocol is used to keep the server from closing the connection before all data is read.
  */
  client = cc3000.connectTCP(ip, 80);

  accessToken = getAPIAccessToken();
  accessToken.trim();
}
/**
 * Fetches and executes the instructions.
 */
void fetchAndExecuteInstructions() {
  String instruction = fetchInstructions();
  instruction.trim();

  Serial.print("Trying to execute: ");
  Serial.println(instruction);

  if(instruction != "") {
    char instructionBuffer[2];
    instruction.toCharArray(instructionBuffer, 2);

    executeInstruction(strToHex(instructionBuffer));    
  }
}

/**
 * Sends Get Request.
 */
String sendGetRequest(char* url, char** query = NULL, int queryPairLength = 0) {
  Serial.println(F("Fetching instructions"));
  bool currentLineIsBlank = true;
  bool startFetching = false;

  String body = "";
  String header = "";
  
  if (client.connected()) {
    client.fastrprint(F("GET "));
    client.fastrprint(url);
    // Add query to the url
    if(query != NULL && queryPairLength != 0) {
      client.fastrprint(F("?"));
      Serial.println("Query string: ");
      int i;
      
      for(i = 0; i < queryPairLength; i += 2) {
        Serial.print(query[i]);
        Serial.print(F("="));
        Serial.print(query[i+1]);
        client.fastrprint(query[i]);
        client.fastrprint(F("="));
        client.fastrprint(query[i+1]);
        if(i != queryPairLength - 1) {
          Serial.print(F("&"));
          client.fastrprint(F("&"));
        }
      }
    }
    client.fastrprint(F(" HTTP/1.1\r\n"));
    client.fastrprint(F("Host: ")); client.fastrprint(SERVER_HOST); client.fastrprint(F("\r\n"));
    client.fastrprint(F("\r\n"));
    client.println();
  } else {
    Serial.println(F("Connection failed"));    
    return;
  }
  
  /* Read data until either the connection is closed, or the idle timeout is reached. */ 
  Serial.println(F("\nReading data"));
  unsigned long lastRead = millis();
  while (client.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (client.available()) {
      char c = client.read();
      Serial.print(c);
      
      if(c == '\n' && currentLineIsBlank) {
        startFetching = true;
      }

      if(!startFetching) {
        header += c;
      }

      if(startFetching) {
        body += c;
      }

      if (c == '\n') {
        // you're starting a new line
        currentLineIsBlank = true;
      } else if (c != '\r') {
        // you've gotten a character on the current line
        currentLineIsBlank = false;
      }

      
      lastRead = millis();
    }
  }

  Serial.println();

  return body;
}

/**
 * Gets MAC Address of the device.
 */
void getMACAddress(char* addressBuffer) {
  uint8_t macAddress[6];
  
  if(!cc3000.getMacAddress(macAddress)) {
    Serial.println(F("Unable to retrieve MAC Address to register!\r\n"));
    Serial.println(F("Reload program"));
    while(1);
  } else {
    Serial.print(F("MAC: "));
    sprintf(addressBuffer, "%02x:%02x:%02x:%02x:%02x:%02x", macAddress[0], macAddress[1], macAddress[2], macAddress[3], macAddress[4], macAddress[5]);
    Serial.print(addressBuffer);
  }
}

String getAPIAccessToken() {
  char mac[17];
  getMACAddress(mac);
  
  const char* macQuery[] = { "mac", mac};
  return sendGetRequest(API_REGISTER, macQuery, 1);
}

/**
 * Fetches instructions into a string
 */
String fetchInstructions() {
  char* tokenBuffer = (char*) malloc(sizeof(char) * (accessToken.length() + 1));

  accessToken.toCharArray(tokenBuffer, accessToken.length() + 1);
  
  const char* tokenQuery[] = { "token", tokenBuffer };
  
  String instruction = sendGetRequest(API_GET_INSTRUCTIONS, tokenQuery, 1);

  free(tokenBuffer);

  return instruction;
}

/**
 * Converts String To HEX Number.
 */
long int strToHex(const char* hexString) {
  return strtol(hexString, NULL, 16);
}

/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

/**
 * Toggles Digital Pins
 */
void digitalToggle(uint8_t pin) {
  uint8_t state = digitalRead(pin);
  digitalWrite(pin, (state == HIGH)?LOW:HIGH);
}

/**
 * Executes instruction given from server.
 */
bool executeInstruction(short int instructionCode) {
  /**  
   *   The instruction codes are 4-bit number with first two bits for instruction and last two bits for PIN.
   *   Alfred will support modifying 3 Pins - 7,8 & 9.
   *   
   *   Instruction Codes:
   *   00 - OFF
   *   01 - ON
   *   10 - TOGGLE
   *   11 - STATUS
   *   
   *   PIN Codes:
   *   00 - ALL (7,8 & 9)
   *   01 - PIN 7
   *   10 - PIN 8
   *   11 - PIN 9
   *   
   *   Since the server will send data in string which will be parsed into a hexadecimal number.
   *   This number can be used to get both instruction and PIN information.
   *   
   *   For Example:
   *   
   *   0xA = B1000 = TOGGLE - ALL
   */
   Serial.print(F("Executing instruction code: "));
   Serial.println(instructionCode);
    
   switch (instructionCode) {
    extern uint8_t pin7State, pin8State, pin9State;
    
    case 0x0:
      // OFF - ALL
      digitalWrite(PIN_7, LOW);
      digitalWrite(PIN_8, LOW);
      digitalWrite(PIN_9, LOW);
      return true;
    case 0x1:
      // OFF - PIN_7
      digitalWrite(PIN_7, LOW);
      return true;
    case 0x2:
      // OFF - PIN_8
      digitalWrite(PIN_8, LOW);
      return true;
    case 0x3:
      // OFF - PIN_9
      digitalWrite(PIN_9, LOW);
      return true;
    case 0x4:
      // ON - ALL
      digitalWrite(PIN_7, HIGH);
      digitalWrite(PIN_8, HIGH);
      digitalWrite(PIN_9, HIGH);
      return true;
    case 0x5:
      // ON - PIN_7
      digitalWrite(PIN_7, HIGH);
      return true;
    case 0x6:
      // ON - PIN_8
      digitalWrite(PIN_8, HIGH);
      return true;
    case 0x7:
      // ON - PIN_9
      digitalWrite(PIN_9, HIGH);
      return true;
    case 0x8:
      // TOGGLE - ALL
      digitalToggle(PIN_7);
      digitalToggle(PIN_8);
      digitalToggle(PIN_9);
      return true;
    case 0x9:
      // TOGGLE - PIN_7
      digitalToggle(PIN_7);
      return true;
    case 0xA:
      // TOGGLE - PIN_8
      digitalToggle(PIN_8);
      return true;
    case 0xB:
      // TOGGLE - PIN_9
      digitalToggle(PIN_9);
      return true;
    case 0xC:
      // STATUS - ALL     
      pin7State = digitalRead(PIN_7);
      pin8State = digitalRead(PIN_8);
      pin9State = digitalRead(PIN_9);
      return true;
    case 0xD:
      // STATUS - PIN_7
      pin7State = digitalRead(PIN_7);
      return true;
    case 0xE:
      // STATUS - PIN_8
      pin8State = digitalRead(PIN_8);
      return true;
    case 0xF:
      // STATUS - PIN_9
      pin9State = digitalRead(PIN_9);
      return true;
    default:
      return false;
   }
}

