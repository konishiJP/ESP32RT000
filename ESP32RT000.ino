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

// Declare some handler functions for the various URLs on the server
// The signature is always the same for those functions. They get two parameters,
// which are pointers to the request data (read request body, headers, ...) and
// to the response data (write response, set status code, ...)
void handleRoot(HTTPRequest * req, HTTPResponse * res);
void handle404(HTTPRequest * req, HTTPResponse * res);

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

  // For every resource available on the server, we need to create a ResourceNode
  // The ResourceNode links URL and HTTP method to a handler function
  ResourceNode * nodeRoot    = new ResourceNode("/", "GET", &handleRoot);
  ResourceNode * node404     = new ResourceNode("", "GET", &handle404);

  // Add the root node to the server
  secureServer.registerNode(nodeRoot);

  // The websocket handler can be linked to the server by using a WebsocketNode:
  // (Note that the standard defines GET as the only allowed method here,
  // so you do not need to pass it explicitly)
  WebsocketNode * chatNode = new WebsocketNode("/chat", &WsHandler::create);

  // Adding the node to the server works in the same way as for all other nodes
  secureServer.registerNode(chatNode);

  // Finally, add the 404 not found node to the server.
  // The path is ignored for the default node.
  secureServer.setDefaultNode(node404);

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

void handle404(HTTPRequest * req, HTTPResponse * res) {
  // Discard request body, if we received any
  // We do this, as this is the default node and may also server POST/PUT requests
  req->discardRequestBody();

  // Set the response status
  res->setStatusCode(404);
  res->setStatusText("Not Found");

  // Set content type of the response
  res->setHeader("Content-Type", "text/html");

  // Write a tiny HTTP page
  res->println("<!DOCTYPE html>");
  res->println("<html>");
  res->println("<head><title>Not Found</title></head>");
  res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
  res->println("</html>");
}


// The following HTML code will present the chat interface.
void handleRoot(HTTPRequest * req, HTTPResponse * res) {
  res->setHeader("Content-Type", "text/html");

  res->print(
    "<!DOCTYPE HTML>\n"
    "<html>\n"
    "   <head>\n"
    "   <title>ESP32 Chat</title>\n"
    "</head>\n"
    "<body>\n"
    "    <div style=\"width:500px;border:1px solid black;margin:20px auto;display:block\">\n"
    "        <form onsubmit=\"return false\">\n"
    "            Your Name: <input type=\"text\" id=\"txtName\" value=\"ESP32 user\">\n"
    "            <button type=\"submit\" id=\"btnConnect\">Connect</button>\n"
    "        </form>\n"
    "        <form onsubmit=\"return false\">\n"
    "            <div style=\"overflow:scroll;height:400px\" id=\"divOut\">Not connected...</div>\n"
    "            Your Message: <input type=\"text\" id=\"txtChat\" disabled>\n"
    "            <button type=\"submit\" id=\"btnSend\" disabled>Send</button>\n"
    "        </form>\n"
    "    </div>\n"
    "    <script type=\"text/javascript\">\n"
    "        const elem = id => document.getElementById(id);\n"
    "        const txtName = elem(\"txtName\");\n"
    "        const txtChat = elem(\"txtChat\");\n"
    "        const btnConnect = elem(\"btnConnect\");\n"
    "        const btnSend = elem(\"btnSend\");\n"
    "        const divOut = elem(\"divOut\");\n"
    "\n"
    "        class Chat {\n"
    "            constructor() {\n"
    "                this.connecting = false;\n"
    "                this.connected = false;\n"
    "                this.name = \"\";\n"
    "                this.ws = null;\n"
    "            }\n"
    "            connect() {\n"
    "                if (this.ws === null) {\n"
    "                    this.connecting = true;\n"
    "                    txtName.disabled = true;\n"
    "                    this.name = txtName.value;\n"
    "                    btnConnect.innerHTML = \"Connecting...\";\n"
    "                    this.ws = new WebSocket(\"wss://\" + document.location.host + \"/chat\");\n"
    "                    this.ws.onopen = e => {\n"
    "                        this.connecting = false;\n"
    "                        this.connected = true;\n"
    "                        divOut.innerHTML = \"<p>Connected.</p>\";\n"
    "                        btnConnect.innerHTML = \"Disconnect\";\n"
    "                        txtChat.disabled=false;\n"
    "                        btnSend.disabled=false;\n"
    "                        this.ws.send(this.name + \" joined!\");\n"
    "                    };\n"
    "                    this.ws.onmessage = e => {\n"
    "                        divOut.innerHTML+=\"<p>\"+e.data+\"</p>\";\n"
    "                        divOut.scrollTo(0,divOut.scrollHeight);\n"
    "                    }\n"
    "                    this.ws.onclose = e => {\n"
    "                        this.disconnect();\n"
    "                    }\n"
    "                }\n"
    "            }\n"
    "            disconnect() {\n"
    "                if (this.ws !== null) {\n"
    "                    this.ws.send(this.name + \" left!\");\n"
    "                    this.ws.close();\n"
    "                    this.ws = null;\n"
    "                }\n"
    "                if (this.connected) {\n"
    "                    this.connected = false;\n"
    "                    txtChat.disabled=true;\n"
    "                    btnSend.disabled=true;\n"
    "                    txtName.disabled = false;\n"
    "                    divOut.innerHTML+=\"<p>Disconnected.</p>\";\n"
    "                    btnConnect.innerHTML = \"Connect\";\n"
    "                }\n"
    "            }\n"
    "            sendMessage(msg) {\n"
    "                if (this.ws !== null) {\n"
    "                    this.ws.send(this.name + \": \" + msg);\n"
    "                }\n"
    "            }\n"
    "        };\n"
    "        let chat = new Chat();\n"
    "        btnConnect.onclick = () => {\n"
    "            if (chat.connected) {\n"
    "                chat.disconnect();\n"
    "            } else if (!chat.connected && !chat.connecting) {\n"
    "                chat.connect();\n"
    "            }\n"
    "        }\n"
    "        btnSend.onclick = () => {\n"
    "            chat.sendMessage(txtChat.value);\n"
    "            txtChat.value=\"\";\n"
    "            txtChat.focus();\n"
    "        }\n"
    "    </script>\n"
    "</body>\n"
    "</html>\n"
  );
}
