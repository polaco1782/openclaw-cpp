#include <openclaw/ai/claude.hpp>
#include <openclaw/plugin/loader.hpp>
#include <cstdlib>
#include <sstream>

namespace openclaw {

ClaudeAI::ClaudeAI()
    : api_key_()
    , default_model_("claude-sonnet-4-20250514")
    , api_url_("https://api.anthropic.com/v1/messages")
    , api_version_("2023-06-01")
    , initialized_(false)
{}

const char* ClaudeAI::name() const { return "Claude AI"; }
const char* ClaudeAI::version() const { return "1.0.0"; }
const char* ClaudeAI::description() const { 
    return "Anthropic Claude AI provider using Messages API"; 
}

bool ClaudeAI::init(const Config& cfg) {
    api_key_ = cfg.get_string("claude.api_key", "");
    if (api_key_.empty()) {
        const char* env_key = std::getenv("ANTHROPIC_API_KEY");
        if (env_key) api_key_ = env_key;
    }
    
    std::string model = cfg.get_string("claude.model", "");
    if (model.empty()) {
        const char* env_model = std::getenv("CLAUDE_MODEL");
        if (env_model && env_model[0]) model = env_model;
    }
    if (!model.empty()) {
        default_model_ = model;
    }
    
    std::string url = cfg.get_string("claude.api_url", "");
    if (!url.empty()) {
        api_url_ = url;
    }
    
    if (api_key_.empty()) {
        LOG_WARN("Claude AI: No API key configured (set ANTHROPIC_API_KEY)");
        initialized_ = false;
        return false;
    }
    
    LOG_INFO("Claude AI initialized with model: %s", default_model_.c_str());
    initialized_ = true;
    return true;
}

void ClaudeAI::shutdown() {
    initialized_ = false;
}

bool ClaudeAI::is_initialized() const { return initialized_; }

std::string ClaudeAI::provider_id() const { return "claude"; }

std::vector<std::string> ClaudeAI::available_models() const {
    std::vector<std::string> models;
    models.push_back("claude-opus-4-20250514");
    models.push_back("claude-sonnet-4-20250514");
    models.push_back("claude-haiku-3-5-20241022");
    models.push_back("claude-3-opus-20240229");
    models.push_back("claude-3-sonnet-20240229");
    models.push_back("claude-3-haiku-20240307");
    return models;
}

std::string ClaudeAI::default_model() const { return default_model_; }

bool ClaudeAI::is_configured() const { return !api_key_.empty(); }

CompletionResult ClaudeAI::complete(
    const std::string& prompt,
    const CompletionOptions& opts
) {
    std::vector<ConversationMessage> messages;
    messages.push_back(ConversationMessage::user(prompt));
    return chat(messages, opts);
}

CompletionResult ClaudeAI::chat(
    const std::vector<ConversationMessage>& messages,
    const CompletionOptions& opts
) {
    if (!initialized_) {
        return CompletionResult::fail("Claude AI not initialized");
    }
    
    if (messages.empty()) {
        return CompletionResult::fail("No messages provided");
    }
    
    Json request = Json::object();
    
    std::string model = opts.model.empty() ? default_model_ : opts.model;
    request.set("model", model);
    
    int max_tokens = opts.max_tokens > 0 ? opts.max_tokens : 4096;
    request.set("max_tokens", static_cast<int64_t>(max_tokens));
    
    if (opts.temperature >= 0.0 && opts.temperature <= 1.0) {
        request.set("temperature", opts.temperature);
    }
    
    if (!opts.system_prompt.empty()) {
        request.set("system", opts.system_prompt);
    }
    
    Json msgs = Json::array();
    for (size_t i = 0; i < messages.size(); ++i) {
        const ConversationMessage& msg = messages[i];
        
        if (msg.role == MessageRole::SYSTEM) {
            if (opts.system_prompt.empty() && i == 0) {
                request.set("system", msg.content);
            }
            continue;
        }
        
        Json m = Json::object();
        m.set("role", role_to_string(msg.role));
        m.set("content", msg.content);
        msgs.push(m);
    }
    request.set("messages", msgs);
    
    if (opts.stream && opts.on_chunk) {
        request.set("stream", true);
    }
    
    std::string request_body = request.dump();
    LOG_DEBUG("Claude request: %s", request_body.c_str());
    
    HttpClient http;
    std::map<std::string, std::string> headers;
    headers["x-api-key"] = api_key_;
    headers["anthropic-version"] = api_version_;
    headers["Content-Type"] = "application/json";
    
    HttpResponse response = http.post_json(api_url_, request_body, headers);
    
    if (response.status_code == 0) {
        return CompletionResult::fail("HTTP request failed: " + response.body);
    }
    
    LOG_DEBUG("Claude response [%d]: %s", 
              response.status_code, response.body.c_str());
    
    Json resp = response.json();
    
    if (response.status_code != 200) {
        std::string error_msg = "API error";
        if (resp.type() == Json::OBJECT) {
            Json error = resp["error"];
            if (error.type() == Json::OBJECT) {
                std::string msg = error["message"].as_string();
                std::string type = error["type"].as_string();
                if (!msg.empty()) {
                    error_msg = type.empty() ? msg : (type + ": " + msg);
                }
            }
        }
        return CompletionResult::fail(error_msg + " (HTTP " + 
                                     std::to_string(response.status_code) + ")");
    }
    
    CompletionResult result;
    result.success = true;
    result.model = resp["model"].as_string();
    result.stop_reason = resp["stop_reason"].as_string();
    
    Json content = resp["content"];
    if (content.type() == Json::ARRAY) {
        std::ostringstream text;
        const std::vector<Json>& blocks = content.as_array();
        for (size_t i = 0; i < blocks.size(); ++i) {
            const Json& block = blocks[i];
            std::string block_type = block["type"].as_string();
            if (block_type == "text") {
                text << block["text"].as_string();
            }
        }
        result.content = text.str();
    }
    
    Json usage = resp["usage"];
    if (usage.type() == Json::OBJECT) {
        result.usage.input_tokens = static_cast<int>(usage["input_tokens"].as_int());
        result.usage.output_tokens = static_cast<int>(usage["output_tokens"].as_int());
        result.usage.total_tokens = result.usage.input_tokens + result.usage.output_tokens;
    }
    
    return result;
}

std::string ClaudeAI::ask(const std::string& question, const std::string& system) {
    CompletionOptions opts;
    if (!system.empty()) {
        opts.system_prompt = system;
    }
    CompletionResult result = complete(question, opts);
    if (result.success) {
        return result.content;
    }
    return "Error: " + result.error;
}

std::string ClaudeAI::reply(
    std::vector<ConversationMessage>& history,
    const std::string& user_message,
    const std::string& system
) {
    history.push_back(ConversationMessage::user(user_message));
    
    CompletionOptions opts;
    if (!system.empty()) {
        opts.system_prompt = system;
    }
    
    CompletionResult result = chat(history, opts);
    
    if (result.success) {
        history.push_back(ConversationMessage::assistant(result.content));
        return result.content;
    }
    
    history.pop_back();
    return "Error: " + result.error;
}

} // namespace openclaw

// Export plugin for dynamic loading
OPENCLAW_DECLARE_PLUGIN(openclaw::ClaudeAI, "claude", "1.0.0", 
                        "Anthropic Claude AI provider", "ai")
