/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#pragma once

#include <curl/curl.h>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace demo
{

struct OrderBookSnapshot
{
  std::string symbol;
  int64_t lastUpdateId;
  std::vector<std::pair<std::string, std::string>> bids;  // [price, quantity]
  std::vector<std::pair<std::string, std::string>> asks;  // [price, quantity]
};

class BinanceRestClient
{
 public:
  BinanceRestClient();
  ~BinanceRestClient();

  // Non-copyable, non-movable
  BinanceRestClient(const BinanceRestClient&) = delete;
  BinanceRestClient& operator=(const BinanceRestClient&) = delete;
  BinanceRestClient(BinanceRestClient&&) = delete;
  BinanceRestClient& operator=(BinanceRestClient&&) = delete;

  // Initialize curl
  bool initialize();

  // Cleanup curl
  void cleanup();

  // Get order book snapshot for a symbol
  // Returns true on success, false on failure
  bool getOrderBookSnapshot(const std::string& symbol, int limit, OrderBookSnapshot& snapshot);

 private:
  CURL* curl_;
  bool initialized_;

  // Callback for writing response data
  static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* response);

  // Parse JSON response into OrderBookSnapshot
  bool parseOrderBookResponse(const std::string& response, OrderBookSnapshot& snapshot);
};

}  // namespace demo
