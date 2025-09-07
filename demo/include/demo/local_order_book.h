/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#pragma once

#include "binance_websocket_client.h"
#include <map>
#include <string>
#include <mutex>
#include <atomic>

namespace demo
{

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

  // Initialize the order book 
  bool initialize();

  // Start maintaining the order book
  void start();

  // Stop the order book maintenance
  void stop();

  // Get current order book state (thread-safe)
  struct OrderBookState
  {
    std::map<double, double> bids;  // price -> quantity
    std::map<double, double> asks;  // price -> quantity
    int64_t lastUpdateId;
  };
  
  OrderBookState getOrderBookState() const;

  // Print top N levels immediately
  void printTopLevels(int levels = 10) const;

 private:
  std::string symbol_;
  BinanceWebSocketClient ws_client_;
  
  // Order book data (protected by mutex)
  mutable std::mutex book_mutex_;
  std::map<double, double> bids_;  // price -> quantity
  std::map<double, double> asks_;  // price -> quantity
  int64_t last_update_id_;
  
  // Control flags
  std::atomic<bool> running_;
  
  // Callbacks
  void onDepthUpdate(const DepthUpdate& update);
  void onWebSocketConnected();
  void onWebSocketError(const std::string& error);
  
  // Simple order book management using partial depth snapshots
  void processDepthUpdate(const DepthUpdate& update);
};

}  // namespace demo
