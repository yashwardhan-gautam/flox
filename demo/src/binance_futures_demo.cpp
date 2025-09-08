/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <thread>
#include "demo/binance_websocket_client.h"

using namespace demo;

std::atomic<bool> keep_running(true);

// Local order book to maintain state
std::map<double, std::string, std::greater<double>> bids_;  // price -> quantity (descending)
std::map<double, std::string> asks_;                        // price -> quantity (ascending)

void signalHandler(int signal)
{
  std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
  keep_running = false;
}

void printOrderBook()
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::cout << "\n=== Binance Futures WebSocket Order Book Update ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << "Symbol: BTCUSDT" << std::endl;

  std::cout << "\nAsks (Sell Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;

  // Print top 5 asks (lowest price first)
  int ask_count = 0;
  for (const auto& ask : asks_)
  {
    if (ask_count >= 5)
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

  // Print top 5 bids (highest price first)
  int bid_count = 0;
  for (const auto& bid : bids_)
  {
    if (bid_count >= 5)
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

void onDepthUpdate(const DepthUpdate& update)
{
  // Update bids
  for (const auto& bid : update.bids)
  {
    double price = std::stod(bid.first);
    const std::string& quantity = bid.second;

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
  for (const auto& ask : update.asks)
  {
    double price = std::stod(ask.first);
    const std::string& quantity = ask.second;

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

  // Print updated order book
  printOrderBook();
}

void onConnection()
{
  std::cout << "✅ Connected to Binance Futures WebSocket!" << std::endl;
  std::cout << "Subscribing to BTCUSDT depth stream..." << std::endl;
}

void onError(const std::string& error)
{
  std::cerr << "❌ WebSocket error: " << error << std::endl;
}

int main()
{
  // Set up signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  std::cout << "🚀 Binance Futures WebSocket Depth Stream Demo" << std::endl;
  std::cout << "Connecting to BTCUSDT depth stream..." << std::endl;
  std::cout << "Will log and display order book on every WebSocket update" << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;

  BinanceWebSocketClient client;

  // Set up callbacks
  client.setDepthUpdateCallback(onDepthUpdate);
  client.setConnectionCallback(onConnection);
  client.setErrorCallback(onError);

  if (!client.initialize())
  {
    std::cerr << "Failed to initialize WebSocket client" << std::endl;
    return 1;
  }

  if (!client.connect("BTCUSDT"))
  {
    std::cerr << "Failed to connect to Binance WebSocket" << std::endl;
    return 1;
  }

  // Start WebSocket in a separate thread
  std::thread ws_thread([&client]()
                        { client.run(); });

  // Wait for connection
  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "\nWaiting for WebSocket updates...\n"
            << std::endl;

  // Main loop - just wait for signals
  while (keep_running && client.isConnected())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nStopping WebSocket client..." << std::endl;
  client.stop();

  if (ws_thread.joinable())
  {
    ws_thread.join();
  }

  std::cout << "Demo completed successfully!" << std::endl;
  return 0;
}
