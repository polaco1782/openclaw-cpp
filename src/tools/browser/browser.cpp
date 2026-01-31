#include <openclaw/tools/browser/browser.hpp>
#include <openclaw/plugin/loader.hpp>
#include <algorithm>
#include <cctype>

namespace openclaw {

BrowserTool::BrowserTool() 
    : max_content_length_(100000)
    , timeout_secs_(30) {}

const char* BrowserTool::name() const { return "browser"; }
const char* BrowserTool::version() const { return "1.0.0"; }
const char* BrowserTool::description() const { return "HTTP browser and web content tool"; }
const char* BrowserTool::tool_id() const { return "browser"; }

std::vector<std::string> BrowserTool::actions() const {
    std::vector<std::string> acts;
    acts.push_back("fetch");
    acts.push_back("extract_text");
    acts.push_back("get_links");
    acts.push_back("status");
    return acts;
}

bool BrowserTool::init(const Config& cfg) {
    max_content_length_ = static_cast<size_t>(cfg.get_int("browser.max_content_length", 100000));
    timeout_secs_ = static_cast<int>(cfg.get_int("browser.timeout", 30));
    http_.set_timeout(timeout_secs_ * 1000);
    
    LOG_INFO("Browser: initialized (max_content=%zu, timeout=%ds)", 
             max_content_length_, timeout_secs_);
    initialized_ = true;
    return true;
}

void BrowserTool::shutdown() {
    initialized_ = false;
    LOG_INFO("Browser: shutdown");
}

ToolResult BrowserTool::execute(const std::string& action, const Json& params) {
    if (!initialized_) {
        return ToolResult::fail("Browser tool not initialized");
    }
    
    if (action == "fetch") {
        return do_fetch(params);
    } else if (action == "extract_text") {
        return do_extract_text(params);
    } else if (action == "get_links") {
        return do_get_links(params);
    } else if (action == "status") {
        return do_status();
    }
    
    return ToolResult::fail("Unknown action: " + action);
}

ToolResult BrowserTool::do_fetch(const Json& params) {
    std::string url = params["url"].as_string();
    if (url.empty()) {
        return ToolResult::fail("Missing required parameter: url");
    }
    
    LOG_DEBUG("Browser: fetching %s", url.c_str());
    
    HttpResponse resp = http_.get(url);
    if (!resp.ok()) {
        return ToolResult::fail("HTTP error: " + resp.error + 
                                " (status " + std::to_string(resp.status_code) + ")");
    }
    
    std::string content = resp.body;
    bool truncated = false;
    if (content.size() > max_content_length_) {
        content = content.substr(0, max_content_length_);
        truncated = true;
    }
    
    Json result = Json::object();
    result.set("url", url);
    result.set("status", Json(static_cast<int64_t>(resp.status_code)));
    result.set("content_length", Json(static_cast<int64_t>(resp.body.size())));
    result.set("truncated", truncated);
    result.set("content", content);
    
    LOG_DEBUG("Browser: fetched %zu bytes from %s", resp.body.size(), url.c_str());
    return ToolResult::ok(result);
}

ToolResult BrowserTool::do_extract_text(const Json& params) {
    std::string url = params["url"].as_string();
    std::string html = params["html"].as_string();
    
    if (!url.empty()) {
        HttpResponse resp = http_.get(url);
        if (!resp.ok()) {
            return ToolResult::fail("HTTP error: " + resp.error);
        }
        html = resp.body;
    }
    
    if (html.empty()) {
        return ToolResult::fail("Missing required parameter: url or html");
    }
    
    std::string text = strip_html(html);
    
    bool truncated = false;
    if (text.size() > max_content_length_) {
        text = text.substr(0, max_content_length_);
        truncated = true;
    }
    
    Json result = Json::object();
    result.set("text", text);
    result.set("text_length", Json(static_cast<int64_t>(text.size())));
    result.set("truncated", truncated);
    if (!url.empty()) {
        result.set("url", url);
    }
    
    return ToolResult::ok(result);
}

ToolResult BrowserTool::do_get_links(const Json& params) {
    std::string url = params["url"].as_string();
    std::string html = params["html"].as_string();
    
    if (!url.empty()) {
        HttpResponse resp = http_.get(url);
        if (!resp.ok()) {
            return ToolResult::fail("HTTP error: " + resp.error);
        }
        html = resp.body;
    }
    
    if (html.empty()) {
        return ToolResult::fail("Missing required parameter: url or html");
    }
    
    std::vector<std::pair<std::string, std::string> > links = extract_links(html, url);
    
    Json links_array = Json::array();
    for (size_t i = 0; i < links.size() && i < 100; ++i) {
        Json link = Json::object();
        link.set("href", links[i].first);
        link.set("text", links[i].second);
        links_array.push(link);
    }
    
    Json result = Json::object();
    result.set("links", links_array);
    result.set("count", Json(static_cast<int64_t>(links.size())));
    if (!url.empty()) {
        result.set("url", url);
    }
    
    return ToolResult::ok(result);
}

ToolResult BrowserTool::do_status() {
    Json result = Json::object();
    result.set("enabled", true);
    result.set("max_content_length", Json(static_cast<int64_t>(max_content_length_)));
    result.set("timeout_seconds", Json(static_cast<int64_t>(timeout_secs_)));
    return ToolResult::ok(result);
}

std::string BrowserTool::strip_html(const std::string& html) {
    std::string result;
    result.reserve(html.size());
    
    bool in_tag = false;
    bool in_script = false;
    bool in_style = false;
    size_t i = 0;
    
    while (i < html.size()) {
        char c = html[i];
        
        if (i + 7 < html.size()) {
            std::string tag7 = html.substr(i, 7);
            std::transform(tag7.begin(), tag7.end(), tag7.begin(), ::tolower);
            if (tag7 == "<script") in_script = true;
            if (tag7 == "<style>") in_style = true;
        }
        if (i + 8 < html.size()) {
            std::string tag8 = html.substr(i, 8);
            std::transform(tag8.begin(), tag8.end(), tag8.begin(), ::tolower);
            if (tag8 == "</script") in_script = false;
            if (tag8 == "</style>") in_style = false;
        }
        
        if (c == '<') {
            in_tag = true;
            if (i + 1 < html.size()) {
                char next = std::tolower(html[i + 1]);
                if (next == 'p' || next == 'd' || next == 'h' || 
                    next == 'l' || next == 'b' || next == 't') {
                    if (!result.empty() && result.back() != '\n' && result.back() != ' ') {
                        result += '\n';
                    }
                }
            }
        } else if (c == '>') {
            in_tag = false;
        } else if (!in_tag && !in_script && !in_style) {
            if (c == '&' && i + 1 < html.size()) {
                size_t end = html.find(';', i);
                if (end != std::string::npos && end - i < 10) {
                    std::string entity = html.substr(i, end - i + 1);
                    if (entity == "&nbsp;" || entity == "&#160;") {
                        result += ' ';
                    } else if (entity == "&lt;") {
                        result += '<';
                    } else if (entity == "&gt;") {
                        result += '>';
                    } else if (entity == "&amp;") {
                        result += '&';
                    } else if (entity == "&quot;") {
                        result += '"';
                    } else if (entity == "&#39;" || entity == "&apos;") {
                        result += '\'';
                    } else {
                        result += ' ';
                    }
                    i = end;
                } else {
                    result += c;
                }
            } else {
                result += c;
            }
        }
        ++i;
    }
    
    return normalize_whitespace(result);
}

std::string BrowserTool::normalize_whitespace(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    bool last_was_space = true;
    int newline_count = 0;
    
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\n' || c == '\r') {
            if (newline_count < 2) {
                result += '\n';
                newline_count++;
                last_was_space = true;
            }
        } else if (std::isspace(c)) {
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
            newline_count = 0;
        }
    }
    
    while (!result.empty() && std::isspace(result.back())) {
        result.pop_back();
    }
    
    return result;
}

std::vector<std::pair<std::string, std::string> > BrowserTool::extract_links(
        const std::string& html, const std::string& base_url) {
    std::vector<std::pair<std::string, std::string> > links;
    
    size_t pos = 0;
    while ((pos = html.find("<a ", pos)) != std::string::npos) {
        size_t end = html.find('>', pos);
        if (end == std::string::npos) break;
        
        size_t href_start = html.find("href=", pos);
        if (href_start != std::string::npos && href_start < end) {
            href_start += 5;
            char quote = html[href_start];
            if (quote == '"' || quote == '\'') {
                href_start++;
                size_t href_end = html.find(quote, href_start);
                if (href_end != std::string::npos && href_end < end + 100) {
                    std::string href = html.substr(href_start, href_end - href_start);
                    
                    size_t text_start = end + 1;
                    size_t text_end = html.find("</a>", text_start);
                    std::string text;
                    if (text_end != std::string::npos) {
                        text = strip_html(html.substr(text_start, text_end - text_start));
                    }
                    
                    if (!href.empty() && href[0] == '/') {
                        size_t scheme_end = base_url.find("://");
                        if (scheme_end != std::string::npos) {
                            size_t path_start = base_url.find('/', scheme_end + 3);
                            if (path_start != std::string::npos) {
                                href = base_url.substr(0, path_start) + href;
                            } else {
                                href = base_url + href;
                            }
                        }
                    }
                    
                    if (!href.empty() && href.find("javascript:") != 0 && href[0] != '#') {
                        links.push_back(std::make_pair(href, text));
                    }
                }
            }
        }
        pos = end + 1;
    }
    
    return links;
}

} // namespace openclaw

// Export plugin for dynamic loading
OPENCLAW_DECLARE_PLUGIN(openclaw::BrowserTool, "browser", "1.0.0", 
                        "HTTP browser and web content tool", "tool")
