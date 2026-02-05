/*
 * OpenClaw C++11 - Personal AI Assistant Framework
 * 
 * A minimal, modular implementation with dynamic plugin loading support.
 * 
 * Usage:
 *   ./openclaw [config.json]
 * 
 * All configuration is read from config.json file.
 */
#include <openclaw/core/application.hpp>

int main(int argc, char* argv[]) {
    auto& app = openclaw::Application::instance();
    
    if (!app.init(argc, argv)) {
        // init returns false for --help/--version or fatal errors
        return app.is_running() ? 1 : 0;
    }
    
    int result = app.run();
    app.shutdown();
    
    return result;
}
