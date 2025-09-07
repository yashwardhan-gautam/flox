/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "demo/local_order_book.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <chrono>

namespace demo
{

LocalOrderBook::LocalOrderBook(const std::string& symbol)
  : symbol_(symbol), last_update_id_(0), running_(false)
{
}

LocalOrderBook::~LocalOrderBook()
{
  stop();
}

bool LocalOrderBook::initialize()
{
  // Initialize WebSocket client  
  if (!ws_client_.initialize())
  {
    std::cerr << "Failed to initialize WebSocket client" << std::endl;
    return false;
  }
  
  // Set up WebSocket callbacks
  ws_client_.setDepthUpdateCallback([this](const DepthUpdate& update) {
    onDepthUpdate(update);
  });
  
  ws_client_.setConnectionCallback([this]() {
    onWebSocketConnected();
  });
  
  ws_client_.setErrorCallback([this](const std::string& error) {
    onWebSocketError(error);
  });
  
  return true;
}

void LocalOrderBook::start()
{
  if (running_)
  {
    return;
  }
  
  running_ = true;
  
  std::cout << "Starting local order book for " << symbol_ << " using partial depth stream" << std::endl;
  
  // Connect to WebSocket partial depth stream  
  if (!ws_client_.connect(symbol_))
  {
    std::cerr << "Failed to connect to WebSocket stream" << std::endl;
    running_ = false;
    return;
  }
  
  // Start WebSocket event loop in a separate thread
  std::thread ws_thread([this]() {
    ws_client_.run();
  });
  ws_thread.detach();
  
  std::cout << "Local order book started - will print updates as they arrive" << std::endl;
}

void LocalOrderBook::stop()
{
  running_ = false;
  ws_client_.stop();
}

LocalOrderBook::OrderBookState LocalOrderBook::getOrderBookState() const
{
  std::lock_guard<std::mutex> lock(book_mutex_);
  return {bids_, asks_, last_update_id_};
}

void LocalOrderBook::printTopLevels(int levels) const
{
  std::lock_guard<std::mutex> lock(book_mutex_);
  
  if (bids_.empty() && asks_.empty())
  {
    std::cout << "Order book is empty, waiting for updates..." << std::endl;
    return;
  }
  
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  
  std::cout << "\n=== Local Order Book - " << symbol_ << " (Partial Depth Stream) ===" << std::endl;
  std::cout << "Timestamp: " << std::ctime(&time_t);
  std::cout << "Last Update ID: " << last_update_id_ << std::endl;
  
  std::cout << "\nAsks (Sell Orders):" << std::endl;
  std::cout << std::setw(15) << "Price" << std::setw(15) << "Quantity" << std::endl;
  std::cout << std::setw(15) << "-----" << std::setw(15) << "--------" << std::endl;
  
  // Print asks in reverse order (highest price first)
  auto ask_it = asks_.rbegin();
  for (int i = 0; i < levels && ask_it != asks_.rend(); ++i, ++ask_it)
  {
    if (ask_it->second > 0)  // Only show non-zero quantities
    {
      std::cout << std::setw(15) << std::fixed << std::setprecision(2) << ask_it->first
                << std::setw(15) << std::fixed << std::setprecision(3) << ask_it->second << std::endl;
    }
  }
  
  std::cout << "\n--- SPREAD ---" << std::endl;
  
  std::cout << "\nBids (Buy Orders):" << std::endl;
  std::cout << std::setw(15) << "Price" << std::setw(15) << "Quantity" << std::endl;
  std::cout << std::setw(15) << "-----" << std::setw(15) << "--------" << std::endl;
  
  // Print bids in descending order (highest price first)
  auto bid_it = bids_.rbegin();
  for (int i = 0; i < levels && bid_it != bids_.rend(); ++i, ++bid_it)
  {
    if (bid_it->second > 0)  // Only show non-zero quantities
    {
      std::cout << std::setw(15) << std::fixed << std::setprecision(2) << bid_it->first
                << std::setw(15) << std::fixed << std::setprecision(3) << bid_it->second << std::endl;
    }
  }
  
  // Calculate spread
  if (!bids_.empty() && !asks_.empty())
  {
    double bestBid = bids_.rbegin()->first;
    double bestAsk = asks_.begin()->first;
    double spread = bestAsk - bestBid;
    double spreadBps = (spread / bestBid) * 10000;
    
    std::cout << "\nSpread: $" << std::fixed << std::setprecision(2) << spread 
              << " (" << std::fixed << std::setprecision(2) << spreadBps << " bps)" << std::endl;
  }
  
  std::cout << "==========================================" << std::endl;
}

void LocalOrderBook::onDepthUpdate(const DepthUpdate& update)
{
  if (!running_)
  {
    return;
  }
  
  // With partial depth stream, each update contains the complete top 10 levels
  // No need for complex synchronization - just process directly
  processDepthUpdate(update);
  
  // Print the order book immediately after each update
  printTopLevels(10);
}

void LocalOrderBook::onWebSocketConnected()
{
  std::cout << "WebSocket connected to partial depth stream - ready to receive updates!" << std::endl;
}

void LocalOrderBook::onWebSocketError(const std::string& error)
{
  std::cerr << "WebSocket error: " << error << std::endl;
}

void LocalOrderBook::processDepthUpdate(const DepthUpdate& update)
{
  std::lock_guard<std::mutex> lock(book_mutex_);
  
  // Clear existing data - partial depth stream provides complete snapshot of top N levels
  bids_.clear();
  asks_.clear();
  
  // Load bid data (partial depth stream contains absolute quantities for top 10 levels)
  for (const auto& bid : update.bids)
  {
    double price = std::stod(bid.first);
    double quantity = std::stod(bid.second);
    if (quantity > 0)  // Only add non-zero quantities
    {
      bids_[price] = quantity;
    }
  }
  
  // Load ask data  
  for (const auto& ask : update.asks)
  {
    double price = std::stod(ask.first);
    double quantity = std::stod(ask.second);
    if (quantity > 0)  // Only add non-zero quantities
    {
      asks_[price] = quantity;
    }
  }
  
  last_update_id_ = update.finalUpdateId;
}

}  // namespace demo
