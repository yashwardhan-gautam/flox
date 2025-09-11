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
#include "demo/gateio_websocket_client.h"

using namespace demo;

std::atomic<bool> keep_running(true);

// Binance order book data
std::map<double, std::string, std::greater<double>> binance_bids_;  // price -> quantity (descending)
std::map<double, std::string> binance_asks_;                        // price -> quantity (ascending)
std::mutex binance_mutex_;

// Gate.io order book data
std::map<double, std::string, std::greater<double>> gateio_bids_;  // price -> quantity (descending)
std::map<double, std::string> gateio_asks_;                        // price -> quantity (ascending)
std::mutex gateio_mutex_;

// Global client reference for Gate.io multiplier
GateIOWebSocketClient* global_gateio_client = nullptr;

// Latency tracking variables
struct LatencyStats
{
  double external_latency_ms = 0.0;
  double internal_latency_us = 0.0;
  uint64_t last_update_time = 0;
};

LatencyStats binance_latency_stats_;
LatencyStats gateio_latency_stats_;
std::mutex latency_mutex_;

void signalHandler(int signal)
{
  std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
  keep_running = false;
}

void printDualOrderBook()
{
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  // Create local copies for thread-safe printing
  std::map<double, std::string, std::greater<double>> local_binance_bids;
  std::map<double, std::string> local_binance_asks;
  std::map<double, std::string, std::greater<double>> local_gateio_bids;
  std::map<double, std::string> local_gateio_asks;

  {
    std::lock_guard<std::mutex> binance_lock(binance_mutex_);
    local_binance_bids = binance_bids_;
    local_binance_asks = binance_asks_;
  }

  {
    std::lock_guard<std::mutex> gateio_lock(gateio_mutex_);
    local_gateio_bids = gateio_bids_;
    local_gateio_asks = gateio_asks_;
  }

  // Clear screen for better display
  std::cout << "\033[2J\033[H";  // ANSI escape codes to clear screen and move cursor to top

  std::cout << "=== DUAL EXCHANGE ORDER BOOK COMPARISON ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << std::endl;

  // Display latency statistics
  LatencyStats local_binance_latency;
  LatencyStats local_gateio_latency;

  {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    local_binance_latency = binance_latency_stats_;
    local_gateio_latency = gateio_latency_stats_;
  }

  // std::cout << "LATENCY STATISTICS" << std::endl;
  // std::cout << std::left << std::setw(50) << "BINANCE FUTURES"
  //           << " | " << std::setw(50) << "GATE.IO FUTURES" << std::endl;
  // std::cout << std::string(50, '-') << " | " << std::string(50, '-') << std::endl;

  // std::ostringstream binance_external_ss, binance_internal_ss;
  // std::ostringstream gateio_external_ss, gateio_internal_ss;

  // binance_external_ss << "External: " << std::fixed << std::setprecision(1)
  //                     << local_binance_latency.external_latency_ms << " ms";
  // binance_internal_ss << "Internal: " << std::fixed << std::setprecision(0)
  //                     << local_binance_latency.internal_latency_us << " μs";

  // gateio_external_ss << "External: " << std::fixed << std::setprecision(1)
  //                    << local_gateio_latency.external_latency_ms << " ms";
  // gateio_internal_ss << "Internal: " << std::fixed << std::setprecision(0)
  //                    << local_gateio_latency.internal_latency_us << " μs";

  // std::cout << std::left << std::setw(50) << binance_external_ss.str()
  //           << " | " << std::setw(50) << gateio_external_ss.str() << std::endl;
  // std::cout << std::left << std::setw(50) << binance_internal_ss.str()
  //           << " | " << std::setw(50) << gateio_internal_ss.str() << std::endl;
  // std::cout << std::endl;

  // Header
  std::cout << std::left << std::setw(50) << "BINANCE FUTURES (BTCUSDT)"
            << " | " << std::setw(50) << "GATE.IO FUTURES (BTC_USDT)" << std::endl;
  std::cout << std::string(50, '-') << " | " << std::string(50, '-') << std::endl;

  // Order book state
  std::cout << std::left << std::setw(50) << ("Bids: " + std::to_string(local_binance_bids.size()) + ", Asks: " + std::to_string(local_binance_asks.size()))
            << " | " << std::setw(50) << ("Bids: " + std::to_string(local_gateio_bids.size()) + ", Asks: " + std::to_string(local_gateio_asks.size())) << std::endl;
  std::cout << std::endl;

  // Asks section
  std::cout << std::left << std::setw(50) << "ASKS (Sell Orders):"
            << " | " << std::setw(50) << "ASKS (Sell Orders):" << std::endl;
  std::cout << std::left << std::setw(50) << "Price        Quantity     Cumulative"
            << " | " << std::setw(50) << "Price        Size (USD)   Cumulative" << std::endl;
  std::cout << std::left << std::setw(50) << "-----        --------     ----------"
            << " | " << std::setw(50) << "-----        ---------    ----------" << std::endl;

  // Pre-calculate cumulative sums for asks (from lowest ask price upward)
  std::vector<std::pair<double, double>> binance_ask_cumulatives;
  std::vector<std::pair<double, double>> gateio_ask_cumulatives;

  // Calculate Binance ask cumulatives (from lowest price upward)
  double binance_ask_running_total = 0.0;
  for (const auto& ask : local_binance_asks)
  {
    double quantity = std::stod(ask.second);
    binance_ask_running_total += quantity;
    binance_ask_cumulatives.push_back({ask.first, binance_ask_running_total});
  }

  // Calculate Gate.io ask cumulatives (from lowest price upward)
  double gateio_ask_running_total = 0.0;
  for (const auto& ask : local_gateio_asks)
  {
    double actual_size = std::stod(ask.second);
    if (global_gateio_client)
    {
      actual_size *= global_gateio_client->getQuantoMultiplier();
    }
    gateio_ask_running_total += actual_size;
    gateio_ask_cumulatives.push_back({ask.first, gateio_ask_running_total});
  }

  // Print top 5 asks side by side (reverse order - highest price first, but with correct cumulatives)
  auto binance_ask_it = local_binance_asks.rbegin();
  auto gateio_ask_it = local_gateio_asks.rbegin();

  for (int i = 0; i < 5; ++i)
  {
    std::string binance_line = "";
    std::string gateio_line = "";

    // Binance ask
    if (binance_ask_it != local_binance_asks.rend())
    {
      double quantity = std::stod(binance_ask_it->second);

      // Find the cumulative for this price level
      double cumulative = 0.0;
      for (const auto& cum : binance_ask_cumulatives)
      {
        if (cum.first == binance_ask_it->first)
        {
          cumulative = cum.second;
          break;
        }
      }

      std::ostringstream binance_ss;
      binance_ss << std::fixed << std::setprecision(1) << binance_ask_it->first
                 << "        " << std::setprecision(4) << quantity
                 << "     " << std::setprecision(4) << cumulative;
      binance_line = binance_ss.str();
      ++binance_ask_it;
    }

    // Gate.io ask
    if (gateio_ask_it != local_gateio_asks.rend())
    {
      double actual_size = std::stod(gateio_ask_it->second);
      if (global_gateio_client)
      {
        actual_size *= global_gateio_client->getQuantoMultiplier();
      }

      // Find the cumulative for this price level
      double cumulative = 0.0;
      for (const auto& cum : gateio_ask_cumulatives)
      {
        if (cum.first == gateio_ask_it->first)
        {
          cumulative = cum.second;
          break;
        }
      }

      std::ostringstream gateio_ss;
      gateio_ss << std::fixed << std::setprecision(1) << gateio_ask_it->first
                << "        " << std::setprecision(4) << actual_size
                << "    " << std::setprecision(4) << cumulative;
      gateio_line = gateio_ss.str();
      ++gateio_ask_it;
    }

    std::cout << std::left << std::setw(50) << binance_line
              << " | " << std::setw(50) << gateio_line << std::endl;
  }

  std::cout << std::endl;
  std::cout << std::left << std::setw(50) << "--- SPREAD ---"
            << " | " << std::setw(50) << "--- SPREAD ---" << std::endl;
  std::cout << std::endl;

  // Bids section
  std::cout << std::left << std::setw(50) << "BIDS (Buy Orders):"
            << " | " << std::setw(50) << "BIDS (Buy Orders):" << std::endl;
  std::cout << std::left << std::setw(50) << "Price        Quantity     Cumulative"
            << " | " << std::setw(50) << "Price        Size (USD)   Cumulative" << std::endl;
  std::cout << std::left << std::setw(50) << "-----        --------     ----------"
            << " | " << std::setw(50) << "-----        ---------    ----------" << std::endl;

  // Pre-calculate cumulative sums for bids (from highest bid price downward)
  std::vector<std::pair<double, double>> binance_bid_cumulatives;
  std::vector<std::pair<double, double>> gateio_bid_cumulatives;

  // Calculate Binance bid cumulatives (from highest price downward)
  double binance_bid_running_total = 0.0;
  for (const auto& bid : local_binance_bids)
  {
    double quantity = std::stod(bid.second);
    binance_bid_running_total += quantity;
    binance_bid_cumulatives.push_back({bid.first, binance_bid_running_total});
  }

  // Calculate Gate.io bid cumulatives (from highest price downward)
  double gateio_bid_running_total = 0.0;
  for (const auto& bid : local_gateio_bids)
  {
    double actual_size = std::stod(bid.second);
    if (global_gateio_client)
    {
      actual_size *= global_gateio_client->getQuantoMultiplier();
    }
    gateio_bid_running_total += actual_size;
    gateio_bid_cumulatives.push_back({bid.first, gateio_bid_running_total});
  }

  // Print top 5 bids side by side (highest price first, with correct cumulatives)
  auto binance_bid_it = local_binance_bids.begin();
  auto gateio_bid_it = local_gateio_bids.begin();

  for (int i = 0; i < 5; ++i)
  {
    std::string binance_line = "";
    std::string gateio_line = "";

    // Binance bid
    if (binance_bid_it != local_binance_bids.end())
    {
      double quantity = std::stod(binance_bid_it->second);

      // Find the cumulative for this price level
      double cumulative = 0.0;
      for (const auto& cum : binance_bid_cumulatives)
      {
        if (cum.first == binance_bid_it->first)
        {
          cumulative = cum.second;
          break;
        }
      }

      std::ostringstream binance_ss;
      binance_ss << std::fixed << std::setprecision(1) << binance_bid_it->first
                 << "        " << std::setprecision(4) << quantity
                 << "     " << std::setprecision(4) << cumulative;
      binance_line = binance_ss.str();
      ++binance_bid_it;
    }

    // Gate.io bid
    if (gateio_bid_it != local_gateio_bids.end())
    {
      double actual_size = std::stod(gateio_bid_it->second);
      if (global_gateio_client)
      {
        actual_size *= global_gateio_client->getQuantoMultiplier();
      }

      // Find the cumulative for this price level
      double cumulative = 0.0;
      for (const auto& cum : gateio_bid_cumulatives)
      {
        if (cum.first == gateio_bid_it->first)
        {
          cumulative = cum.second;
          break;
        }
      }

      std::ostringstream gateio_ss;
      gateio_ss << std::fixed << std::setprecision(1) << gateio_bid_it->first
                << "        " << std::setprecision(4) << actual_size
                << "    " << std::setprecision(4) << cumulative;
      gateio_line = gateio_ss.str();
      ++gateio_bid_it;
    }

    std::cout << std::left << std::setw(50) << binance_line
              << " | " << std::setw(50) << gateio_line << std::endl;
  }

  // Calculate and display spreads
  std::cout << std::endl;
  std::string binance_spread = "";
  std::string gateio_spread = "";

  // Binance spread
  if (!local_binance_bids.empty() && !local_binance_asks.empty())
  {
    try
    {
      double bestBid = local_binance_bids.begin()->first;
      double bestAsk = local_binance_asks.begin()->first;
      double spread = bestAsk - bestBid;
      double spreadBps = (spread / bestBid) * 10000;

      std::ostringstream ss;
      ss << "Spread: $" << std::fixed << std::setprecision(1) << spread
         << " (" << std::setprecision(2) << spreadBps << " bps)";
      binance_spread = ss.str();
    }
    catch (const std::exception& e)
    {
      binance_spread = "Could not calculate spread";
    }
  }

  // Gate.io spread
  if (!local_gateio_bids.empty() && !local_gateio_asks.empty())
  {
    try
    {
      double bestBid = local_gateio_bids.begin()->first;
      double bestAsk = local_gateio_asks.begin()->first;
      double spread = bestAsk - bestBid;
      double spreadBps = (spread / bestBid) * 10000;

      std::ostringstream ss;
      ss << "Spread: $" << std::fixed << std::setprecision(1) << spread
         << " (" << std::setprecision(2) << spreadBps << " bps)";
      gateio_spread = ss.str();
    }
    catch (const std::exception& e)
    {
      gateio_spread = "Could not calculate spread";
    }
  }

  std::cout << std::left << std::setw(50) << binance_spread
            << " | " << std::setw(50) << gateio_spread << std::endl;

  std::cout << std::string(103, '=') << std::endl;
}

// Binance callbacks
void onBinanceDepthUpdate(const DepthUpdate& update)
{
  // Record when we start processing
  auto processing_start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();

  {
    std::lock_guard<std::mutex> lock(binance_mutex_);

    // Clear existing order book (Binance sends snapshots, not incremental updates)
    binance_bids_.clear();
    binance_asks_.clear();

    // Update bids
    for (const auto& bid : update.bids)
    {
      double price = std::stod(bid.first);
      const std::string& quantity = bid.second;

      if (std::stod(quantity) > 0.0)
      {
        binance_bids_[price] = quantity;
      }
    }

    // Update asks
    for (const auto& ask : update.asks)
    {
      double price = std::stod(ask.first);
      const std::string& quantity = ask.second;

      if (std::stod(quantity) > 0.0)
      {
        binance_asks_[price] = quantity;
      }
    }
  }  // Release mutex before calling printDualOrderBook

  // Calculate and store latencies
  auto processing_end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

  // External latency: current time - timestamp sent by exchange
  double external_latency_ms = processing_end_time - update.eventTime;

  // Internal latency: time after books are updated - when we received the update
  double internal_latency_us = (processing_end_time - update.websocket_receive_timestamp) * 1000.0;

  {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    binance_latency_stats_.external_latency_ms = external_latency_ms;
    binance_latency_stats_.internal_latency_us = internal_latency_us;
    binance_latency_stats_.last_update_time = processing_end_time;
  }

  // Print updated dual order book
  printDualOrderBook();
}

// Gate.io callbacks
void onGateIOOrderBookUpdate(const GateOrderBookUpdate& update)
{
  // Record when we start processing
  auto processing_start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();

  {
    std::lock_guard<std::mutex> lock(gateio_mutex_);

    // Clear existing order book (Gate.io sends full snapshots)
    gateio_bids_.clear();
    gateio_asks_.clear();

    // Update bids
    for (const auto& bid : update.bids)
    {
      double price = std::stod(bid.first);
      const std::string& quantity = bid.second;

      if (std::stod(quantity) > 0.0)
      {
        gateio_bids_[price] = quantity;
      }
    }

    // Update asks
    for (const auto& ask : update.asks)
    {
      double price = std::stod(ask.first);
      const std::string& quantity = ask.second;

      if (std::stod(quantity) > 0.0)
      {
        gateio_asks_[price] = quantity;
      }
    }
  }  // Release mutex before calling printDualOrderBook

  // Calculate and store latencies
  auto processing_end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();

  // External latency: current time - timestamp sent by exchange
  double external_latency_ms = processing_end_time - update.timestamp;

  // Internal latency: time after books are updated - when we received the update
  double internal_latency_us = (processing_end_time - update.websocket_receive_timestamp) * 1000.0;

  {
    std::lock_guard<std::mutex> lock(latency_mutex_);
    gateio_latency_stats_.external_latency_ms = external_latency_ms;
    gateio_latency_stats_.internal_latency_us = internal_latency_us;
    gateio_latency_stats_.last_update_time = processing_end_time;
  }

  // Print updated dual order book
  printDualOrderBook();
}

void onBinanceConnection()
{
  std::cout << "Connected to Binance Futures WebSocket!" << std::endl;
}

void onGateIOConnection()
{
  std::cout << "Connected to Gate.io Futures WebSocket!" << std::endl;
}

void onBinanceError(const std::string& error)
{
  std::cerr << "Binance WebSocket error: " << error << std::endl;
}

void onGateIOError(const std::string& error)
{
  std::cerr << "Gate.io WebSocket error: " << error << std::endl;
}

int main()
{
  // Set up signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Clear any stale order book data from previous runs
  binance_bids_.clear();
  binance_asks_.clear();
  gateio_bids_.clear();
  gateio_asks_.clear();

  std::cout << "Dual Exchange Futures WebSocket Order Book Demo" << std::endl;
  std::cout << "Connecting to Binance BTCUSDT and Gate.io BTC_USDT order book streams..." << std::endl;
  std::cout << "Will display both order books side by side with 5 levels each" << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;
  std::cout << std::endl;

  // Create WebSocket clients
  BinanceWebSocketClient binance_client;
  GateIOWebSocketClient gateio_client;
  global_gateio_client = &gateio_client;  // Store reference for multiplier access

  // Set up Binance callbacks
  binance_client.setDepthUpdateCallback(onBinanceDepthUpdate);
  binance_client.setConnectionCallback(onBinanceConnection);
  binance_client.setErrorCallback(onBinanceError);

  // Set up Gate.io callbacks
  gateio_client.setOrderBookUpdateCallback(onGateIOOrderBookUpdate);
  gateio_client.setConnectionCallback(onGateIOConnection);
  gateio_client.setErrorCallback(onGateIOError);

  // Initialize clients
  if (!binance_client.initialize())
  {
    std::cerr << "Failed to initialize Binance WebSocket client" << std::endl;
    return 1;
  }

  if (!gateio_client.initialize())
  {
    std::cerr << "Failed to initialize Gate.io WebSocket client" << std::endl;
    return 1;
  }

  // Connect to exchanges
  if (!binance_client.connect("BTCUSDT"))
  {
    std::cerr << "Failed to connect to Binance WebSocket" << std::endl;
    return 1;
  }

  if (!gateio_client.connect("BTC_USDT"))
  {
    std::cerr << "Failed to connect to Gate.io WebSocket" << std::endl;
    return 1;
  }

  // Start WebSocket clients in separate threads
  std::thread binance_thread([&binance_client]()
                             { binance_client.run(); });

  std::thread gateio_thread([&gateio_client]()
                            { gateio_client.run(); });

  // Wait for connections
  std::this_thread::sleep_for(std::chrono::seconds(3));

  std::cout << "\nWaiting for dual WebSocket updates...\n"
            << std::endl;

  // Main loop - just wait for signals
  while (keep_running && binance_client.isConnected() && gateio_client.isConnected())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nStopping WebSocket clients..." << std::endl;
  binance_client.stop();
  gateio_client.stop();

  if (binance_thread.joinable())
  {
    binance_thread.join();
  }

  if (gateio_thread.joinable())
  {
    gateio_thread.join();
  }

  std::cout << "Dual exchange demo completed successfully!" << std::endl;
  return 0;
}
