#ifndef OPENCLAW_CORE_JSON_HPP
#define OPENCLAW_CORE_JSON_HPP

// Minimal JSON implementation for C++11
// Supports: objects, arrays, strings, numbers, booleans, null

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cctype>

namespace openclaw {

class Json {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };
    
    // Constructors
    Json() : type_(NUL), bool_(false), number_(0) {}
    Json(bool b) : type_(BOOL), bool_(b), number_(0) {}
    Json(int n) : type_(NUMBER), bool_(false), number_(n) {}
    Json(int64_t n) : type_(NUMBER), bool_(false), number_(static_cast<double>(n)) {}
    Json(double n) : type_(NUMBER), bool_(false), number_(n) {}
    Json(const char* s) : type_(STRING), bool_(false), number_(0), string_(s) {}
    Json(const std::string& s) : type_(STRING), bool_(false), number_(0), string_(s) {}
    Json(std::vector<Json> arr) : type_(ARRAY), bool_(false), number_(0), array_(arr) {}
    Json(std::map<std::string, Json> obj) : type_(OBJECT), bool_(false), number_(0), object_(obj) {}
    
    // Type checks
    Type type() const;
    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;
    
    // Value accessors
    bool as_bool(bool def = false) const;
    double as_number(double def = 0) const;
    int64_t as_int(int64_t def = 0) const;
    std::string as_string(const std::string& def = "") const;
    const std::vector<Json>& as_array() const;
    const std::map<std::string, Json>& as_object() const;
    
    // Object access
    const Json& operator[](const std::string& key) const;
    const Json& operator[](size_t idx) const;
    bool has(const std::string& key) const;
    size_t size() const;
    
    // Modifiers
    void set(const std::string& key, const Json& value);
    void push(const Json& value);
    void set_type(Type t);
    
    // Helper getters with defaults
    std::string get_string(const std::string& key, const std::string& def = "") const;
    int get_int(const std::string& key, int def = 0) const;
    double get_double(const std::string& key, double def = 0.0) const;
    bool get_bool(const std::string& key, bool def = false) const;
    
    // Static constructors
    static Json object();
    static Json array();
    
    // Serialization
    std::string dump() const;
    
    // Parsing
    static Json parse(const std::string& str);

private:
    Type type_;
    bool bool_;
    double number_;
    std::string string_;
    std::vector<Json> array_;
    std::map<std::string, Json> object_;
    
    void dump_impl(std::ostringstream& ss) const;
    static void escape_string(std::ostringstream& ss, const std::string& s);
    static void skip_ws(const std::string& s, size_t& pos);
    static Json parse_value(const std::string& s, size_t& pos);
    static Json parse_null(const std::string& s, size_t& pos);
    static Json parse_bool(const std::string& s, size_t& pos);
    static Json parse_number(const std::string& s, size_t& pos);
    static Json parse_string(const std::string& s, size_t& pos);
    static Json parse_array(const std::string& s, size_t& pos);
    static Json parse_object(const std::string& s, size_t& pos);
};

} // namespace openclaw

#endif // OPENCLAW_CORE_JSON_HPP
