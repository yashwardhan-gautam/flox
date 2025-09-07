/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "demo/binance_rest_client.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace demo;

void printOrderBookSnapshot(const OrderBookSnapshot& snapshot)
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  
  std::cout << "\n=== Binance Futures Order Book Snapshot ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << "Symbol: " << snapshot.symbol << std::endl;
  std::cout << "Last Update ID: " << snapshot.lastUpdateId << std::endl;
  
  std::cout << "\nAsks (Sell Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;
  
  // Print asks in reverse order (highest price first)
  for (auto it = snapshot.asks.rbegin(); it != snapshot.asks.rend() && it != snapshot.asks.rbegin() + 5; ++it)
  {
    std::cout << it->first << "\t\t" << it->second << std::endl;
  }
  
  std::cout << "\n--- SPREAD ---" << std::endl;
  
  std::cout << "\nBids (Buy Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;
  
  // Print top 5 bids (highest price first)
  for (size_t i = 0; i < std::min(size_t(5), snapshot.bids.size()); ++i)
  {
    std::cout << snapshot.bids[i].first << "\t\t" << snapshot.bids[i].second << std::endl;
  }
  
  // Calculate spread if we have both bids and asks
  if (!snapshot.bids.empty() && !snapshot.asks.empty())
  {
    try
    {
      double bestBid = std::stod(snapshot.bids[0].first);
      double bestAsk = std::stod(snapshot.asks[0].first);
      double spread = bestAsk - bestBid;
      double spreadBps = (spread / bestBid) * 10000; // basis points
      
      std::cout << "\nSpread: $" << spread << " (" << spreadBps << " bps)" << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cout << "\nCould not calculate spread: " << e.what() << std::endl;
    }
  }
  
  std::cout << "===========================================" << std::endl;
}

int main()
{
  std::cout << "🚀 Binance Futures REST API Demo" << std::endl;
  std::cout << "Fetching BTCUSDT perpetual order book every 10 seconds..." << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;

  BinanceRestClient client;
  
  if (!client.initialize())
  {
    std::cerr << "Failed to initialize Binance REST client" << std::endl;
    return 1;
  }

  const std::string symbol = "BTCUSDT";
  const int limit = 20; // Get top 20 levels
  
  while (true)
  {
    OrderBookSnapshot snapshot;
    
    std::cout << "\nFetching order book snapshot..." << std::endl;
    
    if (client.getOrderBookSnapshot(symbol, limit, snapshot))
    {
      printOrderBookSnapshot(snapshot);
    }
    else
    {
      std::cerr << "Failed to fetch order book snapshot" << std::endl;
    }
    
    // Wait 10 seconds before next request
    std::cout << "\nWaiting 10 seconds before next request..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  return 0;
}
