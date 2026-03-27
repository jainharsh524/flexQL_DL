#!/bin/bash
# scripts/run_master.sh
# "One-click" FlexQL Master Benchmark Test

# 1. Configuration
PORT=9000
DATA_DIR="./data"
BIN_DIR="./bin"
ROWS=${1:-10000000}    # 10M default
QUERIES=${2:-10000000} # 10M default

# 2. Cleanup
echo "--- Cleanup ---"
pkill -9 -f flexql_server 2>/dev/null
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"
sleep 1

# 3. Start Server
echo "--- Starting FlexQL Server (Port $PORT) ---"
"$BIN_DIR/flexql_server" --port "$PORT" --data-dir "$DATA_DIR" > server.log 2>&1 &
SERVER_PID=$!
sleep 2

# Check if server is alive
if ! ps -p $SERVER_PID > /dev/null; then
  echo "Error: Server failed to start. Check server.log"
  exit 1
fi

# 4. Run Benchmark
echo "--- Starting Master Benchmark ($ROWS rows, $QUERIES queries) ---"
"$BIN_DIR/flexql_master_benchmark" "$ROWS" "$QUERIES"

# 5. Shutdown
echo "--- Shutting down server ---"
kill $SERVER_PID 2>/dev/null
pkill -9 -f flexql_server 2>/dev/null

echo "Done."
