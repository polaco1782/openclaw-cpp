#include <openclaw/core/json.hpp>

namespace openclaw {

// Type checks
Json::Type Json::type() const { return type_; }
bool Json::is_null() const { return type_ == NUL; }
bool Json::is_bool() const { return type_ == BOOL; }
bool Json::is_number() const { return type_ == NUMBER; }
bool Json::is_string() const { return type_ == STRING; }
bool Json::is_array() const { return type_ == ARRAY; }
bool Json::is_object() const { return type_ == OBJECT; }

// Value accessors
bool Json::as_bool(bool def) const { 
    return type_ == BOOL ? bool_ : def; 
}

double Json::as_number(double def) const { 
    return type_ == NUMBER ? number_ : def; 
}

int64_t Json::as_int(int64_t def) const { 
    return type_ == NUMBER ? static_cast<int64_t>(number_) : def; 
}

std::string Json::as_string(const std::string& def) const { 
    return type_ == STRING ? string_ : def; 
}

const std::vector<Json>& Json::as_array() const {
    static std::vector<Json> empty;
    return type_ == ARRAY ? array_ : empty;
}

const std::map<std::string, Json>& Json::as_object() const {
    static std::map<std::string, Json> empty;
    return type_ == OBJECT ? object_ : empty;
}

// Object access
const Json& Json::operator[](const std::string& key) const {
    static Json null_json;
    if (type_ != OBJECT) return null_json;
    std::map<std::string, Json>::const_iterator it = object_.find(key);
    return it != object_.end() ? it->second : null_json;
}

const Json& Json::operator[](size_t idx) const {
    static Json null_json;
    if (type_ != ARRAY || idx >= array_.size()) return null_json;
    return array_[idx];
}

bool Json::has(const std::string& key) const {
    return type_ == OBJECT && object_.find(key) != object_.end();
}

size_t Json::size() const {
    if (type_ == ARRAY) return array_.size();
    if (type_ == OBJECT) return object_.size();
    return 0;
}

// Modifiers
void Json::set(const std::string& key, const Json& value) {
    if (type_ != OBJECT) {
        type_ = OBJECT;
        object_.clear();
    }
    object_[key] = value;
}

void Json::push(const Json& value) {
    if (type_ != ARRAY) {
        type_ = ARRAY;
        array_.clear();
    }
    array_.push_back(value);
}

void Json::set_type(Type t) {
    type_ = t;
    if (t == ARRAY) {
        array_.clear();
    } else if (t == OBJECT) {
        object_.clear();
    }
}

// Helper getters with defaults
std::string Json::get_string(const std::string& key, const std::string& def) const {
    if (type_ != OBJECT) return def;
    std::map<std::string, Json>::const_iterator it = object_.find(key);
    if (it != object_.end() && it->second.is_string()) {
        return it->second.as_string();
    }
    return def;
}

int Json::get_int(const std::string& key, int def) const {
    if (type_ != OBJECT) return def;
    std::map<std::string, Json>::const_iterator it = object_.find(key);
    if (it != object_.end() && it->second.is_number()) {
        return static_cast<int>(it->second.as_number());
    }
    return def;
}

double Json::get_double(const std::string& key, double def) const {
    if (type_ != OBJECT) return def;
    std::map<std::string, Json>::const_iterator it = object_.find(key);
    if (it != object_.end() && it->second.is_number()) {
        return it->second.as_number();
    }
    return def;
}

bool Json::get_bool(const std::string& key, bool def) const {
    if (type_ != OBJECT) return def;
    std::map<std::string, Json>::const_iterator it = object_.find(key);
    if (it != object_.end() && it->second.is_bool()) {
        return it->second.as_bool();
    }
    return def;
}

// Static constructors
Json Json::object() {
    Json j;
    j.type_ = OBJECT;
    return j;
}

Json Json::array() {
    Json j;
    j.type_ = ARRAY;
    return j;
}

// Serialization
std::string Json::dump() const {
    std::ostringstream ss;
    dump_impl(ss);
    return ss.str();
}

// Parsing
Json Json::parse(const std::string& str) {
    size_t pos = 0;
    return parse_value(str, pos);
}

void Json::dump_impl(std::ostringstream& ss) const {
    switch (type_) {
        case NUL:
            ss << "null";
            break;
        case BOOL:
            ss << (bool_ ? "true" : "false");
            break;
        case NUMBER: {
            int64_t i = static_cast<int64_t>(number_);
            if (number_ == static_cast<double>(i)) {
                ss << i;
            } else {
                ss << number_;
            }
            break;
        }
        case STRING:
            ss << '"';
            escape_string(ss, string_);
            ss << '"';
            break;
        case ARRAY:
            ss << '[';
            for (size_t i = 0; i < array_.size(); ++i) {
                if (i > 0) ss << ',';
                array_[i].dump_impl(ss);
            }
            ss << ']';
            break;
        case OBJECT: {
            ss << '{';
            bool first = true;
            for (std::map<std::string, Json>::const_iterator it = object_.begin();
                 it != object_.end(); ++it) {
                if (!first) ss << ',';
                first = false;
                ss << '"';
                escape_string(ss, it->first);
                ss << "\":";
                it->second.dump_impl(ss);
            }
            ss << '}';
            break;
        }
    }
}

void Json::escape_string(std::ostringstream& ss, const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    ss << buf;
                } else {
                    ss << c;
                }
        }
    }
}

void Json::skip_ws(const std::string& s, size_t& pos) {
    while (pos < s.size() && std::isspace(s[pos])) pos++;
}

Json Json::parse_value(const std::string& s, size_t& pos) {
    skip_ws(s, pos);
    if (pos >= s.size()) return Json();
    
    char c = s[pos];
    if (c == 'n') return parse_null(s, pos);
    if (c == 't' || c == 'f') return parse_bool(s, pos);
    if (c == '"') return parse_string(s, pos);
    if (c == '[') return parse_array(s, pos);
    if (c == '{') return parse_object(s, pos);
    if (c == '-' || std::isdigit(c)) return parse_number(s, pos);
    
    throw std::runtime_error("Invalid JSON at position " + std::to_string(pos));
}

Json Json::parse_null(const std::string& s, size_t& pos) {
    if (s.substr(pos, 4) == "null") {
        pos += 4;
        return Json();
    }
    throw std::runtime_error("Expected 'null'");
}

Json Json::parse_bool(const std::string& s, size_t& pos) {
    if (s.substr(pos, 4) == "true") {
        pos += 4;
        return Json(true);
    }
    if (s.substr(pos, 5) == "false") {
        pos += 5;
        return Json(false);
    }
    throw std::runtime_error("Expected boolean");
}

Json Json::parse_number(const std::string& s, size_t& pos) {
    size_t start = pos;
    if (s[pos] == '-') pos++;
    while (pos < s.size() && std::isdigit(s[pos])) pos++;
    if (pos < s.size() && s[pos] == '.') {
        pos++;
        while (pos < s.size() && std::isdigit(s[pos])) pos++;
    }
    if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
        pos++;
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
        while (pos < s.size() && std::isdigit(s[pos])) pos++;
    }
    return Json(std::strtod(s.substr(start, pos - start).c_str(), NULL));
}

Json Json::parse_string(const std::string& s, size_t& pos) {
    pos++; // skip opening quote
    std::string result;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            pos++;
            switch (s[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (pos + 4 < s.size()) {
                        int code = std::strtol(s.substr(pos + 1, 4).c_str(), NULL, 16);
                        result += static_cast<char>(code & 0xFF);
                        pos += 4;
                    }
                    break;
                }
                default: result += s[pos];
            }
        } else {
            result += s[pos];
        }
        pos++;
    }
    pos++; // skip closing quote
    return Json(result);
}

Json Json::parse_array(const std::string& s, size_t& pos) {
    pos++; // skip [
    std::vector<Json> arr;
    skip_ws(s, pos);
    if (pos < s.size() && s[pos] == ']') {
        pos++;
        return Json(arr);
    }
    while (pos < s.size()) {
        arr.push_back(parse_value(s, pos));
        skip_ws(s, pos);
        if (pos >= s.size()) break;
        if (s[pos] == ']') {
            pos++;
            break;
        }
        if (s[pos] == ',') pos++;
        skip_ws(s, pos);
    }
    return Json(arr);
}

Json Json::parse_object(const std::string& s, size_t& pos) {
    pos++; // skip {
    std::map<std::string, Json> obj;
    skip_ws(s, pos);
    if (pos < s.size() && s[pos] == '}') {
        pos++;
        return Json(obj);
    }
    while (pos < s.size()) {
        skip_ws(s, pos);
        if (pos >= s.size() || s[pos] != '"') break;
        Json key = parse_string(s, pos);
        skip_ws(s, pos);
        if (pos >= s.size() || s[pos] != ':') break;
        pos++; // skip :
        obj[key.as_string()] = parse_value(s, pos);
        skip_ws(s, pos);
        if (pos >= s.size()) break;
        if (s[pos] == '}') {
            pos++;
            break;
        }
        if (s[pos] == ',') pos++;
    }
    return Json(obj);
}

} // namespace openclaw
