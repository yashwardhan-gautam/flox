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
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>
#include "demo/gateio_websocket_client.h"

using namespace demo;

std::atomic<bool> keep_running(true);

// Local order book to maintain state
std::map<double, std::string, std::greater<double>> bids_;  // price -> quantity (descending)
std::map<double, std::string> asks_;                        // price -> quantity (ascending)

// Global client reference to access multiplier
GateIOWebSocketClient* global_client = nullptr;

void signalHandler(int signal)
{
  std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
  keep_running = false;
}

void printOrderBook(const std::string& contract)
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::cout << "\n=== Gate.io Futures WebSocket Order Book Update ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << "Contract: " << contract << std::endl;

  std::cout << "\nAsks (Sell Orders):" << std::endl;
  std::cout << "Price\t\tSize (USD)" << std::endl;
  std::cout << "-----\t\t---------" << std::endl;

  // Print top 5 asks (lowest price first)
  int ask_count = 0;
  for (const auto& ask : asks_)
  {
    if (ask_count >= 5)
    {
      break;
    }
    // Calculate actual contract size using quanto multiplier
    double actual_size = std::stod(ask.second);
    if (global_client)
    {
      actual_size *= global_client->getQuantoMultiplier();
    }
    std::cout << std::fixed << std::setprecision(1) << ask.first << "\t\t" << std::setprecision(4) << actual_size << std::endl;
    ask_count++;
  }

  std::cout << "\n--- SPREAD ---" << std::endl;

  std::cout << "\nBids (Buy Orders):" << std::endl;
  std::cout << "Price\t\tSize (USD)" << std::endl;
  std::cout << "-----\t\t---------" << std::endl;

  // Print top 5 bids (highest price first)
  int bid_count = 0;
  for (const auto& bid : bids_)
  {
    if (bid_count >= 5)
    {
      break;
    }
    // Calculate actual contract size using quanto multiplier
    double actual_size = std::stod(bid.second);
    if (global_client)
    {
      actual_size *= global_client->getQuantoMultiplier();
    }
    std::cout << std::fixed << std::setprecision(1) << bid.first << "\t\t" << std::setprecision(4) << actual_size << std::endl;
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

      std::cout << "\nSpread: $" << std::fixed << std::setprecision(1) << spread << " (" << std::setprecision(2) << spreadBps << " bps)" << std::endl;
    }
    catch (const std::exception& e)
    {
      std::cout << "\nCould not calculate spread: " << e.what() << std::endl;
    }
  }

  std::cout << "=================================================" << std::endl;
}

void onOrderBookUpdate(const GateOrderBookUpdate& update)
{
  // Clear existing order book (Gate.io sends full snapshots)
  bids_.clear();
  asks_.clear();

  // Update bids
  for (const auto& bid : update.bids)
  {
    double price = std::stod(bid.first);
    const std::string& quantity = bid.second;

    if (std::stod(quantity) > 0.0)
    {
      bids_[price] = quantity;
    }
  }

  // Update asks
  for (const auto& ask : update.asks)
  {
    double price = std::stod(ask.first);
    const std::string& quantity = ask.second;

    if (std::stod(quantity) > 0.0)
    {
      asks_[price] = quantity;
    }
  }

  // Print updated order book
  printOrderBook(update.contract);
}

void onConnection()
{
  std::cout << "✅ Connected to Gate.io Futures WebSocket!" << std::endl;
  std::cout << "Subscribing to BTC_USDT order book (5 levels)..." << std::endl;
}

void onError(const std::string& error)
{
  std::cerr << "❌ Gate.io WebSocket error: " << error << std::endl;
}

int main()
{
  // Set up signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  std::cout << "🚀 Gate.io Futures WebSocket Order Book Demo" << std::endl;
  std::cout << "Connecting to BTC_USDT order book stream..." << std::endl;
  std::cout << "Will log and display 5-level order book on every update" << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;

  GateIOWebSocketClient client;
  global_client = &client;  // Store reference for multiplier access

  // Set up callbacks
  client.setOrderBookUpdateCallback(onOrderBookUpdate);
  client.setConnectionCallback(onConnection);
  client.setErrorCallback(onError);

  if (!client.initialize())
  {
    std::cerr << "Failed to initialize Gate.io WebSocket client" << std::endl;
    return 1;
  }

  if (!client.connect("BTC_USDT"))
  {
    std::cerr << "Failed to connect to Gate.io WebSocket" << std::endl;
    return 1;
  }

  // Start WebSocket in a separate thread
  std::thread ws_thread([&client]()
                        { client.run(); });

  // Wait for connection
  std::this_thread::sleep_for(std::chrono::seconds(3));

  std::cout << "\nWaiting for Gate.io WebSocket updates...\n"
            << std::endl;

  // Main loop - just wait for signals
  while (keep_running && client.isConnected())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nStopping Gate.io WebSocket client..." << std::endl;
  client.stop();

  if (ws_thread.joinable())
  {
    ws_thread.join();
  }

  std::cout << "Gate.io demo completed successfully!" << std::endl;
  return 0;
}
