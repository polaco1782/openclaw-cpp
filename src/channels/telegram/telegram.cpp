#include <openclaw/channels/telegram/telegram.hpp>
#include <openclaw/plugin/loader.hpp>
#include <cstdlib>
#include <sstream>

namespace openclaw {

TelegramChannel::TelegramChannel() 
    : status_(ChannelStatus::STOPPED)
    , last_update_id_(0)
    , poll_timeout_(30) {}

const char* TelegramChannel::name() const { return "telegram"; }
const char* TelegramChannel::version() const { return "1.0.0"; }
const char* TelegramChannel::description() const { return "Telegram Bot API channel"; }
const char* TelegramChannel::channel_id() const { return "telegram"; }

ChannelCapabilities TelegramChannel::capabilities() const {
    ChannelCapabilities caps;
    caps.supports_groups = true;
    caps.supports_reactions = true;
    caps.supports_media = true;
    caps.supports_edit = true;
    caps.supports_delete = true;
    return caps;
}

bool TelegramChannel::init(const Config& cfg) {
    bot_token_ = cfg.get_channel_string("telegram", "bot_token");
    
    if (bot_token_.empty()) {
        const char* env_token = std::getenv("TELEGRAM_BOT_TOKEN");
        if (env_token) bot_token_ = env_token;
    }
    
    if (bot_token_.empty()) {
        LOG_ERROR("Telegram: bot_token not configured");
        return false;
    }
    
    api_base_ = "https://api.telegram.org/bot" + bot_token_;
    
    poll_timeout_ = static_cast<int>(cfg.get_channel_string("telegram", "poll_timeout", "30")[0] - '0') * 10;
    if (poll_timeout_ <= 0 || poll_timeout_ > 60) poll_timeout_ = 30;
    
    LOG_INFO("Telegram: initialized with poll_timeout=%d", poll_timeout_);
    initialized_ = true;
    return true;
}

void TelegramChannel::shutdown() {
    stop();
    initialized_ = false;
    LOG_INFO("Telegram: shutdown");
}

bool TelegramChannel::start() {
    if (!initialized_) {
        LOG_ERROR("Telegram: cannot start - not initialized");
        return false;
    }
    
    status_ = ChannelStatus::STARTING;
    
    HttpResponse resp = http_.get(api_base_ + "/getMe");
    if (!resp.ok()) {
        LOG_ERROR("Telegram: failed to connect - %s", resp.error.c_str());
        status_ = ChannelStatus::ERROR;
        return false;
    }
    
    Json result = resp.json();
    if (!result["ok"].as_bool()) {
        LOG_ERROR("Telegram: API error - %s", result["description"].as_string().c_str());
        status_ = ChannelStatus::ERROR;
        return false;
    }
    
    Json bot_info = result["result"];
    bot_id_ = bot_info["id"].as_int();
    bot_username_ = bot_info["username"].as_string();
    
    LOG_INFO("Telegram: connected as @%s (id=%lld)", 
             bot_username_.c_str(), (long long)bot_id_);
    
    status_ = ChannelStatus::RUNNING;
    return true;
}

bool TelegramChannel::stop() {
    if (status_ == ChannelStatus::STOPPED) return true;
    
    status_ = ChannelStatus::STOPPING;
    status_ = ChannelStatus::STOPPED;
    
    LOG_INFO("Telegram: stopped");
    return true;
}

ChannelStatus TelegramChannel::status() const { return status_; }

SendResult TelegramChannel::send_message(const std::string& to, const std::string& text) {
    return send_message_impl(to, text, 0);
}

SendResult TelegramChannel::send_message(const std::string& to, const std::string& text,
                                         const std::string& reply_to) {
    int64_t reply_id = 0;
    if (!reply_to.empty()) {
        reply_id = std::strtoll(reply_to.c_str(), NULL, 10);
    }
    return send_message_impl(to, text, reply_id);
}

void TelegramChannel::poll() {
    if (status_ != ChannelStatus::RUNNING) return;
    
    std::ostringstream url;
    url << api_base_ << "/getUpdates?timeout=" << poll_timeout_;
    if (last_update_id_ > 0) {
        url << "&offset=" << (last_update_id_ + 1);
    }
    url << "&allowed_updates=" << "[\"message\",\"edited_message\"]";
    
    http_.set_timeout((poll_timeout_ + 5) * 1000);
    
    HttpResponse resp = http_.get(url.str());
    if (!resp.ok()) {
        LOG_WARN("Telegram: poll failed - %s", resp.error.c_str());
        return;
    }
    
    Json result = resp.json();
    if (!result["ok"].as_bool()) {
        LOG_WARN("Telegram: poll API error - %s", result["description"].as_string().c_str());
        return;
    }
    
    const std::vector<Json>& updates = result["result"].as_array();
    for (size_t i = 0; i < updates.size(); ++i) {
        process_update(updates[i]);
    }
}

SendResult TelegramChannel::send_message_impl(const std::string& to, const std::string& text, int64_t reply_to) {
    Json params = Json::object();
    params.set("chat_id", to);
    params.set("text", text);
    params.set("parse_mode", Json("HTML"));
    
    if (reply_to > 0) {
        params.set("reply_to_message_id", Json(reply_to));
    }
    
    HttpResponse resp = http_.post_json(api_base_ + "/sendMessage", params);
    
    if (!resp.ok()) {
        return SendResult::fail("HTTP error: " + resp.error);
    }
    
    Json result = resp.json();
    if (!result["ok"].as_bool()) {
        return SendResult::fail("API error: " + result["description"].as_string());
    }
    
    std::ostringstream msg_id;
    msg_id << result["result"]["message_id"].as_int();
    
    LOG_DEBUG("Telegram: sent message to %s (id=%s)", to.c_str(), msg_id.str().c_str());
    return SendResult::ok(msg_id.str());
}

void TelegramChannel::process_update(const Json& update) {
    int64_t update_id = update["update_id"].as_int();
    if (update_id > last_update_id_) {
        last_update_id_ = update_id;
    }
    
    const Json& msg = update.has("message") ? update["message"] : update["edited_message"];
    if (msg.is_null()) return;
    
    Message m;
    m.channel = "telegram";
    
    std::ostringstream id_ss;
    id_ss << msg["message_id"].as_int();
    m.id = id_ss.str();
    
    const Json& chat = msg["chat"];
    std::ostringstream chat_id;
    chat_id << chat["id"].as_int();
    m.to = chat_id.str();
    
    std::string chat_type = chat["type"].as_string();
    if (chat_type == "private") {
        m.chat_type = "direct";
    } else if (chat_type == "group" || chat_type == "supergroup") {
        m.chat_type = "group";
    } else if (chat_type == "channel") {
        m.chat_type = "channel";
    }
    
    const Json& from = msg["from"];
    if (!from.is_null()) {
        std::ostringstream from_id;
        from_id << from["id"].as_int();
        m.from = from_id.str();
        
        std::string first = from["first_name"].as_string();
        std::string last = from["last_name"].as_string();
        m.from_name = first;
        if (!last.empty()) {
            m.from_name += " " + last;
        }
        std::string username = from["username"].as_string();
        if (!username.empty()) {
            m.from_name += " (@" + username + ")";
        }
    }
    
    m.text = msg["text"].as_string();
    if (m.text.empty()) {
        m.text = msg["caption"].as_string();
    }
    
    const Json& reply = msg["reply_to_message"];
    if (!reply.is_null()) {
        std::ostringstream reply_id;
        reply_id << reply["message_id"].as_int();
        m.reply_to_id = reply_id.str();
    }
    
    m.timestamp = msg["date"].as_int();
    
    LOG_DEBUG("Telegram: received message from %s: %s", 
              m.from_name.c_str(), m.text.c_str());
    
    emit_message(m);
}

} // namespace openclaw

// Export plugin for dynamic loading
OPENCLAW_DECLARE_PLUGIN(openclaw::TelegramChannel, "telegram", "1.0.0", 
                        "Telegram Bot API channel", "channel")
