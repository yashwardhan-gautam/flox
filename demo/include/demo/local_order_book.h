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
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace demo
{

struct OrderBookLevel
{
  std::string price;
  std::string quantity;

  OrderBookLevel(const std::string& p, const std::string& q) : price(p), quantity(q) {}
};

class LocalOrderBook
{
 public:
  LocalOrderBook(const std::string& symbol);
  ~LocalOrderBook();

  // Non-copyable, non-movable
  LocalOrderBook(const LocalOrderBook&) = delete;
  LocalOrderBook& operator=(const LocalOrderBook&) = delete;
  LocalOrderBook(LocalOrderBook&&) = delete;
  LocalOrderBook& operator=(LocalOrderBook&&) = delete;

  bool initialize();
  void start();
  void stop();
  void printOrderBook();

 private:
  std::string symbol_;
  std::atomic<bool> running_;
  std::thread websocket_thread_;

  // Order book data
  std::map<double, std::string, std::greater<double>> bids_;  // price -> quantity (descending)
  std::map<double, std::string> asks_;                        // price -> quantity (ascending)
  std::mutex order_book_mutex_;

  // WebSocket context
  struct lws_context* context_;
  struct lws* wsi_;

  // WebSocket callbacks
  static int callback_binance(struct lws* wsi, enum lws_callback_reasons reason,
                              void* user, void* in, size_t len);

  void processPartialDepthUpdate(const std::string& message);
  void updateOrderBook(const std::vector<std::vector<std::string>>& bids,
                       const std::vector<std::vector<std::string>>& asks);
  void runWebSocket();
};

}  // namespace demo
