#define FLAG_ORG 0
#define FLAG_DNS 0

#include <sstream>

#if FLAG_ORG
// TODO: Configure your WiFi here
#define WIFI_SSID "<your ssid goes here>"
#define WIFI_PSK  "<your pre-shared key goes here>"
#else   //FLAG_ORG
#endif  //FLAG_ORG

// Include certificate data (see note above)
#include "cert.h"
#include "private_key.h"

// We will use wifi
#include <WiFi.h>

// We will use SPIFFS and FS
#include <SPIFFS.h>
#include <FS.h>

// Working with c++ strings
#include <string>

// Define the name of the directory for public files in the SPIFFS parition
#define DIR_PUBLIC "/public"

// We need to specify some content-type mapping, so the resources get delivered with the
// right content type and are displayed correctly in the browser
char contentTypes[][2][32] = {
  {".html", "text/html"},
  {".css",  "text/css"},
  {".js",   "application/javascript"},
  {".json", "application/json"},
  {".png",  "image/png"},
  {".jpg",  "image/jpg"},
  {"", ""}
};

// Includes for the server
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>


#include "WsHandler.h"


#if FLAG_DNS
#include <DNSServer.h>
const byte DNS_PORT = 53;
DNSServer dnsServer;
#endif

#if FLAG_ORG
#else   //FLAG_ORG
const char ssid[] = "test00";
const char pass[] = "12345678";
const IPAddress ip(192,168,48,1);
const IPAddress subnet(255,255,255,0);
#endif  //FLAG_ORG

// Create an SSL certificate object from the files included above ...
SSLCert cert = SSLCert(
    example_crt_DER, example_crt_DER_len,
    example_key_DER, example_key_DER_len
    );
// ... and create a server based on this certificate.
// The constructor has some optional parameters like the TCP port that should be used
// and the max client count. For simplicity, we use a fixed amount of clients that is bound
// to the max client count.
HTTPSServer secureServer = HTTPSServer(&cert, 443, MAX_CLIENTS);

SSLCert * getCertificate();
void handleSPIFFS(HTTPRequest * req, HTTPResponse * res);

// Simple array to store the active clients:
WsHandler* activeClients[MAX_CLIENTS];

void setup() {
  // Initialize the slots
  for(int i = 0; i < MAX_CLIENTS; i++) activeClients[i] = nullptr;

  // For logging
  Serial.begin(115200);

  // Connect to WiFi
  Serial.println("Setting up WiFi");
#if FLAG_ORG
  WiFi.begin(WIFI_SSID, WIFI_PSK);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Connected. IP=");
  Serial.println(WiFi.localIP());
#else   //FLAG_ORG
  WiFi.softAP(ssid,pass);
  delay(100);
  WiFi.softAPConfig(ip,ip,subnet);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Connected. IP=");
  Serial.println(myIP);
#endif  //FLAG_ORG

#if FLAG_DNS
  //dnsServer.start(DNS_PORT, "*", ip);
#endif

  // Try to mount SPIFFS without formatting on failure
  if (!SPIFFS.begin(false)) {
    // If SPIFFS does not work, we wait for serial connection...
    while(!Serial);
    delay(1000);

    // Ask to format SPIFFS using serial interface
    Serial.print("Mounting SPIFFS failed. Try formatting? (y/n): ");
    while(!Serial.available());
    Serial.println();

    // If the user did not accept to try formatting SPIFFS or formatting failed:
    if (Serial.read() != 'y' || !SPIFFS.begin(true)) {
      Serial.println("SPIFFS not available. Stop.");
      while(true);
    }
    Serial.println("SPIFFS has been formated.");
  }
  Serial.println("SPIFFS has been mounted.");

  // We register the SPIFFS handler as the default node, so every request that does
  // not hit any other node will be redirected to the file system.
  ResourceNode * spiffsNode = new ResourceNode("", "", &handleSPIFFS);
  secureServer.setDefaultNode(spiffsNode);

  // The websocket handler can be linked to the server by using a WebsocketNode:
  // (Note that the standard defines GET as the only allowed method here,
  // so you do not need to pass it explicitly)
  WebsocketNode * wsNode = new WebsocketNode("/ws", &WsHandler::create);

  // Adding the node to the server works in the same way as for all other nodes
  secureServer.registerNode(wsNode);

  Serial.println("Starting server...");
  secureServer.start();
  if (secureServer.isRunning()) {
    Serial.print("Server ready. Open the following URL in multiple browser windows to start chatting: https://");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  // This call will let the server do its work
  secureServer.loop();

  // Other code would go here...
  delay(1);
}

/**
 * This handler function will try to load the requested resource from SPIFFS's /public folder.
 * 
 * If the method is not GET, it will throw 405, if the file is not found, it will throw 404.
 */
void handleSPIFFS(HTTPRequest * req, HTTPResponse * res) {

  // We only handle GET here
  if (req->getMethod() == "GET") {
    // Redirect / to /index.html
    std::string reqFile = req->getRequestString()=="/" ? "/index.html" : req->getRequestString();

    // Try to open the file
    std::string filename = std::string(DIR_PUBLIC) + reqFile;

    // Check if the file exists
    if (!SPIFFS.exists(filename.c_str())) {
      // Send "404 Not Found" as response, as the file doesn't seem to exist
      res->setStatusCode(404);
      res->setStatusText("Not found");
      res->println("404 Not Found");
      Serial.println("404 Not Found");
      return;
    }

    File file = SPIFFS.open(filename.c_str());

    // Set length
    res->setHeader("Content-Length", httpsserver::intToString(file.size()));

    // Content-Type is guessed using the definition of the contentTypes-table defined above
    int cTypeIdx = 0;
    do {
      if(reqFile.rfind(contentTypes[cTypeIdx][0])!=std::string::npos) {
        res->setHeader("Content-Type", contentTypes[cTypeIdx][1]);
        break;
      }
      cTypeIdx+=1;
    } while(strlen(contentTypes[cTypeIdx][0])>0);

    // Read the file and write it to the response
    uint8_t buffer[256];
    size_t length = 0;
    do {
      length = file.read(buffer, 256);
      res->write(buffer, length);
    } while (length > 0);

    file.close();
  } else {
    // If there's any body, discard it
    req->discardRequestBody();
    // Send "405 Method not allowed" as response
    res->setStatusCode(405);
    res->setStatusText("Method not allowed");
    res->println("405 Method not allowed");
  }
}
