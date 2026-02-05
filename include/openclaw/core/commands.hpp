#ifndef OPENCLAW_CORE_COMMANDS_HPP
#define OPENCLAW_CORE_COMMANDS_HPP

#include <openclaw/core/types.hpp>
#include <openclaw/core/config.hpp>
#include <openclaw/core/registry.hpp>
#include <openclaw/core/session.hpp>
#include <string>

namespace openclaw {

namespace commands {
    std::string cmd_ping(const Message& msg, Session& session, const std::string& args);
    std::string cmd_help(const Message& msg, Session& session, const std::string& args);
    std::string cmd_info(const Message& msg, Session& session, const std::string& args);
    std::string cmd_start(const Message& msg, Session& session, const std::string& args);
    std::string cmd_new(const Message& msg, Session& session, const std::string& args);
    std::string cmd_status(const Message& msg, Session& session, const std::string& args);
    std::string cmd_tools(const Message& msg, Session& session, const std::string& args);
}

// Register built-in core commands (/ping, /help, /info, /start, /new, /status, /tools)
void register_core_commands(const Config& cfg, PluginRegistry& registry);

} // namespace openclaw

#endif // OPENCLAW_CORE_COMMANDS_HPP
