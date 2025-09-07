/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "demo/binance_websocket_client.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <chrono>

namespace demo
{

BinanceWebSocketClient::BinanceWebSocketClient()
  : context_(nullptr), websocket_(nullptr), connected_(false), should_stop_(false)
{
}

BinanceWebSocketClient::~BinanceWebSocketClient()
{
  stop();
  if (context_)
  {
    lws_context_destroy(context_);
  }
}

bool BinanceWebSocketClient::initialize()
{
  // Set up protocols
  static struct lws_protocols protocols[] = {
    {
      "",
      websocket_callback,
      0,
      65536,  // 64KB buffer for large messages
    },
    { nullptr, nullptr, 0, 0 }
  };
  
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof(info));
  
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.gid = -1;
  info.uid = -1;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  
  context_ = lws_create_context(&info);
  if (!context_)
  {
    std::cerr << "Failed to create libwebsockets context" << std::endl;
    return false;
  }
  
  return true;
}

bool BinanceWebSocketClient::connect(const std::string& symbol)
{
  if (!context_)
  {
    std::cerr << "WebSocket client not initialized" << std::endl;
    return false;
  }
  
  // Create stream name: btcusdt@depth10@100ms (partial book depth with 10 levels at 100ms)
  stream_name_ = symbol + "@depth10@0ms";
  std::transform(stream_name_.begin(), stream_name_.end(), stream_name_.begin(), ::tolower);
  
  // Store the full path to ensure it stays in scope
  path_ = "/ws/" + stream_name_;
  
  struct lws_client_connect_info connect_info;
  memset(&connect_info, 0, sizeof(connect_info));
  
  connect_info.context = context_;
  connect_info.address = "fstream.binance.com";
  connect_info.port = 443;
  connect_info.path = path_.c_str();
  connect_info.host = connect_info.address;
  connect_info.origin = connect_info.address;
  connect_info.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
  connect_info.protocol = nullptr;
  connect_info.userdata = this;
  
  websocket_ = lws_client_connect_via_info(&connect_info);
  if (!websocket_)
  {
    std::cerr << "Failed to create WebSocket connection" << std::endl;
    return false;
  }
  
  // Set user data for the WebSocket
  lws_set_wsi_user(websocket_, this);
  
  return true;
}

void BinanceWebSocketClient::setDepthUpdateCallback(DepthUpdateCallback callback)
{
  depth_callback_ = callback;
}

void BinanceWebSocketClient::setConnectionCallback(ConnectionCallback callback)
{
  connection_callback_ = callback;
}

void BinanceWebSocketClient::setErrorCallback(ErrorCallback callback)
{
  error_callback_ = callback;
}

void BinanceWebSocketClient::run()
{
  while (!should_stop_)
  {
    lws_service(context_, 50);
    
    // Process queued messages
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!message_queue_.empty())
    {
      processMessage(message_queue_.front());
      message_queue_.pop();
    }
  }
}

void BinanceWebSocketClient::stop()
{
  should_stop_ = true;
  connected_ = false;
}

int BinanceWebSocketClient::websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                              void* user, void* in, size_t len)
{
  BinanceWebSocketClient* client = static_cast<BinanceWebSocketClient*>(lws_wsi_user(wsi));
  if (!client) return 0;
  
  switch (reason)
  {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      std::cout << "WebSocket connection established" << std::endl;
      client->connected_ = true;
      if (client->connection_callback_)
      {
        client->connection_callback_();
      }
      break;
      
    case LWS_CALLBACK_CLIENT_RECEIVE:
      {
        // Handle potentially fragmented messages
        static thread_local std::string partial_message;
        
        std::string fragment(static_cast<char*>(in), len);
        partial_message += fragment;
        
        // Check if this is the final fragment
        if (lws_is_final_fragment(wsi))
        {
          std::lock_guard<std::mutex> lock(client->queue_mutex_);
          client->message_queue_.push(partial_message);
          partial_message.clear();
        }
      }
      break;
      
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      std::cerr << "WebSocket connection error" << std::endl;
      client->connected_ = false;
      if (client->error_callback_)
      {
        client->error_callback_("Connection error");
      }
      break;
      
    case LWS_CALLBACK_CLOSED:
      std::cout << "WebSocket connection closed" << std::endl;
      client->connected_ = false;
      break;
      
    default:
      break;
  }
  
  return 0;
}

void BinanceWebSocketClient::processMessage(const std::string& message)
{
  // Log the raw JSON message immediately upon arrival
  std::cout << "\n=== WebSocket Message Received ===" << std::endl;
  std::cout << "Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count() << std::endl;
  std::cout << "Message length: " << message.length() << " bytes" << std::endl;
  std::cout << "JSON: " << message << std::endl;
  std::cout << "=================================" << std::endl;
  
  try
  {
    auto json = nlohmann::json::parse(message);
    
    // For single stream connections, the message is directly the depth update
    if (json.contains("e") && json["e"] == "depthUpdate")
    {
      parseDepthUpdate(json);
    }
    // Check if this is a combined stream message
    else if (json.contains("stream") && json.contains("data"))
    {
      std::string stream = json["stream"];
      if (stream == stream_name_)
      {
        parseDepthUpdate(json["data"]);
      }
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
    std::cerr << "Message: " << message << std::endl;
  }
}

void BinanceWebSocketClient::parseDepthUpdate(const nlohmann::json& json)
{
  try
  {
    DepthUpdate update;
    
    update.eventTime = json["E"];
    update.transactionTime = json["T"];
    update.firstUpdateId = json["U"];
    update.finalUpdateId = json["u"];
    update.prevFinalUpdateId = json["pu"];
    
    // Parse bids
    for (const auto& bid : json["b"])
    {
      std::string price = bid[0];
      std::string quantity = bid[1];
      update.bids.emplace_back(price, quantity);
    }
    
    // Parse asks
    for (const auto& ask : json["a"])
    {
      std::string price = ask[0];
      std::string quantity = ask[1];
      update.asks.emplace_back(price, quantity);
    }
    
    if (depth_callback_)
    {
      depth_callback_(update);
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing depth update: " << e.what() << std::endl;
  }
}

}  // namespace demo
