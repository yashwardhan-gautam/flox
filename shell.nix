{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    cmake
    gcc14
    pkg-config
    git

    # C++ libraries  
    gtest
    gbenchmark
    
    # Network libraries
    openssl
    zlib
    curl.dev  # Include development headers
    curl      # Runtime library
    libwebsockets
    
    # JSON parsing
    nlohmann_json
    jsoncpp

    # Development tools
    clang-tools
    gdb
    lldb
    
    # Shell utilities
    findutils
    coreutils
    bash
    fish
  ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
    # Linux-specific packages
    numactl
    valgrind
    strace
    ltrace
    rr
    linuxPackages.perf
    heaptrack
  ];

  # Environment variables
  CMAKE_BUILD_TYPE = "Release";
  NIX_ENFORCE_NO_NATIVE = "0";  # Allow -march=native for local development

  shellHook = ''
    echo "FLOX Futures WebSocket Demo Environment"
    echo "==========================================="
    echo "CMake:       $(cmake --version | head -n1)"
    echo "GCC:         $(gcc --version | head -n1)"
    echo "GDB:         $(gdb --version | head -n1)"
    echo "OpenSSL:     $(openssl version)"
    echo "cURL:        $(curl --version | head -n1)"
    echo "WebSockets:  libwebsockets available"
    echo "JSON:        nlohmann_json available"
    echo ""
    echo "Build commands:"
    echo "  mkdir build && cd build"
    echo "  cmake .."
    echo "  make -j$(if command -v nproc >/dev/null 2>&1; then nproc; else sysctl -n hw.ncpu 2>/dev/null || echo 4; fi)"
    echo ""
    echo "Run WebSocket demos:"
    echo "  ./binance_futures_demo   # Binance Futures BTCUSDT (5 levels)"
    echo "  ./gateio_futures_demo    # Gate.io Futures BTC_USDT (5 levels)"
    echo "  ./dual_orderbook_demo    # Dual Exchange Order Book (side-by-side)"
    echo ""
    echo "Binance demo features:"
    echo "  - Connect to Binance Futures WebSocket"
    echo "  - Subscribe to BTCUSDT depth stream (5 levels)"
    echo "  - Log every WebSocket message"
    echo "  - Display real-time order book updates"
    echo ""
    echo "Gate.io demo features:"
    echo "  - Connect to Gate.io Futures WebSocket"
    echo "  - Subscribe to BTC_USDT order book (5 levels)"
    echo "  - Log every WebSocket message"
    echo "  - Display real-time order book snapshots"
    echo ""
    echo "Dual Order Book demo features:"
    echo "  - Connect to both Binance and Gate.io simultaneously"
    echo "  - Display order books side-by-side in real-time"
    echo "  - Compare spreads and liquidity across exchanges"
    echo "  - Multi-threaded WebSocket connections"
    echo ""
    
    # Setup GDB for debugging
    export GDB_PATH="$(which gdb)"
    
    # Create a simple gdbinit for better debugging
    if [ ! -f "$HOME/.gdbinit" ]; then
      echo "set auto-load safe-path /" > "$HOME/.gdbinit"
      echo "set print pretty on" >> "$HOME/.gdbinit"
      echo "set print object on" >> "$HOME/.gdbinit"
      echo "set print static-members on" >> "$HOME/.gdbinit"
      echo "set print vtbl on" >> "$HOME/.gdbinit"
      echo "set print demangle on" >> "$HOME/.gdbinit"
      echo "set demangle-style gnu-v3" >> "$HOME/.gdbinit"
      echo "Created ~/.gdbinit for better debugging experience"
    fi
  '';
}
