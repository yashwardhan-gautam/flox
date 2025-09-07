/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "demo/binance_rest_client.h"

#include <iostream>
#include <sstream>

namespace demo
{

BinanceRestClient::BinanceRestClient() : curl_(nullptr), initialized_(false) {}

BinanceRestClient::~BinanceRestClient()
{
  cleanup();
}

bool BinanceRestClient::initialize()
{
  if (initialized_)
  {
    return true;
  }

  // Initialize curl globally (should be done once per application)
  CURLcode global_init_result = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (global_init_result != CURLE_OK)
  {
    std::cerr << "Failed to initialize curl globally: " << curl_easy_strerror(global_init_result)
              << std::endl;
    return false;
  }

  // Create a curl handle
  curl_ = curl_easy_init();
  if (!curl_)
  {
    std::cerr << "Failed to initialize curl handle" << std::endl;
    curl_global_cleanup();
    return false;
  }

  // Set common options
  curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl_, CURLOPT_USERAGENT, "Flox-BinanceDemo/1.0");
  curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);

  initialized_ = true;
  return true;
}

void BinanceRestClient::cleanup()
{
  if (curl_)
  {
    curl_easy_cleanup(curl_);
    curl_ = nullptr;
  }

  if (initialized_)
  {
    curl_global_cleanup();
    initialized_ = false;
  }
}

size_t BinanceRestClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* response)
{
  size_t total_size = size * nmemb;
  response->append(static_cast<char*>(contents), total_size);
  return total_size;
}

bool BinanceRestClient::getOrderBookSnapshot(const std::string& symbol, int limit, OrderBookSnapshot& snapshot)
{
  if (!initialized_ || !curl_)
  {
    std::cerr << "BinanceRestClient not initialized" << std::endl;
    return false;
  }

  // Construct URL
  std::string url = "https://fapi.binance.com/fapi/v1/depth?symbol=" + symbol + "&limit=" + std::to_string(limit);

  // Response string
  std::string response;

  // Set URL and callback
  curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

  // Perform the request
  CURLcode res = curl_easy_perform(curl_);

  if (res != CURLE_OK)
  {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
    return false;
  }

  // Check HTTP response code
  long response_code;
  curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);

  if (response_code != 200)
  {
    std::cerr << "HTTP request failed with code: " << response_code << std::endl;
    std::cerr << "Response: " << response << std::endl;
    return false;
  }

  // Parse the response
  return parseOrderBookResponse(response, snapshot);
}

bool BinanceRestClient::parseOrderBookResponse(const std::string& response, OrderBookSnapshot& snapshot)
{
  // Simple JSON parsing for the specific Binance API response format
  // This is a basic implementation - in production, you'd use a proper JSON library
  
  try
  {
    // Find lastUpdateId
    size_t lastUpdateIdPos = response.find("\"lastUpdateId\":");
    if (lastUpdateIdPos == std::string::npos)
    {
      std::cerr << "Failed to find lastUpdateId in response" << std::endl;
      return false;
    }
    
    size_t idStart = lastUpdateIdPos + 16; // length of "\"lastUpdateId\":"
    size_t idEnd = response.find(',', idStart);
    if (idEnd == std::string::npos) idEnd = response.find('}', idStart);
    
    std::string idStr = response.substr(idStart, idEnd - idStart);
    snapshot.lastUpdateId = std::stoll(idStr);

    // Parse bids - format: "bids":[["price","quantity"],["price","quantity"],...]
    snapshot.bids.clear();
    size_t bidsPos = response.find("\"bids\":[");
    if (bidsPos != std::string::npos)
    {
      size_t bidsStart = bidsPos + 8; // length of "\"bids\":["
      size_t bidsEnd = response.find("],\"asks\"", bidsStart);
      if (bidsEnd == std::string::npos) bidsEnd = response.find("]}", bidsStart);
      
      std::string bidsStr = response.substr(bidsStart, bidsEnd - bidsStart);
      
      // Parse individual bid entries ["price","quantity"]
      size_t pos = 0;
      while (pos < bidsStr.length())
      {
        size_t entryStart = bidsStr.find('[', pos);
        if (entryStart == std::string::npos) break;
        
        size_t entryEnd = bidsStr.find(']', entryStart);
        if (entryEnd == std::string::npos) break;
        
        std::string entry = bidsStr.substr(entryStart + 1, entryEnd - entryStart - 1);
        
        // Find the two quoted strings: "price","quantity"
        size_t firstQuote = entry.find('"');
        if (firstQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        size_t secondQuote = entry.find('"', firstQuote + 1);
        if (secondQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        size_t thirdQuote = entry.find('"', secondQuote + 1);
        if (thirdQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        size_t fourthQuote = entry.find('"', thirdQuote + 1);
        if (fourthQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        std::string price = entry.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        std::string quantity = entry.substr(thirdQuote + 1, fourthQuote - thirdQuote - 1);
        
        snapshot.bids.emplace_back(price, quantity);
        
        pos = entryEnd + 1;
      }
    }

    // Parse asks - format: "asks":[["price","quantity"],["price","quantity"],...]
    snapshot.asks.clear();
    size_t asksPos = response.find("\"asks\":[");
    if (asksPos != std::string::npos)
    {
      size_t asksStart = asksPos + 8; // length of "\"asks\":["
      size_t asksEnd = response.find("]}", asksStart);
      if (asksEnd == std::string::npos) asksEnd = response.find("],", asksStart);
      
      std::string asksStr = response.substr(asksStart, asksEnd - asksStart);
      
      // Parse individual ask entries ["price","quantity"]
      size_t pos = 0;
      while (pos < asksStr.length())
      {
        size_t entryStart = asksStr.find('[', pos);
        if (entryStart == std::string::npos) break;
        
        size_t entryEnd = asksStr.find(']', entryStart);
        if (entryEnd == std::string::npos) break;
        
        std::string entry = asksStr.substr(entryStart + 1, entryEnd - entryStart - 1);
        
        // Find the two quoted strings: "price","quantity"
        size_t firstQuote = entry.find('"');
        if (firstQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        size_t secondQuote = entry.find('"', firstQuote + 1);
        if (secondQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        size_t thirdQuote = entry.find('"', secondQuote + 1);
        if (thirdQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        size_t fourthQuote = entry.find('"', thirdQuote + 1);
        if (fourthQuote == std::string::npos) { pos = entryEnd + 1; continue; }
        
        std::string price = entry.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        std::string quantity = entry.substr(thirdQuote + 1, fourthQuote - thirdQuote - 1);
        
        snapshot.asks.emplace_back(price, quantity);
        
        pos = entryEnd + 1;
      }
    }

    snapshot.symbol = "BTCUSDT"; // Set the symbol
    return true;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error parsing JSON response: " << e.what() << std::endl;
    return false;
  }
}

}  // namespace demo
