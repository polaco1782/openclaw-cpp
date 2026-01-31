#include <openclaw/core/config.hpp>

namespace openclaw {

Config::Config() {}

bool Config::load_file(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    try {
        data_ = Json::parse(content);
        return true;
    } catch (...) {
        return false;
    }
}

bool Config::load_string(const std::string& json_str) {
    try {
        data_ = Json::parse(json_str);
        return true;
    } catch (...) {
        return false;
    }
}

std::string Config::get_string(const std::string& key, const std::string& def) const {
    if (data_.has(key)) {
        return data_[key].as_string(def);
    }
    std::string env_key = to_env_key(key);
    const char* env_val = std::getenv(env_key.c_str());
    return env_val ? env_val : def;
}

int64_t Config::get_int(const std::string& key, int64_t def) const {
    if (data_.has(key)) {
        return data_[key].as_int(def);
    }
    std::string env_key = to_env_key(key);
    const char* env_val = std::getenv(env_key.c_str());
    if (env_val) {
        return std::strtoll(env_val, NULL, 10);
    }
    return def;
}

bool Config::get_bool(const std::string& key, bool def) const {
    if (data_.has(key)) {
        return data_[key].as_bool(def);
    }
    std::string env_key = to_env_key(key);
    const char* env_val = std::getenv(env_key.c_str());
    if (env_val) {
        std::string val(env_val);
        return val == "1" || val == "true" || val == "yes";
    }
    return def;
}

const Json& Config::get_section(const std::string& key) const {
    return data_[key];
}

std::string Config::get_channel_string(const std::string& channel, 
                                        const std::string& key,
                                        const std::string& def) const {
    if (data_.has("channels")) {
        const Json& channels = data_["channels"];
        if (channels.has(channel)) {
            const Json& ch = channels[channel];
            if (ch.has(key)) {
                return ch[key].as_string(def);
            }
        }
    }
    std::string env_key = "OPENCLAW_" + to_upper(channel) + "_" + to_upper(key);
    const char* env_val = std::getenv(env_key.c_str());
    return env_val ? env_val : def;
}

const Json& Config::data() const { return data_; }

std::string Config::to_upper(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        result += c;
    }
    return result;
}

std::string Config::to_env_key(const std::string& key) {
    std::string result = "OPENCLAW_";
    for (size_t i = 0; i < key.size(); ++i) {
        char c = key[i];
        if (c == '.') c = '_';
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        result += c;
    }
    return result;
}

} // namespace openclaw
