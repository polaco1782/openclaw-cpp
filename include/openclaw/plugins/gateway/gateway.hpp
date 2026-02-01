#ifndef OPENCLAW_PLUGINS_GATEWAY_GATEWAY_HPP
#define OPENCLAW_PLUGINS_GATEWAY_GATEWAY_HPP

#include "../../core/plugin.hpp"
#include "../../core/types.hpp"
#include "../../core/json.hpp"
#include <string>
#include <map>
#include <vector>
#include <set>

namespace openclaw {

// Forward declarations
class WebSocketServer;
class GatewayClient;

// Gateway Server Plugin
// Implements a WebSocket-based gateway for remote agent control
// Compatible with OpenClaw gateway protocol
class GatewayPlugin : public Plugin {
public:
    GatewayPlugin();
    virtual ~GatewayPlugin();
    
    // Plugin interface
    virtual const char* name() const { return "gateway"; }
    virtual const char* version() const { return "1.0.0"; }
    virtual const char* description() const { 
        return "WebSocket gateway server for remote agent control"; 
    }
    
    virtual bool init(const Config& cfg);
    virtual void shutdown();
    virtual void poll();  // Poll WebSocket server
    
    // Receive all incoming messages for routing to gateway clients
    virtual void on_incoming_message(const Message& msg);
    
    // Gateway operations
    bool start(int port = 18789);
    void stop();
    bool is_running() const { return running_; }
    
    // Get server status
    int port() const { return port_; }
    size_t client_count() const;
    
    // Broadcast events to all connected clients
    void broadcast(const std::string& event, const Json& payload);
    
    // Message routing - called by channel plugins to broadcast messages to gateway
    void route_incoming_message(const Message& msg);
    
    // Protocol handlers (public for WebSocketServer callback)
    void handle_client_connect(GatewayClient* client);
    void handle_client_disconnect(GatewayClient* client);
    void handle_client_message(GatewayClient* client, const std::string& msg);
    
private:
    bool running_;
    int port_;
    std::string bind_host_;
    std::string auth_token_;
    
    // WebSocket server (implementation in .cpp)
    WebSocketServer* ws_server_;
    
    // Connected clients
    std::set<GatewayClient*> clients_;
    
    // Gateway protocol methods
    Json handle_hello(GatewayClient* client, const Json& params);
    Json handle_chat_send(GatewayClient* client, const Json& params);
    Json handle_config_get(GatewayClient* client, const Json& params);
    Json handle_health_get(GatewayClient* client, const Json& params);
    Json handle_models_list(GatewayClient* client, const Json& params);
};

// Client connection state
class GatewayClient {
public:
    GatewayClient(void* ws_connection, const std::string& conn_id);
    ~GatewayClient();
    
    const std::string& conn_id() const { return conn_id_; }
    bool is_authenticated() const { return authenticated_; }
    void set_authenticated(bool auth) { authenticated_ = auth; }
    
    // Send message to client
    bool send(const std::string& message);
    bool send_json(const Json& data);
    
    // Protocol info
    int protocol_version() const { return protocol_version_; }
    void set_protocol_version(int ver) { protocol_version_ = ver; }
    
    const std::string& client_id() const { return client_id_; }
    void set_client_id(const std::string& id) { client_id_ = id; }
    
private:
    void* ws_connection_;  // Opaque WebSocket connection handle
    std::string conn_id_;
    std::string client_id_;
    bool authenticated_;
    int protocol_version_;
};

} // namespace openclaw

// Plugin factory function
extern "C" {
    openclaw::Plugin* create_plugin();
}

#endif // OPENCLAW_PLUGINS_GATEWAY_GATEWAY_HPP
