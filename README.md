# OpenClaw C++11

A minimal, modular AI assistant framework written in C++11, inspired by [OpenClaw](https://github.com/openclaw/openclaw).

## Features

- **Dynamic Plugin System**: Load plugins at runtime from shared libraries
- **Multiple Channels**: Telegram, WhatsApp support
- **AI Integration**: Claude AI provider with conversation history
- **Session Management**: Per-user sessions with conversation history
- **Memory System**: SQLite-backed memory with full-text search
- **Rate Limiting**: Token bucket and sliding window rate limiters
- **Polls**: Create and manage interactive polls
- **Tools**: Browser tool for web fetching, memory/task management
- **Utilities**: Phone number normalization, path handling, string helpers
- **Minimal Core**: Small binary with optional plugin loading

## Architecture

```
┌─────────────────────────────────────────────────┐
│                   Main Loop                      │
│    (poll channels, handle messages, dispatch)    │
└─────────────────────┬───────────────────────────┘
                      │
┌─────────────────────┴───────────────────────────┐
│          Plugin Loader + Registry                │
│      (loads .so files, manages plugins)          │
└──────┬──────────────┬──────────────────┬────────┘
       │              │                  │
┌──────┴──────┐ ┌─────┴─────┐    ┌──────┴──────┐
│  telegram   │ │  claude   │    │   memory    │
│    .so      │ │   .so     │    │    .so      │
└─────────────┘ └───────────┘    └─────────────┘
```

## Building

### Requirements

- C++11 compatible compiler (g++, clang++)
- libcurl development headers
- libsqlite3 development headers
- libssl development headers

### Fedora/RHEL

```bash
sudo dnf install gcc-c++ libcurl-devel sqlite-devel openssl-devel
```

### Ubuntu/Debian

```bash
sudo apt-get install build-essential libcurl4-openssl-dev libsqlite3-dev libssl-dev
```

### Build

```bash
make          # Build binary and all plugins
make plugins  # Build only plugins
make clean    # Clean build artifacts
```

### Output

```
bin/
├── openclaw              # Main binary
└── plugins/
    ├── telegram.so       # Telegram channel
    ├── whatsapp.so       # WhatsApp channel
    ├── claude.so         # Claude AI provider
    ├── browser.so        # Web browser tool
    └── memory.so         # Memory/task tool
```

## Usage

### Quick Start

```bash
# Set environment variables
export TELEGRAM_BOT_TOKEN="your-bot-token-here"
export ANTHROPIC_API_KEY="sk-ant-..."

# Run with default plugins
./bin/openclaw
```

### Configuration File

Create a `config.json`:

```json
{
    "plugins": [
        "telegram",
        "claude",
        "browser",
        "memory"
    ],
    "plugins_dir": "./bin/plugins",
    "workspace_dir": ".",
    "telegram": {
        "poll_timeout": 30
    }
}
```

Run with config:

```bash
./bin/openclaw config.json
```

### Environment Variables

| Variable | Description |
|----------|-------------|
| `OPENCLAW_PLUGIN_PATH` | Plugin search paths (colon-separated) |
| `TELEGRAM_BOT_TOKEN` | Telegram Bot API token |
| `ANTHROPIC_API_KEY` | Claude API key |
| `OPENCLAW_LOG_LEVEL` | Log level: debug, info, warn, error |

## Bot Commands

- `/start` - Welcome message
- `/help` - Show available commands
- `/ping` - Check if bot is alive
- `/info` - Show bot information
- `/new` - Start new conversation
- `/fetch <url>` - Fetch web page content
- `/links <url>` - Extract links from page

Or just send a message to chat with Claude AI!

## Plugin System

### Plugin Search Paths

1. `OPENCLAW_PLUGIN_PATH` environment variable
2. `./plugins`
3. `/usr/lib/openclaw/plugins`
4. `/usr/local/lib/openclaw/plugins`

### Creating a Plugin

1. Include the plugin header:

```cpp
#include <openclaw/plugin/loader.hpp>
#include <openclaw/plugin/channel.hpp>  // or tool.hpp, ai/ai.hpp

class MyPlugin : public openclaw::ChannelPlugin {
    // Implement required methods...
};

// Export plugin
OPENCLAW_DECLARE_PLUGIN(MyPlugin, "myplugin", "1.0.0", 
                        "My custom plugin", "channel")
```

2. Build as shared library:

```bash
g++ -std=c++11 -fPIC -shared myplugin.cpp -o myplugin.so
```

3. Place in plugin directory or add to config.

### Plugin Types

- `channel` - Communication channels (telegram, whatsapp)
- `tool` - Tools for AI function calling (browser, memory)
- `ai` - AI providers (claude)

## Project Structure

```
openclaw-cpp/
├── include/openclaw/
│   ├── core/           # Core utilities
│   │   ├── types.hpp
│   │   ├── logger.hpp
│   │   ├── json.hpp
│   │   ├── config.hpp
│   │   ├── http_client.hpp
│   │   └── utils.hpp       # String, path, phone utilities
│   ├── plugin/         # Plugin system
│   │   ├── plugin.hpp
│   │   ├── loader.hpp
│   │   ├── channel.hpp
│   │   ├── tool.hpp
│   │   └── registry.hpp
│   ├── session/        # Session management
│   │   └── session.hpp
│   ├── polls/          # Poll system
│   │   └── polls.hpp
│   ├── rate_limiter/   # Rate limiting
│   │   └── rate_limiter.hpp
│   ├── channels/       # Channel plugins
│   ├── tools/          # Tool plugins
│   ├── ai/             # AI plugins
│   └── memory/         # Memory system
├── src/
│   ├── main.cpp        # Application entry point
│   ├── core/
│   ├── plugin/
│   ├── session/
│   ├── polls/
│   ├── rate_limiter/
│   ├── channels/
│   ├── tools/
│   ├── ai/
│   └── memory/
├── Makefile
├── config.example.json
└── README.md
```

## License

MIT License

## Acknowledgments

Inspired by [OpenClaw](https://github.com/openclaw/openclaw) - a TypeScript-based personal AI assistant.
