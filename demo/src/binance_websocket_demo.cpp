/*
 * Flox Engine
 * Developed by FLOX Foundation (https://github.com/FLOX-Foundation)
 *
 * Copyright (c) 2025 FLOX Foundation
 * Licensed under the MIT License. See LICENSE file in the project root for full
 * license information.
 */

#include "demo/local_order_book.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>

using namespace demo;

std::atomic<bool> keep_running(true);

void signalHandler(int signal)
{
  std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
  keep_running = false;
}

int main()
{
  // Set up signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);
  
  std::cout << "🚀 Binance Futures WebSocket Partial Depth Order Book Demo" << std::endl;
  std::cout << "Maintaining local BTCUSDT order book using partial depth stream" << std::endl;
  std::cout << "Will print top 10 levels on every WebSocket update..." << std::endl;
  std::cout << "Press Ctrl+C to exit" << std::endl;

  LocalOrderBook order_book("BTCUSDT");
  
  if (!order_book.initialize())
  {
    std::cerr << "Failed to initialize local order book" << std::endl;
    return 1;
  }

  order_book.start();
  
  // Wait for initial connection
  std::this_thread::sleep_for(std::chrono::seconds(2));
  
  std::cout << "\nWaiting for WebSocket updates... Order book will be printed on every update.\n" << std::endl;
  
  // Simple wait loop - order book prints itself on every WebSocket update
  while (keep_running)
  {
    // Sleep to avoid busy waiting - the order book handles all printing automatically
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  std::cout << "\nStopping order book..." << std::endl;
  order_book.stop();
  
  std::cout << "Demo completed successfully!" << std::endl;
  return 0;
}
