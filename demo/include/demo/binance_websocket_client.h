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

struct DepthUpdate
{
  int64_t eventTime;                                      // E (exchange timestamp)
  int64_t transactionTime;                                // T
  int64_t firstUpdateId;                                  // U
  int64_t finalUpdateId;                                  // u
  int64_t prevFinalUpdateId;                              // pu
  std::vector<std::pair<std::string, std::string>> bids;  // [price, quantity]
  std::vector<std::pair<std::string, std::string>> asks;  // [price, quantity]
  int64_t websocket_receive_timestamp;                    // timestamp when WebSocket message was received
};

class BinanceWebSocketClient
{
 public:
  using DepthUpdateCallback = std::function<void(const DepthUpdate&)>;
  using ConnectionCallback = std::function<void()>;
  using ErrorCallback = std::function<void(const std::string&)>;

  BinanceWebSocketClient();
  ~BinanceWebSocketClient();

  // Non-copyable, non-movable
  BinanceWebSocketClient(const BinanceWebSocketClient&) = delete;
  BinanceWebSocketClient& operator=(const BinanceWebSocketClient&) = delete;
  BinanceWebSocketClient(BinanceWebSocketClient&&) = delete;
  BinanceWebSocketClient& operator=(BinanceWebSocketClient&&) = delete;

  // Initialize WebSocket client
  bool initialize();

  // Connect to Binance WebSocket stream
  bool connect(const std::string& symbol);

  // Set callbacks
  void setDepthUpdateCallback(DepthUpdateCallback callback);
  void setConnectionCallback(ConnectionCallback callback);
  void setErrorCallback(ErrorCallback callback);

  // Start the WebSocket event loop (blocking)
  void run();

  // Stop the WebSocket client
  void stop();

  // Check if connected
  bool isConnected() const { return connected_; }

 private:
  struct lws_context* context_;
  struct lws* websocket_;
  std::atomic<bool> connected_;
  std::atomic<bool> should_stop_;
  std::string stream_name_;
  std::string path_;  // Store the full WebSocket path

  // Callbacks
  DepthUpdateCallback depth_callback_;
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
  void parseDepthUpdate(const nlohmann::json& json);
};

}  // namespace demo
