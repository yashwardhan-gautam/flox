/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "local_order_book.h"
#include <json/json.h>
#include <algorithm>
#include <sstream>

namespace demo
{

LocalOrderBook::LocalOrderBook(const std::string& symbol)
    : symbol_(symbol), running_(false), context_(nullptr), wsi_(nullptr)
{
}

LocalOrderBook::~LocalOrderBook()
{
  stop();
}

bool LocalOrderBook::initialize()
{
  // Define WebSocket protocol
  static struct lws_protocols protocols[] = {
      {
          "binance",
          LocalOrderBook::callback_binance,
          0,
          65536,
      },
      {nullptr, nullptr, 0, 0}};

  // Initialize libwebsockets
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

void LocalOrderBook::start()
{
  if (running_)
  {
    return;
  }

  running_ = true;
  websocket_thread_ = std::thread(&LocalOrderBook::runWebSocket, this);
}

void LocalOrderBook::stop()
{
  if (!running_)
  {
    return;
  }

  running_ = false;

  if (websocket_thread_.joinable())
  {
    websocket_thread_.join();
  }

  if (context_)
  {
    lws_context_destroy(context_);
    context_ = nullptr;
  }
}

void LocalOrderBook::runWebSocket()
{
  // Create WebSocket connection info
  struct lws_client_connect_info ccinfo;
  memset(&ccinfo, 0, sizeof(ccinfo));

  ccinfo.context = context_;
  ccinfo.address = "fstream.binance.com";
  ccinfo.port = 443;
  ccinfo.path = "/ws/btcusdt@partialBookDepth20@100ms";
  ccinfo.host = ccinfo.address;
  ccinfo.origin = ccinfo.address;
  ccinfo.protocol = "binance";
  ccinfo.ssl_connection = LCCSCF_USE_SSL;
  ccinfo.userdata = this;

  // Connect to WebSocket
  wsi_ = lws_client_connect_via_info(&ccinfo);
  if (!wsi_)
  {
    std::cerr << "Failed to connect to Binance WebSocket" << std::endl;
    return;
  }

  // Run the event loop
  while (running_)
  {
    int ret = lws_service(context_, 50);  // 50ms timeout
    if (ret < 0)
    {
      std::cerr << "WebSocket service error: " << ret << std::endl;
      break;
    }
  }
}

int LocalOrderBook::callback_binance(struct lws* wsi, enum lws_callback_reasons reason,
                                     void* user, void* in, size_t len)
{
  LocalOrderBook* self = static_cast<LocalOrderBook*>(user);

  switch (reason)
  {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      std::cout << "Connected to Binance WebSocket" << std::endl;
      break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
      if (self && in && len > 0)
      {
        std::string message(static_cast<char*>(in), len);
        self->processPartialDepthUpdate(message);
      }
      break;

    case LWS_CALLBACK_CLIENT_CLOSED:
      std::cout << "WebSocket connection closed" << std::endl;
      break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      std::cerr << "WebSocket connection error" << std::endl;
      break;

    default:
      break;
  }

  return 0;
}

void LocalOrderBook::processPartialDepthUpdate(const std::string& message)
{
  try
  {
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(message, root))
    {
      std::cerr << "Failed to parse JSON: " << reader.getFormattedErrorMessages() << std::endl;
      return;
    }

    // Check if this is a partial depth update
    if (!root.isMember("bids") || !root.isMember("asks"))
    {
      return;
    }

    // Parse bids
    std::vector<std::vector<std::string>> bids;
    const Json::Value& bidsJson = root["bids"];
    for (const auto& bid : bidsJson)
    {
      if (bid.isArray() && bid.size() >= 2)
      {
        bids.push_back({bid[0].asString(), bid[1].asString()});
      }
    }

    // Parse asks
    std::vector<std::vector<std::string>> asks;
    const Json::Value& asksJson = root["asks"];
    for (const auto& ask : asksJson)
    {
      if (ask.isArray() && ask.size() >= 2)
      {
        asks.push_back({ask[0].asString(), ask[1].asString()});
      }
    }

    // Update order book
    updateOrderBook(bids, asks);

    // Print updated order book
    printOrderBook();
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error processing partial depth update: " << e.what() << std::endl;
  }
}

void LocalOrderBook::updateOrderBook(const std::vector<std::vector<std::string>>& bids,
                                     const std::vector<std::vector<std::string>>& asks)
{
  std::lock_guard<std::mutex> lock(order_book_mutex_);

  // Update bids
  for (const auto& bid : bids)
  {
    double price = std::stod(bid[0]);
    const std::string& quantity = bid[1];

    if (std::stod(quantity) == 0.0)
    {
      // Remove level if quantity is 0
      bids_.erase(price);
    }
    else
    {
      // Update level
      bids_[price] = quantity;
    }
  }

  // Update asks
  for (const auto& ask : asks)
  {
    double price = std::stod(ask[0]);
    const std::string& quantity = ask[1];

    if (std::stod(quantity) == 0.0)
    {
      // Remove level if quantity is 0
      asks_.erase(price);
    }
    else
    {
      // Update level
      asks_[price] = quantity;
    }
  }
}

void LocalOrderBook::printOrderBook()
{
  std::lock_guard<std::mutex> lock(order_book_mutex_);

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::cout << "\n=== Binance Futures WebSocket Order Book Update ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << "Symbol: " << symbol_ << std::endl;

  std::cout << "\nAsks (Sell Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;

  // Print top 10 asks (lowest price first)
  int ask_count = 0;
  for (const auto& ask : asks_)
  {
    if (ask_count >= 10)
    {
      break;
    }
    std::cout << ask.first << "\t\t" << ask.second << std::endl;
    ask_count++;
  }

  std::cout << "\n--- SPREAD ---" << std::endl;

  std::cout << "\nBids (Buy Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;

  // Print top 10 bids (highest price first)
  int bid_count = 0;
  for (const auto& bid : bids_)
  {
    if (bid_count >= 10)
    {
      break;
    }
    std::cout << bid.first << "\t\t" << bid.second << std::endl;
    bid_count++;
  }

  // Calculate spread if we have both bids and asks
  if (!bids_.empty() && !asks_.empty())
  {
    try
    {
      double bestBid = bids_.begin()->first;  // Highest bid
      double bestAsk = asks_.begin()->first;  // Lowest ask
      double spread = bestAsk - bestBid;
      double spreadBps = (spread / bestBid) * 10000;  // basis points

      std::cout << "\nSpread: $" << spread << " (" << spreadBps << " bps)" << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cout << "\nCould not calculate spread: " << e.what() << std::endl;
    }
  }

  std::cout << "===========================================" << std::endl;
}

}  // namespace demo
