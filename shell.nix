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
    numactl
    
    # Network libraries
    openssl
    zlib
    curl
    libwebsockets
    
    # JSON parsing
    nlohmann_json

    # Development tools
    clang-tools
    gdb
    valgrind
    strace
    ltrace
    
    # Modern debugging tools
    lldb
    rr
    
    # Editor and DAP support
    neovim
    
    # Optional profiling tools
    linuxPackages.perf
    heaptrack

    # Shell utilities
    findutils
    coreutils
    bash
  ];

  # Environment variables
  CMAKE_BUILD_TYPE = "Release";
  NIX_ENFORCE_NO_NATIVE = "0";  # Allow -march=native for local development

  shellHook = ''
    echo "🚀 FLOX Development Environment"
    echo "==============================="
    echo "CMake:       $(cmake --version | head -n1)"
    echo "GCC:         $(gcc --version | head -n1)"
    echo "GDB:         $(gdb --version | head -n1)"
    echo "GTest:       Available"
    echo "Benchmark:   Available"
    echo "OpenSSL:     $(openssl version)"
    echo "cURL:        $(curl --version | head -n1)"
    echo "WebSockets:  libwebsockets available"
    echo "JSON:        nlohmann_json available"
    echo ""
    echo "Build commands:"
    echo "  mkdir build && cd build"
    echo "  cmake .. -DFLOX_ENABLE_TESTS=ON -DFLOX_ENABLE_BENCHMARKS=ON -DFLOX_ENABLE_DEMO=ON"
    echo "  make -j$(nproc)"
    echo ""
    echo "Debug build:"
    echo "  cmake .. -DCMAKE_BUILD_TYPE=Debug -DFLOX_ENABLE_TESTS=ON -DFLOX_ENABLE_DEMO=ON"
    echo "  make -j$(nproc)"
    echo ""
    echo "Debugging tools available:"
    echo "  gdb ./demo/flox_demo              # Debug with GDB"
    echo "  lldb ./demo/flox_demo             # Debug with LLDB"
    echo "  valgrind ./tests/test_decimal     # Memory debugging"
    echo "  rr record ./tests/test_decimal    # Record & replay debugging"
    echo "  strace ./demo/flox_demo           # System call tracing"
    echo "  heaptrack ./demo/flox_demo        # Heap profiling"
    echo ""
    echo "Nvim DAP Setup for GDB:"
    echo "  Add to your nvim config:"
    echo "  require('dap').adapters.cppdbg = {"
    echo "    id = 'cppdbg',"
    echo "    type = 'executable',"
    echo "    command = '$(which gdb)',"
    echo "    args = {'--interpreter=dap'}"
    echo "  }"
    echo "  require('dap').configurations.cpp = {{"
    echo "    name = 'Launch flox_demo',"
    echo "    type = 'cppdbg',"
    echo "    request = 'launch',"
    echo "    program = '\$\{workspaceFolder\}/build/demo/flox_demo',"
    echo "    cwd = '\$\{workspaceFolder\}',"
    echo "    stopAtEntry = false"
    echo "  }}"
    echo ""
    echo "Optional features:"
    echo "  -DFLOX_ENABLE_CPU_AFFINITY=ON  (requires dedicated hardware)"
    echo "  -DFLOX_ENABLE_TRACY=ON         (Tracy profiler)"
    echo ""
    echo "Scripts:"
    echo "  ./scripts/check-format.sh        # Format check"
    echo "  ./scripts/run-benchmarks.sh build # Run benchmarks"
    echo ""
    
    # Setup GDB for DAP debugging
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
