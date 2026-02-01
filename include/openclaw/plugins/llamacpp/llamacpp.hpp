/*
 * OpenClaw C++11 - Llama.cpp AI Plugin
 * 
 * Implementation of the llama.cpp server API.
 * Uses the OpenAI-compatible API (http://localhost:8080/v1/chat/completions).
 * 
 * Config:
 *   llamacpp.url          - Server URL (default: http://localhost:8080)
 *   llamacpp.model        - Model name (optional)
 *   llamacpp.api_key      - API key if server requires authentication (optional)
 */
#ifndef OPENCLAW_PLUGINS_LLAMACPP_HPP
#define OPENCLAW_PLUGINS_LLAMACPP_HPP

#include <openclaw/ai/ai.hpp>
#include <openclaw/core/http_client.hpp>
#include <openclaw/core/logger.hpp>
#include <openclaw/core/config.hpp>
#include <sstream>

namespace openclaw {

class LlamaCppAI : public AIPlugin {
public:
    LlamaCppAI();
    
    // Plugin interface
    const char* name() const;
    const char* version() const;
    const char* description() const;
    
    bool init(const Config& cfg);
    void shutdown();
    bool is_initialized() const;
    
    // AIPlugin interface
    std::string provider_id() const;
    std::vector<std::string> available_models() const;
    std::string default_model() const;
    bool is_configured() const;
    
    // Single prompt completion
    CompletionResult complete(
        const std::string& prompt,
        const CompletionOptions& opts = CompletionOptions()
    );
    
    // Conversation completion
    CompletionResult chat(
        const std::vector<ConversationMessage>& messages,
        const CompletionOptions& opts = CompletionOptions()
    );
    
    // Convenience method: simple question-answer
    std::string ask(const std::string& question, const std::string& system = "");
    
    // Convenience method: continue a conversation
    std::string reply(
        std::vector<ConversationMessage>& history,
        const std::string& user_message,
        const std::string& system = ""
    );

private:
    std::string server_url_;
    std::string api_key_;
    std::string default_model_;
    bool initialized_;
};

} // namespace openclaw

#endif // OPENCLAW_PLUGINS_LLAMACPP_HPP
