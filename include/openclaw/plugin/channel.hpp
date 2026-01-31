#ifndef OPENCLAW_PLUGIN_CHANNEL_HPP
#define OPENCLAW_PLUGIN_CHANNEL_HPP

#include "plugin.hpp"
#include "../core/types.hpp"
#include <string>
#include <functional>

namespace openclaw {

// Channel plugin interface - for messaging integrations
class ChannelPlugin : public Plugin {
public:
    virtual ~ChannelPlugin() {}
    
    // Channel metadata
    virtual const char* channel_id() const = 0;
    virtual ChannelCapabilities capabilities() const = 0;
    
    // Lifecycle
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual ChannelStatus status() const = 0;
    
    // Message handling
    virtual SendResult send_message(const std::string& to, const std::string& text) = 0;
    virtual SendResult send_message(const std::string& to, const std::string& text, 
                                     const std::string& reply_to) {
        (void)reply_to; // Ignore by default
        return send_message(to, text);
    }
    
    // Poll for new messages (call regularly in event loop)
    virtual void poll() = 0;
    
    // Callbacks
    void set_message_callback(MessageCallback cb) { on_message_ = cb; }
    void set_error_callback(ErrorCallback cb) { on_error_ = cb; }
    
protected:
    MessageCallback on_message_;
    ErrorCallback on_error_;
    
    // Helper for subclasses to emit messages
    void emit_message(const Message& msg) {
        if (on_message_) on_message_(msg);
    }
    
    void emit_error(const std::string& err) {
        if (on_error_) on_error_(channel_id(), err);
    }
};

} // namespace openclaw

#endif // OPENCLAW_PLUGIN_CHANNEL_HPP
