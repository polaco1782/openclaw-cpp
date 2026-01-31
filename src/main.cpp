/*
 * OpenClaw C++11 - Personal AI Assistant Framework
 * 
 * A minimal, modular implementation inspired by OpenClaw
 * with dynamic plugin loading support.
 * 
 * Usage:
 *   ./openclaw [config.json]
 * 
 * Environment variables:
 *   OPENCLAW_PLUGIN_PATH     - Plugin search paths (colon-separated)
 *   TELEGRAM_BOT_TOKEN       - Telegram Bot API token
 *   WHATSAPP_PHONE_NUMBER_ID - WhatsApp Cloud API phone number ID
 *   WHATSAPP_ACCESS_TOKEN    - WhatsApp Cloud API access token
 *   ANTHROPIC_API_KEY        - Anthropic Claude API key
 *   CLAUDE_MODEL             - Claude model to use (optional)
 *   OPENCLAW_LOG_LEVEL       - Log level (debug, info, warn, error)
 */

#include <openclaw/core/types.hpp>
#include <openclaw/core/logger.hpp>
#include <openclaw/core/config.hpp>
#include <openclaw/core/json.hpp>
#include <openclaw/core/utils.hpp>
#include <openclaw/plugin/registry.hpp>
#include <openclaw/plugin/loader.hpp>
#include <openclaw/plugin/channel.hpp>
#include <openclaw/plugin/tool.hpp>
#include <openclaw/ai/ai.hpp>
#include <openclaw/session/session.hpp>
#include <openclaw/rate_limiter/rate_limiter.hpp>

#include <iostream>
#include <csignal>
#include <cstring>
#include <unistd.h>
#include <sstream>

namespace openclaw {

// ============================================================================
// Application Configuration
// ============================================================================

static const char* APP_VERSION = "0.5.0";
static const char* APP_NAME = "OpenClaw C++11";

// System prompt for the AI assistant
static const char* DEFAULT_SYSTEM_PROMPT = 
    "You are OpenClaw, a helpful AI assistant running on a minimal C++11 framework. "
    "You are friendly, concise, and helpful. Keep responses brief unless asked for detail. "
    "You can help with questions, coding, and general conversation.\n\n"
    "You have access to tools that let you browse the web and manage memory/tasks.";

// ============================================================================
// Application State
// ============================================================================

class Application {
public:
    // Singleton access
    static Application& instance() {
        static Application app;
        return app;
    }
    
    // Configuration
    Config& config() { return config_; }
    PluginLoader& loader() { return loader_; }
    PluginRegistry& registry() { return PluginRegistry::instance(); }
    SessionManager& sessions() { return SessionManager::instance(); }
    
    // State
    bool is_running() const { return running_; }
    void stop() { running_ = false; }
    
    // Rate limiting
    KeyedRateLimiter& user_limiter() { return user_limiter_; }
    MessageDebouncer& debouncer() { return debouncer_; }
    TypingIndicator& typing() { return typing_; }
    
    // System prompt (can be customized via config)
    const std::string& system_prompt() const { return system_prompt_; }
    void set_system_prompt(const std::string& prompt) { system_prompt_ = prompt; }
    
    // Initialize the application
    bool init(int argc, char* argv[]);
    
    // Run the main loop
    int run();
    
    // Cleanup
    void shutdown();

private:
    Application() 
        : running_(true)
        , user_limiter_(KeyedRateLimiter::TOKEN_BUCKET, 10, 2)
        , debouncer_(5)
        , system_prompt_(DEFAULT_SYSTEM_PROMPT) {}
    
    Application(const Application&);
    Application& operator=(const Application&);
    
    bool running_;
    Config config_;
    PluginLoader loader_;
    KeyedRateLimiter user_limiter_;
    MessageDebouncer debouncer_;
    TypingIndicator typing_;
    std::string system_prompt_;
};

// Global signal handler
static void signal_handler(int sig) {
    (void)sig;
    Application::instance().stop();
    LOG_INFO("Received shutdown signal");
}

// ============================================================================
// Command Handlers
// ============================================================================

// Base command handler
struct CommandHandler {
    const char* command;
    const char* description;
    std::string (*handler)(const Message& msg, const std::string& args);
};

// Built-in command implementations
namespace commands {

std::string cmd_ping(const Message& /*msg*/, const std::string& /*args*/) {
    return "Pong! 🏓";
}

std::string cmd_help(const Message& /*msg*/, const std::string& /*args*/) {
    std::ostringstream oss;
    oss << "OpenClaw C++11 Bot 🦞\n\n"
        << "Commands:\n"
        << "/ping - Check if bot is alive\n"
        << "/help - Show this help\n"
        << "/info - Show bot info\n"
        << "/new - Start a new conversation\n"
        << "/status - Show session status\n"
        << "/tools - List available tools\n"
        << "/fetch <url> - Fetch a web page\n"
        << "/links <url> - Extract links from a page\n\n"
        << "Or just send a message to chat with Claude AI!";
    return oss.str();
}

std::string cmd_info(const Message& msg, const std::string& /*args*/) {
    PluginRegistry& registry = PluginRegistry::instance();
    SessionManager& sessions = SessionManager::instance();
    
    AIPlugin* ai = registry.get_default_ai();
    std::string ai_info = ai ? 
        (ai->provider_id() + "/" + ai->default_model()) : "not configured";
    
    // Build channels list
    std::ostringstream channels_list;
    const std::vector<ChannelPlugin*>& channels = registry.channels();
    for (size_t i = 0; i < channels.size(); ++i) {
        if (i > 0) channels_list << ", ";
        channels_list << channels[i]->channel_id();
    }
    
    // Build tools list
    std::ostringstream tools_list;
    const std::vector<ToolPlugin*>& tools = registry.tools();
    for (size_t i = 0; i < tools.size(); ++i) {
        if (i > 0) tools_list << ", ";
        tools_list << tools[i]->tool_id();
    }
    
    std::ostringstream oss;
    oss << APP_NAME << " v" << APP_VERSION << "\n"
        << "Channels: " << (channels_list.str().empty() ? "none" : channels_list.str()) << "\n"
        << "Tools: " << (tools_list.str().empty() ? "none" : tools_list.str()) << "\n"
        << "AI: " << ai_info << "\n"
        << "Your channel: " << msg.channel << "\n"
        << "Plugins loaded: " << registry.plugins().size() << "\n"
        << "Active sessions: " << sessions.session_count();
    return oss.str();
}

std::string cmd_start(const Message& /*msg*/, const std::string& /*args*/) {
    return "Welcome to OpenClaw! 🦞\n\n"
           "I'm a personal AI assistant powered by Claude.\n"
           "Just send me a message to chat, or type /help for commands.";
}

std::string cmd_new(const Message& msg, const std::string& /*args*/) {
    SessionManager& sessions = SessionManager::instance();
    Session& session = sessions.get_session_for_message(msg);
    session.clear_history();
    return "🔄 Conversation cleared. Let's start fresh!";
}

std::string cmd_status(const Message& msg, const std::string& /*args*/) {
    SessionManager& sessions = SessionManager::instance();
    Session& session = sessions.get_session_for_message(msg);
    
    std::ostringstream oss;
    oss << "📊 Session Status\n\n"
        << "Session: " << session.key() << "\n"
        << "Messages: " << session.history().size() << "\n"
        << "Last active: " << format_timestamp(session.last_activity()) << "\n"
        << "Total sessions: " << sessions.session_count();
    return oss.str();
}

std::string cmd_tools(const Message& /*msg*/, const std::string& /*args*/) {
    PluginRegistry& registry = PluginRegistry::instance();
    const std::vector<ToolPlugin*>& tools = registry.tools();
    
    if (tools.empty()) {
        return "No tools available.";
    }
    
    std::ostringstream oss;
    oss << "🔧 Available Tools\n\n";
    
    for (size_t i = 0; i < tools.size(); ++i) {
        oss << "• " << tools[i]->tool_id() << " - " << tools[i]->description() << "\n";
        std::vector<std::string> actions = tools[i]->actions();
        oss << "  Actions: ";
        for (size_t j = 0; j < actions.size(); ++j) {
            if (j > 0) oss << ", ";
            oss << actions[j];
        }
        oss << "\n";
    }
    
    return oss.str();
}

std::string cmd_fetch(const Message& /*msg*/, const std::string& args) {
    std::string url = trim(args);
    
    if (url.empty()) {
        return "Usage: /fetch <url>\nExample: /fetch https://example.com";
    }
    
    PluginRegistry& registry = PluginRegistry::instance();
    Json params = Json::object();
    params.set("url", url);
    ToolResult result = registry.execute_tool("browser", "extract_text", params);
    
    if (result.success) {
        std::string text = result.data["text"].as_string();
        text = truncate_safe(text, 1000);
        if (text.size() == 1000) text += "...";
        return "📄 " + url + "\n\n" + text;
    }
    return "❌ Fetch failed: " + result.error;
}

std::string cmd_links(const Message& /*msg*/, const std::string& args) {
    std::string url = trim(args);
    
    if (url.empty()) {
        return "Usage: /links <url>\nExample: /links https://example.com";
    }
    
    PluginRegistry& registry = PluginRegistry::instance();
    Json params = Json::object();
    params.set("url", url);
    ToolResult result = registry.execute_tool("browser", "get_links", params);
    
    if (result.success) {
        const std::vector<Json>& links = result.data["links"].as_array();
        std::ostringstream ss;
        ss << "🔗 Links from " << url << "\n\n";
        for (size_t i = 0; i < links.size() && i < 10; ++i) {
            std::string href = links[i]["href"].as_string();
            std::string text = links[i]["text"].as_string();
            if (text.empty()) text = href;
            text = truncate_safe(text, 50);
            if (text.size() == 50) text += "...";
            ss << "• " << text << "\n  " << href << "\n";
        }
        int64_t total = result.data["count"].as_int();
        if (total > 10) {
            ss << "\n(" << total << " total links)";
        }
        return ss.str();
    }
    return "❌ Failed: " + result.error;
}

} // namespace commands

// Command registry
static CommandHandler g_commands[] = {
    { "/ping",   "Check if bot is alive",     commands::cmd_ping },
    { "/help",   "Show help message",         commands::cmd_help },
    { "/info",   "Show bot info",             commands::cmd_info },
    { "/start",  "Welcome message",           commands::cmd_start },
    { "/new",    "Clear conversation",        commands::cmd_new },
    { "/status", "Show session status",       commands::cmd_status },
    { "/tools",  "List available tools",      commands::cmd_tools },
    { "/fetch",  "Fetch web page",            commands::cmd_fetch },
    { "/links",  "Extract links from page",   commands::cmd_links },
    { NULL, NULL, NULL }
};

// ============================================================================
// Message Handler
// ============================================================================

std::string handle_ai_message(const Message& msg, Session& session) {
    Application& app = Application::instance();
    PluginRegistry& registry = app.registry();
    
    AIPlugin* ai = registry.get_default_ai();
    if (!ai || !ai->is_configured()) {
        return "🤖 AI not configured. Set ANTHROPIC_API_KEY to enable Claude.\n"
               "Type /help for available commands.";
    }
    
    // Start typing indicator
    app.typing().start_typing(msg.to);
    
    // Add user message to session history
    session.add_message(ConversationMessage::user(msg.text));
    
    // Build completion options
    CompletionOptions opts;
    opts.system_prompt = app.system_prompt();
    opts.max_tokens = 1024;
    
    LOG_DEBUG("Calling AI with %zu messages", session.history().size());
    CompletionResult result = ai->chat(session.history(), opts);
    
    // Stop typing indicator
    app.typing().stop_typing(msg.to);
    
    if (result.success) {
        // Add assistant response to history
        session.add_message(ConversationMessage::assistant(result.content));
        LOG_DEBUG("AI response (%d tokens): %s", 
                  result.usage.output_tokens, 
                  result.content.substr(0, 100).c_str());
        return result.content;
    }
    
    // Remove failed user message from history
    session.history().pop_back();
    return "❌ AI error: " + result.error;
}

void on_message(const Message& msg) {
    Application& app = Application::instance();
    
    // Deduplicate messages
    if (!app.debouncer().should_process(msg.id)) {
        LOG_DEBUG("Skipping duplicate message: %s", msg.id.c_str());
        return;
    }
    
    LOG_INFO("[%s] Message from %s: %s", 
             msg.channel.c_str(), msg.from_name.c_str(), msg.text.c_str());
    
    ChannelPlugin* channel = app.registry().get_channel(msg.channel);
    if (!channel || msg.text.empty()) {
        return;
    }
    
    // Check rate limit for user
    RateLimitResult rate_result = app.user_limiter().check(msg.from);
    if (!rate_result.allowed) {
        LOG_WARN("Rate limit exceeded for user %s, retry in %lldms", 
                 msg.from.c_str(), (long long)rate_result.retry_after_ms);
        return;
    }
    
    std::string response;
    
    // Get session for this message
    Session& session = app.sessions().get_session_for_message(msg);
    
    // Handle commands (start with /)
    if (msg.text[0] == '/') {
        // Parse command and arguments
        std::string cmd_text = msg.text;
        
        // Handle @botname suffix (e.g., /help@mybot)
        size_t at_pos = cmd_text.find('@');
        if (at_pos != std::string::npos) {
            size_t space_pos = cmd_text.find(' ');
            if (space_pos == std::string::npos || at_pos < space_pos) {
                cmd_text = cmd_text.substr(0, at_pos) + 
                          (space_pos != std::string::npos ? cmd_text.substr(space_pos) : "");
            }
        }
        
        // Find matching command
        for (int i = 0; g_commands[i].command != NULL; ++i) {
            size_t cmd_len = strlen(g_commands[i].command);
            if (starts_with(cmd_text, g_commands[i].command) &&
                (cmd_text.size() == cmd_len || cmd_text[cmd_len] == ' ')) {
                std::string args = cmd_text.size() > cmd_len ? 
                    cmd_text.substr(cmd_len + 1) : "";
                response = g_commands[i].handler(msg, args);
                break;
            }
        }
        
        // Unknown command - don't respond
        if (response.empty()) {
            return;
        }
    } else {
        // Regular message - send to AI
        response = handle_ai_message(msg, session);
    }
    
    // Send response
    if (!response.empty()) {
        SendResult result = channel->send_message(msg.to, response, msg.id);
        if (!result.success) {
            LOG_ERROR("Failed to send response: %s", result.error.c_str());
        }
    }
}

void on_error(const std::string& channel, const std::string& error) {
    LOG_ERROR("Channel %s error: %s", channel.c_str(), error.c_str());
}

// ============================================================================
// Application Implementation
// ============================================================================

void print_usage(const char* prog) {
    std::cout << APP_NAME << " - Personal AI Assistant Framework\n\n"
              << "Usage: " << prog << " [options] [config.json]\n\n"
              << "Options:\n"
              << "  -h, --help     Show this help message\n"
              << "  -v, --version  Show version\n\n"
              << "Config file format (JSON):\n"
              << "  {\n"
              << "    \"plugins\": [\"telegram\", \"claude\", \"browser\", \"memory\"],\n"
              << "    \"plugins_dir\": \"./plugins\",\n"
              << "    \"system_prompt\": \"You are a helpful assistant.\"\n"
              << "  }\n\n"
              << "Environment variables:\n"
              << "  OPENCLAW_PLUGIN_PATH     - Plugin search paths (colon-separated)\n"
              << "  TELEGRAM_BOT_TOKEN       - Telegram Bot API token\n"
              << "  ANTHROPIC_API_KEY        - Anthropic Claude API key\n"
              << "  OPENCLAW_LOG_LEVEL       - Log level (debug, info, warn, error)\n\n"
              << "Example:\n"
              << "  export TELEGRAM_BOT_TOKEN=123456:ABC...\n"
              << "  export ANTHROPIC_API_KEY=sk-ant-...\n"
              << "  " << prog << "\n";
}

void print_version() {
    std::cout << APP_NAME << " v" << APP_VERSION << " (dynamic plugins)\n"
              << "Available plugins: telegram, whatsapp, browser, claude, memory\n";
}

bool Application::init(int argc, char* argv[]) {
    const char* config_file = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return false;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return false;
        }
        config_file = argv[i];
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Configure logging
    const char* log_level = std::getenv("OPENCLAW_LOG_LEVEL");
    if (log_level) {
        if (strcmp(log_level, "debug") == 0) {
            Logger::instance().set_level(LogLevel::DEBUG);
        } else if (strcmp(log_level, "warn") == 0) {
            Logger::instance().set_level(LogLevel::WARN);
        } else if (strcmp(log_level, "error") == 0) {
            Logger::instance().set_level(LogLevel::ERROR);
        }
    }
    
    LOG_INFO("%s v%s starting...", APP_NAME, APP_VERSION);
    
    // Load configuration
    if (config_file) {
        if (!config_.load_file(config_file)) {
            LOG_WARN("Failed to load config from %s, using defaults", config_file);
        } else {
            LOG_INFO("Loaded config from %s", config_file);
        }
    }
    
    // Load custom system prompt from config if present
    std::string custom_prompt = config_.get_string("system_prompt", "");
    if (!custom_prompt.empty()) {
        system_prompt_ = custom_prompt;
        LOG_DEBUG("Using custom system prompt from config");
    }
    
    // Configure session manager
    sessions().set_max_history(static_cast<size_t>(
        config_.get_int("session.max_history", 20)));
    
    // Configure plugin search paths
    loader_.add_search_path("./bin/plugins");
    loader_.add_search_path("./plugins");
    
    std::string plugins_dir = config_.get_string("plugins_dir", "");
    if (!plugins_dir.empty()) {
        loader_.add_search_path(plugins_dir);
    }
    
    // Load plugins from config or defaults
    int loaded = loader_.load_from_config(config_);
    
    if (loaded == 0) {
        LOG_INFO("No plugins in config, loading defaults...");
        const char* defaults[] = {"telegram", "claude", "browser", "memory", NULL};
        for (int i = 0; defaults[i]; ++i) {
            if (loader_.load(defaults[i])) {
                loaded++;
            }
        }
    }
    
    LOG_INFO("Loaded %d plugins", loaded);
    
    // Register plugins with registry
    const std::vector<LoadedPlugin>& plugins = loader_.plugins();
    for (size_t i = 0; i < plugins.size(); ++i) {
        if (plugins[i].instance) {
            registry().register_plugin(plugins[i].instance);
            LOG_DEBUG("Registered plugin: %s (%s)", 
                      plugins[i].info.name, plugins[i].info.type);
        }
    }
    
    LOG_INFO("Registered %zu plugins (%zu channels, %zu tools, %zu AI providers)",
             registry().plugins().size(), 
             registry().channels().size(),
             registry().tools().size(),
             registry().ai_providers().size());
    
    // Initialize all plugins
    registry().init_all(config_);
    
    // Set callbacks on channels
    const std::vector<ChannelPlugin*>& channels = registry().channels();
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i]->is_initialized()) {
            channels[i]->set_message_callback(on_message);
            channels[i]->set_error_callback(on_error);
        }
    }
    
    // Start channels
    int started_count = 0;
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i]->is_initialized() && channels[i]->start()) {
            LOG_INFO("Started channel: %s", channels[i]->channel_id());
            started_count++;
        }
    }
    
    if (started_count == 0) {
        LOG_ERROR("No channels started. Configure at least one channel:");
        LOG_ERROR("  1. Set TELEGRAM_BOT_TOKEN for Telegram");
        LOG_ERROR("  2. Ensure telegram.so plugin is in plugin path");
        LOG_ERROR("  3. Add \"telegram\" to plugins array in config.json");
        return false;
    }
    
    // Log AI status
    AIPlugin* ai = registry().get_default_ai();
    if (ai && ai->is_configured()) {
        LOG_INFO("AI provider: %s (%s)", ai->provider_id().c_str(), ai->default_model().c_str());
    } else {
        LOG_WARN("No AI provider configured. Set ANTHROPIC_API_KEY for Claude.");
    }
    
    LOG_INFO("%d channel(s) started, ready to receive messages", started_count);
    return true;
}

int Application::run() {
    LOG_INFO("Entering main loop");
    
    while (running_) {
        registry().poll_all_channels();
        sleep_ms(100);
        
        // Periodic cleanup (every ~10 seconds)
        static int cleanup_counter = 0;
        if (++cleanup_counter >= 100) {
            cleanup_counter = 0;
            sessions().cleanup_inactive(3600);  // Remove sessions older than 1 hour
            user_limiter_.cleanup(3600);
        }
    }
    
    return 0;
}

void Application::shutdown() {
    LOG_INFO("Shutting down...");
    
    registry().stop_all_channels();
    registry().shutdown_all();
    loader_.unload_all();
    
    LOG_INFO("Goodbye!");
}

} // namespace openclaw

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    openclaw::Application& app = openclaw::Application::instance();
    
    if (!app.init(argc, argv)) {
        // init returns false for --help/--version or fatal errors
        return app.is_running() ? 1 : 0;
    }
    
    int result = app.run();
    app.shutdown();
    
    return result;
}
