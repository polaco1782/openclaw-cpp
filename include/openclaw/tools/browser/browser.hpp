#ifndef OPENCLAW_TOOLS_BROWSER_HPP
#define OPENCLAW_TOOLS_BROWSER_HPP

#include "../../plugin/tool.hpp"
#include "../../core/http_client.hpp"
#include "../../core/logger.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace openclaw {

// Browser tool plugin - HTTP fetching and web content extraction
class BrowserTool : public ToolPlugin {
public:
    BrowserTool();
    
    // Plugin interface
    const char* name() const;
    const char* version() const;
    const char* description() const;
    
    // Tool interface
    const char* tool_id() const;
    std::vector<std::string> actions() const;
    
    bool init(const Config& cfg);
    void shutdown();
    
    ToolResult execute(const std::string& action, const Json& params);

private:
    HttpClient http_;
    size_t max_content_length_;
    int timeout_secs_;
    
    ToolResult do_fetch(const Json& params);
    ToolResult do_extract_text(const Json& params);
    ToolResult do_get_links(const Json& params);
    ToolResult do_status();
    
    static std::string strip_html(const std::string& html);
    static std::string normalize_whitespace(const std::string& s);
    static std::vector<std::pair<std::string, std::string> > extract_links(
            const std::string& html, const std::string& base_url);
};

} // namespace openclaw

#endif // OPENCLAW_TOOLS_BROWSER_HPP
