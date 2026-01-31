# OpenClaw C++11 Makefile
# Minimal build system for the modular AI assistant framework

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I./include
CXXFLAGS_PIC = $(CXXFLAGS) -fPIC -fvisibility=hidden
LDFLAGS = -lpthread -lsqlite3 -lssl -lcrypto -lcurl -ldl
LDFLAGS_PLUGIN = -shared

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
PLUGIN_DIR = $(BIN_DIR)/plugins

# Core source files (always built into main binary)
CORE_SOURCES = $(SRC_DIR)/core/types.cpp \
               $(SRC_DIR)/core/logger.cpp \
               $(SRC_DIR)/core/json.cpp \
               $(SRC_DIR)/core/config.cpp \
               $(SRC_DIR)/core/http_client.cpp \
               $(SRC_DIR)/core/utils.cpp \
               $(SRC_DIR)/plugin/loader.cpp \
               $(SRC_DIR)/ai/ai.cpp \
               $(SRC_DIR)/memory/store.cpp \
               $(SRC_DIR)/memory/manager.cpp \
               $(SRC_DIR)/session/session.cpp \
               $(SRC_DIR)/polls/polls.cpp \
               $(SRC_DIR)/rate_limiter/rate_limiter.cpp

# Core object files
CORE_OBJECTS = $(BUILD_DIR)/types.o \
               $(BUILD_DIR)/logger.o \
               $(BUILD_DIR)/json.o \
               $(BUILD_DIR)/config.o \
               $(BUILD_DIR)/http_client.o \
               $(BUILD_DIR)/utils.o \
               $(BUILD_DIR)/loader.o \
               $(BUILD_DIR)/ai.o \
               $(BUILD_DIR)/memory_store.o \
               $(BUILD_DIR)/memory_manager.o \
               $(BUILD_DIR)/session.o \
               $(BUILD_DIR)/polls.o \
               $(BUILD_DIR)/rate_limiter.o

# Main binary objects
MAIN_OBJECTS = $(BUILD_DIR)/main.o $(CORE_OBJECTS)

# Plugin shared libraries
PLUGINS = $(PLUGIN_DIR)/telegram.so \
          $(PLUGIN_DIR)/whatsapp.so \
          $(PLUGIN_DIR)/browser.so \
          $(PLUGIN_DIR)/claude.so \
          $(PLUGIN_DIR)/memory.so

TARGET = $(BIN_DIR)/openclaw

# Default target - build main binary and all plugins
all: dirs $(TARGET) plugins

# Build only plugins
plugins: dirs $(PLUGINS)

# Create directories
dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR) $(PLUGIN_DIR)

# Link main binary
$(TARGET): $(MAIN_OBJECTS)
	$(CXX) $(MAIN_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

# ============ Core object files ============
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/types.o: $(SRC_DIR)/core/types.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/logger.o: $(SRC_DIR)/core/logger.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/json.o: $(SRC_DIR)/core/json.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/config.o: $(SRC_DIR)/core/config.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/http_client.o: $(SRC_DIR)/core/http_client.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/loader.o: $(SRC_DIR)/plugin/loader.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/ai.o: $(SRC_DIR)/ai/ai.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/memory_store.o: $(SRC_DIR)/memory/store.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/memory_manager.o: $(SRC_DIR)/memory/manager.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/utils.o: $(SRC_DIR)/core/utils.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/session.o: $(SRC_DIR)/session/session.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/polls.o: $(SRC_DIR)/polls/polls.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(BUILD_DIR)/rate_limiter.o: $(SRC_DIR)/rate_limiter/rate_limiter.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

# ============ Plugin shared libraries ============
# Telegram channel plugin
$(BUILD_DIR)/telegram_plugin.o: $(SRC_DIR)/channels/telegram/telegram.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(PLUGIN_DIR)/telegram.so: $(BUILD_DIR)/telegram_plugin.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS_PLUGIN) $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# WhatsApp channel plugin
$(BUILD_DIR)/whatsapp_plugin.o: $(SRC_DIR)/channels/whatsapp/whatsapp.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(PLUGIN_DIR)/whatsapp.so: $(BUILD_DIR)/whatsapp_plugin.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS_PLUGIN) $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Browser tool plugin
$(BUILD_DIR)/browser_plugin.o: $(SRC_DIR)/tools/browser/browser.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(PLUGIN_DIR)/browser.so: $(BUILD_DIR)/browser_plugin.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS_PLUGIN) $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Claude AI plugin
$(BUILD_DIR)/claude_plugin.o: $(SRC_DIR)/ai/claude.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(PLUGIN_DIR)/claude.so: $(BUILD_DIR)/claude_plugin.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS_PLUGIN) $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Memory tool plugin
$(BUILD_DIR)/memory_plugin.o: $(SRC_DIR)/tools/memory/memory.cpp
	$(CXX) $(CXXFLAGS_PIC) -c $< -o $@

$(PLUGIN_DIR)/memory.so: $(BUILD_DIR)/memory_plugin.o $(CORE_OBJECTS)
	$(CXX) $(LDFLAGS_PLUGIN) $^ -o $@ $(LDFLAGS)
	@echo "Built plugin: $@"

# Debug build
debug: CXXFLAGS += -g -DDEBUG -O0
debug: CXXFLAGS_PIC += -g -DDEBUG -O0
debug: clean all

# Release build (with optimizations and stripping)
release: CXXFLAGS += -O3 -DNDEBUG
release: CXXFLAGS_PIC += -O3 -DNDEBUG
release: clean all
	strip $(TARGET)
	strip $(PLUGINS)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Install to /usr/local
install: all
	install -d /usr/local/bin
	install -d /usr/local/lib/openclaw/plugins
	install -m 755 $(TARGET) /usr/local/bin/openclaw
	install -m 755 $(PLUGINS) /usr/local/lib/openclaw/plugins/

# Uninstall
uninstall:
	rm -f /usr/local/bin/openclaw
	rm -rf /usr/local/lib/openclaw

# Run with example
run: all
	@echo "Starting OpenClaw..."
	@echo "Make sure TELEGRAM_BOT_TOKEN is set"
	OPENCLAW_LOG_LEVEL=debug OPENCLAW_PLUGIN_PATH=./bin/plugins $(TARGET)

# Help
help:
	@echo "OpenClaw C++11 Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build main binary and all plugins (default)"
	@echo "  plugins  - Build only plugin shared libraries"
	@echo "  debug    - Build with debug symbols"
	@echo "  release  - Build optimized release"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install to /usr/local"
	@echo "  run      - Build and run"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Plugins (in bin/plugins/):"
	@echo "  telegram.so  - Telegram Bot API channel"
	@echo "  whatsapp.so  - WhatsApp messaging channel"
	@echo "  browser.so   - HTTP browser tool"
	@echo "  claude.so    - Anthropic Claude AI"
	@echo "  memory.so    - Memory/task management"
	@echo ""
	@echo "Requirements:"
	@echo "  - g++ with C++11 support"
	@echo "  - libcurl development headers"
	@echo "  - libsqlite3 development headers"
	@echo "  - libssl development headers"
	@echo ""
	@echo "Environment:"
	@echo "  OPENCLAW_PLUGIN_PATH - Plugin search paths (colon-separated)"
	@echo "  TELEGRAM_BOT_TOKEN   - Telegram Bot API token"
	@echo "  ANTHROPIC_API_KEY    - Claude API key"
	@echo "  OPENCLAW_LOG_LEVEL   - debug, info, warn, error"

.PHONY: all dirs plugins clean debug release install uninstall run help
