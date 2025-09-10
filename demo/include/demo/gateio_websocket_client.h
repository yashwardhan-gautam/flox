/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#pragma once

#include <libwebsockets.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>

namespace demo
{

struct GateOrderBookUpdate
{
  int64_t timestamp;                                      // t (exchange timestamp)
  std::string contract;                                   // s (contract symbol)
  int64_t id;                                             // id
  std::vector<std::pair<std::string, std::string>> bids;  // [price, quantity]
  std::vector<std::pair<std::string, std::string>> asks;  // [price, quantity]
  int64_t websocket_receive_timestamp;                    // timestamp when WebSocket message was received
};

struct ContractInfo
{
  std::string name;
  double quanto_multiplier;
  std::string type;
  std::string settle;
};

class GateIOWebSocketClient
{
 public:
  using OrderBookUpdateCallback = std::function<void(const GateOrderBookUpdate&)>;
  using ConnectionCallback = std::function<void()>;
  using ErrorCallback = std::function<void(const std::string&)>;

  GateIOWebSocketClient();
  ~GateIOWebSocketClient();

  // Non-copyable, non-movable
  GateIOWebSocketClient(const GateIOWebSocketClient&) = delete;
  GateIOWebSocketClient& operator=(const GateIOWebSocketClient&) = delete;
  GateIOWebSocketClient(GateIOWebSocketClient&&) = delete;
  GateIOWebSocketClient& operator=(GateIOWebSocketClient&&) = delete;

  // Initialize WebSocket client
  bool initialize();

  // Connect to Gate.io WebSocket stream
  bool connect(const std::string& contract);

  // Set callbacks
  void setOrderBookUpdateCallback(OrderBookUpdateCallback callback);
  void setConnectionCallback(ConnectionCallback callback);
  void setErrorCallback(ErrorCallback callback);

  // Start the WebSocket event loop (blocking)
  void run();

  // Stop the WebSocket client
  void stop();

  // Check if connected
  bool isConnected() const { return connected_; }

  // Get contract multiplier
  double getQuantoMultiplier() const { return contract_info_.quanto_multiplier; }

 private:
  struct lws_context* context_;
  struct lws* websocket_;
  std::atomic<bool> connected_;
  std::atomic<bool> should_stop_;
  std::string contract_;
  std::string subscription_id_;
  ContractInfo contract_info_;

  // Callbacks
  OrderBookUpdateCallback orderbook_callback_;
  ConnectionCallback connection_callback_;
  ErrorCallback error_callback_;

  // Message buffer
  std::queue<std::string> message_queue_;
  std::mutex queue_mutex_;

  // libwebsockets callback
  static int websocket_callback(struct lws* wsi, enum lws_callback_reasons reason,
                                void* user, void* in, size_t len);

  // Message processing
  void processMessage(const std::string& message);
  void parseOrderBookUpdate(const nlohmann::json& json);
  void sendSubscription();
  void sendPing();

  // Contract info fetching
  bool fetchContractInfo(const std::string& contract);

  // Helper to generate request ID
  std::string generateRequestId();
};

}  // namespace demo
