#ifndef OPENCLAW_PLUGIN_PLUGIN_HPP
#define OPENCLAW_PLUGIN_PLUGIN_HPP

#include "../core/config.hpp"
#include <string>

namespace openclaw {

// Base plugin interface
class Plugin {
public:
    virtual ~Plugin() {}
    
    // Plugin metadata
    virtual const char* name() const = 0;
    virtual const char* version() const = 0;
    virtual const char* description() const { return ""; }
    
    // Lifecycle
    virtual bool init(const Config& cfg) = 0;
    virtual void shutdown() = 0;
    
    // Plugin state
    virtual bool is_initialized() const { return initialized_; }
    
protected:
    bool initialized_;
    
    Plugin() : initialized_(false) {}
};

} // namespace openclaw

#endif // OPENCLAW_PLUGIN_PLUGIN_HPP
