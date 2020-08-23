#ifndef WSHANDLER_HPP
#define WSHANDLER_HPP

#include <WebsocketHandler.hpp>
using namespace httpsserver;

// Max clients to be connected to the chat
#define MAX_CLIENTS 4

// As websockets are more complex, they need a custom class that is derived from WebsocketHandler
class WsHandler : public WebsocketHandler {
public:
  // This method is called by the webserver to instantiate a new handler for each
  // client that connects to the websocket endpoint
  static WebsocketHandler* create();

  // This method is called when a message arrives
  void onMessage(WebsocketInputStreambuf * input);

  // Handler function on connection close
  void onClose();
};

// Simple array to store the active clients:
extern WsHandler* activeClients[MAX_CLIENTS];

#endif  //WSHANDLER_HPP

