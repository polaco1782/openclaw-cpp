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
- **Tools**: Built-in browser tool for web fetching, memory/task management
- **Utilities**: Phone number normalization, path handling, string helpers
- **Minimal Core**: Small binary with optional plugin loading

## Architecture

```
┌─────────────────────────────────────────────────┐
│              Main Orchestrator                   │
│   (load plugins, route messages, main loop)      │
└─────────────────────┬───────────────────────────┘
                      │
┌─────────────────────┴───────────────────────────┐
│          Plugin Loader + Registry                │
│   (loads .so files, registers commands/tools)    │
┌──────┬──────────┬──────────┬────────┘
    │          │          │
┌──────┴────┐ ┌───┴───┐ ┌────┴────┐
│ claude.so │ │telegram│ │ core  │
│   AI      │ │channel │ │ tools │
└───────────┘ └────────┘ └───────┘
```

**Plugin Types:**
- **channel** - Communication channels (telegram, whatsapp)
- **ai** - AI providers (claude) - handles chat messages
- **tool** - Tools for commands and AI function calling (memory)
- **core tools** - Built-in tools (browser)

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
├── openclaw              # Main binary (orchestrator)
└── plugins/
    ├── telegram.so       # Telegram channel
    ├── whatsapp.so       # WhatsApp channel
    ├── claude.so         # Claude AI provider
    └── memory.so         # Memory/task tool
```

## Usage

### Quick Start

Create a `config.json` with your credentials:

```json
{
    "plugins": ["telegram", "claude", "memory"],
    "plugins_dir": "./bin/plugins",
    "log_level": "info",
    "telegram": {
        "bot_token": "your-bot-token-here"
    },
    "claude": {
        "api_key": "sk-ant-..."
    }
}
```

Run:

```bash
./bin/openclaw config.json
```

### Configuration File

All configuration is stored in `config.json`:

```json
{
    "plugins": [
        "telegram",
        "claude",
        "memory"
    ],
    "plugins_dir": "./bin/plugins",
    "workspace_dir": ".",
    "log_level": "info",
    "system_prompt": "You are a helpful assistant.",
    
    "telegram": {
        "bot_token": "123456:ABC...",
        "poll_timeout": 30
    },
    
    "whatsapp": {
        "phone_number_id": "...",
        "access_token": "...",
        "bridge_url": "http://localhost:8080"
    },
    
    "claude": {
        "api_key": "sk-ant-...",
        "model": "claude-sonnet-4-20250514"
    },
    
    "memory": {
        "chunk_tokens": 400,
        "chunk_overlap": 80,
        "max_results": 10
    }
}
```

### Configuration Options

| Option | Description |
|--------|-------------|
| `plugins` | List of plugins to load |
| `plugins_dir` | Directory to search for plugins |
| `log_level` | Log level: debug, info, warn, error |
| `system_prompt` | Custom system prompt for AI |
| `telegram.bot_token` | Telegram Bot API token |
| `claude.api_key` | Claude API key |
| `claude.model` | Claude model to use (optional) |

## Bot Commands

- `/start` - Welcome message
- `/help` - Show available commands
- `/skills` - List available skills
- `/ping` - Check if bot is alive
- `/info` - Show bot information
- `/new` - Start new conversation
- `/status` - Show session status
- `/tools` - List available tools
- `/fetch <url>` - Fetch web page content
- `/links <url>` - Extract links from page

Or just send a message to chat with Claude AI!

## Plugin System

Core registers built-in commands at startup. Plugins can register additional commands during initialization. The main orchestrator routes messages to registered command handlers or the AI plugin.

### Plugin Search Paths

1. `plugins_dir` from config.json
2. `./plugins`
3. `/usr/lib/openclaw/plugins`
4. `/usr/local/lib/openclaw/plugins`

### Creating a Plugin

1. Include the plugin header:

```cpp
#include <openclaw/core/loader.hpp>
#include <openclaw/core/channel.hpp>  // or tool.hpp, ai/ai.hpp

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
- `tool` - Tools for commands and AI function calling (memory)
- `core tools` - Built-in tools compiled into the main binary (browser)
- `ai` - AI providers (claude) - handles chat messages via `handle_message()`

### Registering Commands

Plugins can register commands in their `init()` method:

```cpp
bool MyPlugin::init(const Config& cfg) {
    std::vector<CommandDef> cmds;
    cmds.push_back(CommandDef("/mycommand", "Description", my_handler));
    PluginRegistry::instance().register_commands(cmds);
    return true;
}
```

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

```bash
cp config.example.json config.json
# Edit config.json with your API keys and tokens
./bin/openclaw config.json
```

The `config.example.json` file contains ALL available configuration options with comments explaining each setting.

### Quick Configurations

**Telegram Bot:**
```json
{
    "plugins": ["telegram", "claude"],
  "telegram": { "bot_token": "..." },
  "claude": { "api_key": "..." }
}
```

**WebSocket Gateway:**
```json
{
  "plugins": ["gateway"],
  "gateway": { "port": 18789, "bind": "0.0.0.0" }
}
```

**Local Testing:**
```json
{
    "plugins": ["claude"],
  "claude": { "api_key": "..." }
}
```

See `config.example.json` for all available options and detailed comments.

## Acknowledgments

Inspired by [OpenClaw](https://github.com/openclaw/openclaw) - a TypeScript-based personal AI assistant.
