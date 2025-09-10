/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "demo/gateio_websocket_client.h"
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>

namespace demo
{

GateIOWebSocketClient::GateIOWebSocketClient()
    : context_(nullptr), websocket_(nullptr), connected_(false), should_stop_(false)
{
}

GateIOWebSocketClient::~GateIOWebSocketClient()
{
  stop();
  if (context_)
  {
    lws_context_destroy(context_);
  }
}

bool GateIOWebSocketClient::initialize()
{
  // Set up protocols
  static struct lws_protocols protocols[] = {
      {
          "",
          websocket_callback,
          0,
          65536,  // 64KB buffer for large messages
      },
      {nullptr, nullptr, 0, 0}};

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

bool GateIOWebSocketClient::connect(const std::string& contract)
{
  if (!context_)
  {
    std::cerr << "WebSocket client not initialized" << std::endl;
    return false;
  }

  contract_ = contract;

  // Fetch contract info first before establishing WebSocket connection
  std::cout << "📡 Fetching contract information for " << contract << " from Gate.io API..." << std::endl;
  if (!fetchContractInfo(contract))
  {
    std::cerr << "Failed to fetch contract info for " << contract << std::endl;
    return false;
  }

  std::cout << "✅ Contract info fetched successfully! Proceeding with WebSocket connection..." << std::endl;

  std::cout << "📋 Contract Info Summary:" << std::endl;
  std::cout << "   Name: " << contract_info_.name << std::endl;
  std::cout << "   Quanto Multiplier: " << contract_info_.quanto_multiplier << std::endl;
  std::cout << "   Type: " << contract_info_.type << std::endl;
  std::cout << "   Settle: " << contract_info_.settle << std::endl;
  std::cout << std::endl;

  struct lws_client_connect_info connect_info;
  memset(&connect_info, 0, sizeof(connect_info));

  connect_info.context = context_;
  connect_info.address = "fx-ws.gateio.ws";
  connect_info.port = 443;
  connect_info.path = "/v4/ws/usdt";  // USDT futures WebSocket endpoint
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

void GateIOWebSocketClient::setOrderBookUpdateCallback(OrderBookUpdateCallback callback)
{
  orderbook_callback_ = callback;
}

void GateIOWebSocketClient::setConnectionCallback(ConnectionCallback callback)
{
  connection_callback_ = callback;
}

void GateIOWebSocketClient::setErrorCallback(ErrorCallback callback)
{
  error_callback_ = callback;
}

void GateIOWebSocketClient::run()
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

void GateIOWebSocketClient::stop()
{
  should_stop_ = true;
  connected_ = false;
}

std::string GateIOWebSocketClient::generateRequestId()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(100000, 999999);
  return "req_" + std::to_string(dis(gen));
}

// Callback function for curl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
  userp->append((char*)contents, size * nmemb);
  return size * nmemb;
}

bool GateIOWebSocketClient::fetchContractInfo(const std::string& contract)
{
  CURL* curl;
  CURLcode res;
  std::string response_data;

  curl = curl_easy_init();
  if (!curl)
  {
    std::cerr << "Failed to initialize curl" << std::endl;
    return false;
  }

  // Gate.io API endpoint for futures contracts
  std::string url = "https://api.gateio.ws/api/v4/futures/usdt/contracts/" + contract;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
  {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    return false;
  }

  // Print the raw API response
  std::cout << "\n=== Gate.io Contract API Response ===" << std::endl;
  std::cout << "URL: " << url << std::endl;
  std::cout << "Response: " << response_data << std::endl;
  std::cout << "====================================" << std::endl;

  try
  {
    auto json = nlohmann::json::parse(response_data);

    contract_info_.name = json["name"].get<std::string>();
    contract_info_.quanto_multiplier = std::stod(json["quanto_multiplier"].get<std::string>());
    contract_info_.type = json["type"].get<std::string>();

    // Handle settle field which might be null for some contracts
    if (json.contains("settle") && !json["settle"].is_null())
    {
      contract_info_.settle = json["settle"].get<std::string>();
    }
    else
    {
      contract_info_.settle = "USDT";  // Default for USDT futures
    }

    // Print the parsed contract info
    std::cout << "\n=== Parsed Contract Info ===" << std::endl;
    std::cout << "Name: " << contract_info_.name << std::endl;
    std::cout << "Quanto Multiplier: " << contract_info_.quanto_multiplier << std::endl;
    std::cout << "Type: " << contract_info_.type << std::endl;
    std::cout << "Settle: " << contract_info_.settle << std::endl;
    std::cout << "============================" << std::endl;

    return true;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing contract info: " << e.what() << std::endl;
    std::cerr << "Response: " << response_data << std::endl;
    return false;
  }
}

void GateIOWebSocketClient::sendPing()
{
  if (!websocket_ || !connected_)
  {
    return;
  }

  // Gate.io ping format: {"time": timestamp, "channel": "futures.ping", "event": "", "payload": []}
  nlohmann::json ping_msg = {
      {"time", std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()},
      {"channel", "futures.ping"},
      {"event", ""},
      {"payload", nlohmann::json::array()}};

  std::string ping_str = ping_msg.dump();

  // Prepare message with LWS_PRE padding
  size_t msg_len = ping_str.length();
  unsigned char* buf = new unsigned char[LWS_PRE + msg_len];
  memcpy(&buf[LWS_PRE], ping_str.c_str(), msg_len);

  int result = lws_write(websocket_, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
  delete[] buf;

  if (result < 0)
  {
    std::cerr << "Failed to send ping message" << std::endl;
  }
  else
  {
    std::cout << "Sent ping to Gate.io" << std::endl;
  }
}

void GateIOWebSocketClient::sendSubscription()
{
  if (!websocket_ || !connected_)
  {
    return;
  }

  // Subscribe to order book updates with 5 levels at 0ms interval (real-time)
  // Format: {"time": timestamp, "channel": "futures.order_book", "event": "subscribe", "payload": ["BTC_USDT", "5", "0"]}
  nlohmann::json subscribe_msg = {
      {"time", std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count()},
      {"channel", "futures.order_book"},
      {"event", "subscribe"},
      {"payload", nlohmann::json::array({contract_, "5", "0"})}  // contract, levels=5, interval=0ms
  };

  std::string subscribe_str = subscribe_msg.dump();

  // Prepare message with LWS_PRE padding
  size_t msg_len = subscribe_str.length();
  unsigned char* buf = new unsigned char[LWS_PRE + msg_len];
  memcpy(&buf[LWS_PRE], subscribe_str.c_str(), msg_len);

  int result = lws_write(websocket_, &buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
  delete[] buf;

  if (result < 0)
  {
    std::cerr << "Failed to send subscription message" << std::endl;
  }
  else
  {
    std::cout << "Sent subscription for " << contract_ << " order book (5 levels, 0ms interval)" << std::endl;
  }
}

int GateIOWebSocketClient::websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                              void* user, void* in, size_t len)
{
  GateIOWebSocketClient* client = static_cast<GateIOWebSocketClient*>(lws_wsi_user(wsi));
  if (!client)
  {
    return 0;
  }

  switch (reason)
  {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      std::cout << "Gate.io WebSocket connection established" << std::endl;
      client->connected_ = true;
      if (client->connection_callback_)
      {
        client->connection_callback_();
      }
      // Send ping first, then subscribe
      client->sendPing();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      client->sendSubscription();
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
      std::cerr << "Gate.io WebSocket connection error" << std::endl;
      client->connected_ = false;
      if (client->error_callback_)
      {
        client->error_callback_("Connection error");
      }
      break;

    case LWS_CALLBACK_CLOSED:
      std::cout << "Gate.io WebSocket connection closed" << std::endl;
      client->connected_ = false;
      break;

    default:
      break;
  }

  return 0;
}

void GateIOWebSocketClient::processMessage(const std::string& message)
{
  // Gate.io WebSocket Message Format:
  // - Connection: wss://fx-ws.gateio.ws/v4/ws/usdt (USDT futures endpoint)
  // - Subscription: {"channel":"futures.order_book","event":"subscribe","payload":["BTC_USDT","5","0"]}
  // - Response Format: {"channel":"futures.order_book","event":"all","result":{...},"time":timestamp}
  // - Order Book Data: {"bids":[{"p":"price","s":size}],"asks":[{"p":"price","s":size}]}
  // - Update Type: Full snapshots (complete 5-level order book each time)
  // - Frequency: Real-time (0ms interval) whenever order book changes

  // Log the raw JSON message immediately upon arrival
  // std::cout << "\n=== Gate.io WebSocket Message Received ===" << std::endl;
  // std::cout << "Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
  //           << std::endl;
  // std::cout << "Message length: " << message.length() << " bytes" << std::endl;
  // std::cout << "JSON: " << message << std::endl;
  // std::cout << "=========================================" << std::endl;

  try
  {
    auto json = nlohmann::json::parse(message);

    // Check for subscription error
    if (json.contains("error") && !json["error"].is_null())
    {
      std::cerr << "❌ Gate.io subscription error: " << json["error"] << std::endl;
      return;
    }

    // Check for subscription confirmation
    if (json.contains("error") && json["error"].is_null() &&
        json.contains("result") && json["result"].contains("status") &&
        json["result"]["status"] == "success")
    {
      std::cout << "✅ Successfully subscribed to Gate.io order book updates!" << std::endl;
      return;
    }

    // Check for ping/pong response
    if (json.contains("channel") && (json["channel"] == "futures.ping" || json["channel"] == "futures.pong"))
    {
      std::cout << "📡 Received pong response from Gate.io" << std::endl;
      return;
    }

    // Check for order book update (both "update" and "all" events)
    if (json.contains("channel") && json["channel"] == "futures.order_book" &&
        json.contains("event") && (json["event"] == "update" || json["event"] == "all") &&
        json.contains("result"))
    {
      parseOrderBookUpdate(json["result"]);
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing Gate.io WebSocket message: " << e.what() << std::endl;
    std::cerr << "Message: " << message << std::endl;
  }
}

void GateIOWebSocketClient::parseOrderBookUpdate(const nlohmann::json& json)
{
  try
  {
    GateOrderBookUpdate update;

    update.timestamp = json["t"];
    update.contract = json["contract"];
    update.id = json["id"];
    update.websocket_receive_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

    // Debug: Log timestamp comparison for troubleshooting
    // std::cout << "[DEBUG] Exchange timestamp: " << update.timestamp << std::endl;
    // std::cout << "[DEBUG] WebSocket receive timestamp: " << update.websocket_receive_timestamp << std::endl;
    // std::cout << "[DEBUG] Difference: " << (update.websocket_receive_timestamp - update.timestamp) << "ms" << std::endl;

    // Debug info for parsing
    // std::cout << "\n[DEBUG] 🔍 Parsing Gate.io order book update:" << std::endl;
    // std::cout << "[DEBUG]    Timestamp: " << update.timestamp << std::endl;
    // std::cout << "[DEBUG]    Contract: " << update.contract << std::endl;
    // std::cout << "[DEBUG]    ID: " << update.id << std::endl;

    // Parse bids - Gate.io format: [{"p":"price","s":size}, ...]
    if (json.contains("bids"))
    {
      // std::cout << "[DEBUG]    Parsing " << json["bids"].size() << " bids:" << std::endl;
      for (const auto& bid : json["bids"])
      {
        if (bid.contains("p") && bid.contains("s"))
        {
          std::string price = bid["p"].get<std::string>();
          std::string quantity = bid["s"].is_string() ? bid["s"].get<std::string>() : std::to_string(bid["s"].get<int>());
          // std::cout << "[DEBUG]      Raw Bid: " << price << " @ " << quantity << std::endl;
          update.bids.emplace_back(price, quantity);
        }
      }
    }

    // Parse asks - Gate.io format: [{"p":"price","s":size}, ...]
    if (json.contains("asks"))
    {
      // std::cout << "[DEBUG]    Parsing " << json["asks"].size() << " asks:" << std::endl;
      for (const auto& ask : json["asks"])
      {
        if (ask.contains("p") && ask.contains("s"))
        {
          std::string price = ask["p"].get<std::string>();
          std::string quantity = ask["s"].is_string() ? ask["s"].get<std::string>() : std::to_string(ask["s"].get<int>());
          // std::cout << "[DEBUG]      Raw Ask: " << price << " @ " << quantity << std::endl;
          update.asks.emplace_back(price, quantity);
        }
      }
    }

    if (orderbook_callback_)
    {
      orderbook_callback_(update);
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing Gate.io order book update: " << e.what() << std::endl;
  }
}

}  // namespace demo
