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
#include <mutex>
#include <thread>
#include "demo/binance_websocket_client.h"

using namespace demo;

std::atomic<bool> keep_running(true);

// Local order book to maintain state
std::map<double, std::string, std::greater<double>> bids_;  // price -> quantity (descending)
std::map<double, std::string> asks_;                        // price -> quantity (ascending)
std::mutex order_book_mutex_;                               // Protect order book access

void signalHandler(int signal)
{
  std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
  keep_running = false;
}

void printOrderBook(long external_latency_ms, long internal_latency_us)
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  // Create local copies of the order book maps for thread-safe printing
  std::map<double, std::string, std::greater<double>> local_bids;
  std::map<double, std::string> local_asks;

  {
    std::lock_guard<std::mutex> lock(order_book_mutex_);
    local_bids = bids_;
    local_asks = asks_;
  }

  std::cout << "\n=== Binance Futures WebSocket Order Book Update ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << "Symbol: BTCUSDT" << std::endl;
  std::cout << "Local Order Book State - Bids: " << local_bids.size() << ", Asks: " << local_asks.size() << std::endl;
  std::cout << "External Latency: " << external_latency_ms << "ms (WebSocket receive - Exchange timestamp)" << std::endl;
  std::cout << "Internal Latency: " << internal_latency_us << "μs (Order book update - WebSocket receive)" << std::endl;

  std::cout << "\nAsks (Sell Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;

  // Print top 5 asks (highest price first - reverse order)
  int ask_count = 0;
  for (auto ask_it = local_asks.rbegin(); ask_it != local_asks.rend() && ask_count < 5; ++ask_it, ++ask_count)
  {
    std::cout << std::fixed << std::setprecision(1) << ask_it->first << "\t\t" << std::setprecision(4) << std::stod(ask_it->second) << std::endl;
  }

  std::cout << "\n--- SPREAD ---" << std::endl;

  std::cout << "\nBids (Buy Orders):" << std::endl;
  std::cout << "Price\t\tQuantity" << std::endl;
  std::cout << "-----\t\t--------" << std::endl;

  // Print top 5 bids (highest price first)
  int bid_count = 0;
  for (const auto& bid : local_bids)
  {
    if (bid_count >= 5)
    {
      break;
    }
    std::cout << std::fixed << std::setprecision(1) << bid.first << "\t\t" << std::setprecision(4) << std::stod(bid.second) << std::endl;
    bid_count++;
  }

  // Calculate spread if we have both bids and asks
  if (!local_bids.empty() && !local_asks.empty())
  {
    try
    {
      double bestBid = local_bids.begin()->first;  // Highest bid
      double bestAsk = local_asks.begin()->first;  // Lowest ask
      double spread = bestAsk - bestBid;
      double spreadBps = (spread / bestBid) * 10000;  // basis points

      std::cout << "\nSpread: $" << std::fixed << std::setprecision(1) << spread << " (" << std::setprecision(2) << spreadBps << " bps)" << std::endl;
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
  // Binance WebSocket Update Format:
  // - Stream: btcusdt@depth5@0ms (partial book depth, 5 levels, real-time)
  // - Type: SNAPSHOT updates (not incremental) - each message contains complete 5-level order book
  // - Format: {"b":[["price","qty"],...], "a":[["price","qty"],...]}
  // - Example: {"b":[["113622.60","14.642"],["113622.50","0.276"]], "a":[["113622.70","0.660"]]}
  // - Frequency: Real-time updates whenever top 5 levels change
  // - Processing: Clear existing maps, then rebuild with new snapshot data

  // Calculate external latency (WebSocket receive time - Exchange timestamp)
  auto external_latency_ms = update.websocket_receive_timestamp - update.eventTime;

  // std::cout << "\n[DEBUG] Processing depth update - Bids: " << update.bids.size() << ", Asks: " << update.asks.size() << std::endl;

  {
    std::lock_guard<std::mutex> lock(order_book_mutex_);

    // Clear existing order book (Binance sends snapshots, not incremental updates)
    // std::cout << "[DEBUG] Clearing existing order book - Previous Bids: " << bids_.size() << ", Asks: " << asks_.size() << std::endl;
    bids_.clear();
    asks_.clear();

    // Update bids
    for (const auto& bid : update.bids)
    {
      double price = std::stod(bid.first);
      const std::string& quantity = bid.second;
      // std::cout << "[DEBUG] Bid: " << bid.first << " @ " << quantity << std::endl;

      if (std::stod(quantity) > 0.0)
      {
        // Add level (no need to check for 0 since we cleared the maps)
        bids_[price] = quantity;
      }
    }

    // Update asks
    for (const auto& ask : update.asks)
    {
      double price = std::stod(ask.first);
      const std::string& quantity = ask.second;
      // std::cout << "[DEBUG] Ask: " << ask.first << " @ " << quantity << std::endl;

      if (std::stod(quantity) > 0.0)
      {
        // Add level (no need to check for 0 since we cleared the maps)
        asks_[price] = quantity;
      }
    }

    // std::cout << "[DEBUG] Order book after update - Bids: " << bids_.size() << ", Asks: " << asks_.size() << std::endl;
  }

  // Calculate internal latency (Order book update complete time - WebSocket receive time)
  auto orderbook_update_complete_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count();
  auto websocket_receive_time_us = update.websocket_receive_timestamp * 1000;  // Convert ms to microseconds
  auto internal_latency_us = orderbook_update_complete_time - websocket_receive_time_us;

  // Print updated order book with latency information
  printOrderBook(external_latency_ms, internal_latency_us);
}

void onConnection()
{
  std::cout << "Connected to Binance Futures WebSocket!" << std::endl;
  std::cout << "Subscribing to BTCUSDT depth stream..." << std::endl;
}

void onError(const std::string& error)
{
  std::cerr << "WebSocket error: " << error << std::endl;
}

int main()
{
  // Set up signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Clear any stale order book data from previous runs
  bids_.clear();
  asks_.clear();

  std::cout << "Binance Futures WebSocket Depth Stream Demo" << std::endl;
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
