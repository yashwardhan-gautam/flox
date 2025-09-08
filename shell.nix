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
    curl
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
    echo "🚀 FLOX Binance Futures WebSocket Demo Environment"
    echo "=================================================="
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
    echo "Run WebSocket demo:"
    echo "  ./binance_futures_demo"
    echo ""
    echo "This demo will:"
    echo "  - Connect to Binance Futures WebSocket"
    echo "  - Subscribe to BTCUSDT depth stream"
    echo "  - Log every WebSocket message"
    echo "  - Display real-time order book updates"
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
