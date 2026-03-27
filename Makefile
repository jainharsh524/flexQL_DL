CXXFLAGS := -std=c++17 -O3 -march=native -Wall -Wextra -Iinclude -DNDEBUG -flto -funroll-loops
LDFLAGS  := -flto

SRC_DIRS := src/storage src/index src/cache src/parser \
            src/executor src/optimizer src/server src/client
BUILD    := build
BIN      := bin

# ── Collect all non-main sources ──────────────────────────────────────────────
STORAGE_SRCS := src/storage/pager.cpp src/storage/table.cpp \
                src/storage/column_store.cpp src/storage/column_reader.cpp \
                src/storage/batch_buffer.cpp src/storage/column_ops.cpp \
                src/storage/serializer.cpp
INDEX_SRCS    := src/index/bptree.cpp src/index/index_manager.cpp
CACHE_SRCS    := src/cache/cache_manager.cpp src/cache/lru_cache.cpp
PARSER_SRCS   := src/parser/parser.cpp
EXEC_SRCS     := src/executor/executor.cpp src/executor/parallel_scan.cpp \
                 src/executor/hash_join.cpp src/executor/scan.cpp \
                 src/executor/join.cpp
OPT_SRCS      := src/optimizer/optimizer.cpp
SERVER_SRCS   := src/server/server.cpp src/server/thread_pool.cpp src/server/main_server.cpp
CLIENT_SRCS   := src/client/flexql.cpp src/client/main_client.cpp

CORE_SRCS := $(STORAGE_SRCS) $(INDEX_SRCS) $(CACHE_SRCS) \
             $(PARSER_SRCS) $(EXEC_SRCS) $(OPT_SRCS)

SERVER_ALL := $(CORE_SRCS) $(SERVER_SRCS)
CLIENT_ALL := $(CLIENT_SRCS)

# ── Object helpers ─────────────────────────────────────────────────────────────
to_obj = $(patsubst src/%.cpp,$(BUILD)/%.o,$(1))

CORE_OBJS   := $(call to_obj,$(CORE_SRCS))
SERVER_OBJS := $(call to_obj,$(SERVER_ALL))
CLIENT_OBJS := $(call to_obj,$(CLIENT_ALL))

# ── Primary targets ────────────────────────────────────────────────────────────
.PHONY: all server client tests clean

all: server client

server: $(BIN)/flexql_server

client: $(BIN)/flexql_client

$(BIN)/flexql_server: $(SERVER_OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) -lpthread

$(BIN)/flexql_client: $(CLIENT_OBJS) $(CORE_OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) -lpthread

# ── Test targets ───────────────────────────────────────────────────────────────
TEST_NAMES := test_storage test_btree test_parser test_executor benchmark

tests: $(patsubst %,$(BIN)/%,$(TEST_NAMES))

define TEST_RULE
$(BIN)/$(1): $(BUILD)/tests/$(1).o $(SERVER_OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) $$^ -o $$@ $(LDFLAGS) -lpthread
endef
$(foreach t,$(TEST_NAMES),$(eval $(call TEST_RULE,$(t))))

# ── Generic compilation rule ───────────────────────────────────────────────────
$(BUILD)/%.o: src/%.cpp | makedirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/tests/%.o: tests/%.cpp | makedirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── Directory creation ─────────────────────────────────────────────────────────
$(BIN):
	mkdir -p $(BIN)

makedirs:
	@mkdir -p $(BIN) \
	  $(BUILD)/storage $(BUILD)/index $(BUILD)/cache \
	  $(BUILD)/parser $(BUILD)/executor $(BUILD)/optimizer \
	  $(BUILD)/server $(BUILD)/client $(BUILD)/tests $(BUILD)/benchmarks

clean:
	rm -rf $(BUILD) $(BIN) data/

# ── Benchmark targets ──────────────────────────────────────────────────────────
# Benchmarks use the client TCP API (flexql.h) and link client+core objs.
# Include path also adds include/client so #include "flexql.h" works.
BENCH_CXXFLAGS := $(CXXFLAGS) -Iinclude/client

BENCH_NAMES := master_benchmark benchmark_flexql

benchmarks: $(patsubst %,$(BIN)/flexql_%,$(BENCH_NAMES))

$(BUILD)/benchmarks/%.o: benchmarks/%.cpp | makedirs
	$(CXX) $(BENCH_CXXFLAGS) -c $< -o $@

# Each benchmark binary links: its own .o + client library + core (storage/index/cache/parser/executor/optimizer)
BENCH_LINK_OBJS := build/client/flexql.o $(CORE_OBJS)

define BENCH_RULE
$(BIN)/flexql_$(1): $(BUILD)/benchmarks/$(1).o $(BENCH_LINK_OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) $$^ -o $$@ $(LDFLAGS) -lpthread
endef
$(foreach b,$(BENCH_NAMES),$(eval $(call BENCH_RULE,$(b))))
