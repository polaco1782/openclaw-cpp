#include <openclaw/channels/whatsapp/whatsapp.hpp>
#include <openclaw/plugin/loader.hpp>
#include <cstdlib>
#include <sstream>
#include <ctime>

namespace openclaw {

WhatsAppChannel::WhatsAppChannel() 
    : status_(ChannelStatus::STOPPED)
    , mode_(MODE_NONE)
    , poll_interval_(5)
    , last_poll_time_(0) {}

const char* WhatsAppChannel::name() const { return "whatsapp"; }
const char* WhatsAppChannel::version() const { return "1.0.0"; }
const char* WhatsAppChannel::description() const { return "WhatsApp messaging channel"; }
const char* WhatsAppChannel::channel_id() const { return "whatsapp"; }

ChannelCapabilities WhatsAppChannel::capabilities() const {
    ChannelCapabilities caps;
    caps.supports_groups = true;
    caps.supports_reactions = true;
    caps.supports_media = true;
    caps.supports_edit = false;
    caps.supports_delete = true;
    return caps;
}

bool WhatsAppChannel::init(const Config& cfg) {
    phone_number_id_ = cfg.get_channel_string("whatsapp", "phone_number_id");
    if (phone_number_id_.empty()) {
        const char* env = std::getenv("WHATSAPP_PHONE_NUMBER_ID");
        if (env) phone_number_id_ = env;
    }
    
    access_token_ = cfg.get_channel_string("whatsapp", "access_token");
    if (access_token_.empty()) {
        const char* env = std::getenv("WHATSAPP_ACCESS_TOKEN");
        if (env) access_token_ = env;
    }
    
    bridge_url_ = cfg.get_channel_string("whatsapp", "bridge_url");
    if (bridge_url_.empty()) {
        const char* env = std::getenv("WHATSAPP_BRIDGE_URL");
        if (env) bridge_url_ = env;
    }
    
    if (!phone_number_id_.empty() && !access_token_.empty()) {
        mode_ = MODE_CLOUD_API;
        api_base_ = "https://graph.facebook.com/v18.0/" + phone_number_id_;
        LOG_INFO("WhatsApp: configured for Cloud API mode");
    } else if (!bridge_url_.empty()) {
        mode_ = MODE_BRIDGE;
        api_base_ = bridge_url_;
        LOG_INFO("WhatsApp: configured for Bridge mode at %s", bridge_url_.c_str());
    } else {
        LOG_ERROR("WhatsApp: no valid configuration found");
        LOG_ERROR("  Set WHATSAPP_PHONE_NUMBER_ID + WHATSAPP_ACCESS_TOKEN for Cloud API");
        LOG_ERROR("  Or set WHATSAPP_BRIDGE_URL for local bridge mode");
        return false;
    }
    
    poll_interval_ = static_cast<int>(cfg.get_int("whatsapp.poll_interval", 5));
    
    initialized_ = true;
    return true;
}

void WhatsAppChannel::shutdown() {
    stop();
    initialized_ = false;
    LOG_INFO("WhatsApp: shutdown");
}

bool WhatsAppChannel::start() {
    if (!initialized_) {
        LOG_ERROR("WhatsApp: cannot start - not initialized");
        return false;
    }
    
    status_ = ChannelStatus::STARTING;
    
    if (mode_ == MODE_CLOUD_API) {
        if (!verify_cloud_api()) {
            status_ = ChannelStatus::ERROR;
            return false;
        }
    } else if (mode_ == MODE_BRIDGE) {
        if (!verify_bridge()) {
            status_ = ChannelStatus::ERROR;
            return false;
        }
    }
    
    status_ = ChannelStatus::RUNNING;
    LOG_INFO("WhatsApp: started in %s mode", 
             mode_ == MODE_CLOUD_API ? "Cloud API" : "Bridge");
    return true;
}

bool WhatsAppChannel::stop() {
    if (status_ == ChannelStatus::STOPPED) return true;
    
    status_ = ChannelStatus::STOPPING;
    status_ = ChannelStatus::STOPPED;
    
    LOG_INFO("WhatsApp: stopped");
    return true;
}

ChannelStatus WhatsAppChannel::status() const { return status_; }

SendResult WhatsAppChannel::send_message(const std::string& to, const std::string& text) {
    if (mode_ == MODE_CLOUD_API) {
        return send_cloud_api(to, text);
    } else if (mode_ == MODE_BRIDGE) {
        return send_bridge(to, text);
    }
    return SendResult::fail("WhatsApp not configured");
}

SendResult WhatsAppChannel::send_message(const std::string& to, const std::string& text,
                                         const std::string& reply_to) {
    if (mode_ == MODE_CLOUD_API) {
        return send_cloud_api(to, text, reply_to);
    } else if (mode_ == MODE_BRIDGE) {
        return send_bridge(to, text, reply_to);
    }
    return SendResult::fail("WhatsApp not configured");
}

void WhatsAppChannel::poll() {
    if (status_ != ChannelStatus::RUNNING) return;
    
    if (mode_ == MODE_BRIDGE) {
        poll_bridge();
    }
}

WhatsAppChannel::Mode WhatsAppChannel::mode() const { return mode_; }

std::string WhatsAppChannel::normalize_phone(const std::string& phone) {
    std::string result;
    for (size_t i = 0; i < phone.size(); ++i) {
        char c = phone[i];
        if (c >= '0' && c <= '9') {
            result += c;
        }
    }
    while (!result.empty() && result[0] == '0') {
        result.erase(0, 1);
    }
    return result;
}

bool WhatsAppChannel::verify_cloud_api() {
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + access_token_;
    
    HttpResponse resp = http_.get(api_base_ + "?fields=id,display_phone_number", headers);
    if (!resp.ok()) {
        LOG_ERROR("WhatsApp Cloud API: connection failed - %s", resp.error.c_str());
        return false;
    }
    
    Json result = resp.json();
    if (result.has("error")) {
        LOG_ERROR("WhatsApp Cloud API: %s", result["error"]["message"].as_string().c_str());
        return false;
    }
    
    std::string display_number = result["display_phone_number"].as_string();
    LOG_INFO("WhatsApp Cloud API: connected as %s", display_number.c_str());
    return true;
}

SendResult WhatsAppChannel::send_cloud_api(const std::string& to, const std::string& text,
                                           const std::string& reply_to) {
    std::string phone = normalize_phone(to);
    
    Json message = Json::object();
    message.set("messaging_product", Json("whatsapp"));
    message.set("recipient_type", Json("individual"));
    message.set("to", phone);
    message.set("type", Json("text"));
    
    Json text_obj = Json::object();
    text_obj.set("preview_url", false);
    text_obj.set("body", text);
    message.set("text", text_obj);
    
    if (!reply_to.empty()) {
        Json context = Json::object();
        context.set("message_id", reply_to);
        message.set("context", context);
    }
    
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + access_token_;
    
    HttpResponse resp = http_.post_json(api_base_ + "/messages", message, headers);
    
    if (!resp.ok()) {
        return SendResult::fail("HTTP error: " + resp.error);
    }
    
    Json result = resp.json();
    if (result.has("error")) {
        return SendResult::fail("API error: " + result["error"]["message"].as_string());
    }
    
    std::string msg_id = result["messages"][0]["id"].as_string();
    LOG_DEBUG("WhatsApp: sent message to %s (id=%s)", phone.c_str(), msg_id.c_str());
    return SendResult::ok(msg_id);
}

bool WhatsAppChannel::verify_bridge() {
    HttpResponse resp = http_.get(api_base_ + "/status");
    if (!resp.ok()) {
        LOG_ERROR("WhatsApp Bridge: connection failed - %s", resp.error.c_str());
        return false;
    }
    
    Json result = resp.json();
    if (!result["connected"].as_bool()) {
        LOG_WARN("WhatsApp Bridge: not connected to WhatsApp Web");
        LOG_WARN("WhatsApp Bridge: scan QR code at %s/qr", bridge_url_.c_str());
    } else {
        std::string phone = result["phone"].as_string();
        LOG_INFO("WhatsApp Bridge: connected as %s", phone.c_str());
    }
    return true;
}

SendResult WhatsAppChannel::send_bridge(const std::string& to, const std::string& text,
                                        const std::string& reply_to) {
    Json message = Json::object();
    message.set("to", to);
    message.set("text", text);
    if (!reply_to.empty()) {
        message.set("reply_to", reply_to);
    }
    
    HttpResponse resp = http_.post_json(api_base_ + "/send", message);
    
    if (!resp.ok()) {
        return SendResult::fail("HTTP error: " + resp.error);
    }
    
    Json result = resp.json();
    if (!result["success"].as_bool()) {
        return SendResult::fail("Bridge error: " + result["error"].as_string());
    }
    
    std::string msg_id = result["message_id"].as_string();
    LOG_DEBUG("WhatsApp: sent message to %s via bridge (id=%s)", to.c_str(), msg_id.c_str());
    return SendResult::ok(msg_id);
}

void WhatsAppChannel::poll_bridge() {
    time_t now = time(NULL);
    if (now - last_poll_time_ < poll_interval_) return;
    last_poll_time_ = now;
    
    HttpResponse resp = http_.get(api_base_ + "/messages");
    if (!resp.ok()) {
        LOG_WARN("WhatsApp: poll failed - %s", resp.error.c_str());
        return;
    }
    
    Json result = resp.json();
    const std::vector<Json>& messages = result["messages"].as_array();
    
    for (size_t i = 0; i < messages.size(); ++i) {
        process_bridge_message(messages[i]);
    }
}

void WhatsAppChannel::process_bridge_message(const Json& msg) {
    Message m;
    m.channel = "whatsapp";
    m.id = msg["id"].as_string();
    m.from = msg["from"].as_string();
    m.from_name = msg["from_name"].as_string(m.from);
    m.to = msg["to"].as_string();
    m.text = msg["text"].as_string();
    m.timestamp = msg["timestamp"].as_int();
    
    if (msg["is_group"].as_bool()) {
        m.chat_type = "group";
    } else {
        m.chat_type = "direct";
    }
    
    if (msg.has("reply_to")) {
        m.reply_to_id = msg["reply_to"].as_string();
    }
    
    LOG_DEBUG("WhatsApp: received message from %s: %s", 
              m.from_name.c_str(), m.text.c_str());
    
    emit_message(m);
}

} // namespace openclaw

// Export plugin for dynamic loading
OPENCLAW_DECLARE_PLUGIN(openclaw::WhatsAppChannel, "whatsapp", "1.0.0", 
                        "WhatsApp messaging channel", "channel")
