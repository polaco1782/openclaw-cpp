# OpenClaw Control UI Plugin

## Overview

Web-based control interface for the OpenClaw C++ Gateway. Provides real-time monitoring and management through a browser.

## Features

- ✅ **Real-time Gateway Status** - Connection state, protocol version, clients
- ✅ **Health Monitoring** - Server health and port information  
- ✅ **Model Information** - Available AI models
- ✅ **Activity Log** - Real-time event logging
- ✅ **WebSocket Integration** - Live updates via gateway protocol
- ✅ **Dark Theme UI** - Modern, responsive design

## Architecture

The Control UI is embedded directly into the gateway plugin as a C++ string, eliminating the need for separate deployment or build steps.

### Components

1. **Embedded HTML/CSS/JS** (`control_ui.h`)
   - Single-page application
   - Zero external dependencies
   - Self-contained styling and logic

2. **HTTP Server** (libwebsockets)
   - Serves control UI on same port as WebSocket (18789)
   - HTTP GET / → Control UI
   - WS upgrade → Gateway protocol

3. **WebSocket Client** (JavaScript)
   - Connects to gateway WebSocket
   - Implements OpenClaw protocol
   - Real-time data updates

## Usage

### Access the Control UI

1. Start the gateway:
   ```bash
   ./bin/openclaw config.gateway.json
   ```

2. Open browser to:
   ```
   http://localhost:18789/
   ```

3. The UI will automatically:
   - Connect via WebSocket
   - Perform protocol handshake
   - Display gateway status
   - Refresh health data every 10 seconds

### URL

- **Local**: http://localhost:18789/
- **LAN**: http://<your-ip>:18789/ (if `gateway.bind` is set to "lan")

## Implementation

### Source Files

- `ui/index.html` - Control UI source (HTML/CSS/JS)
- `control_ui.h` - Generated C++ header with embedded UI
- `gateway.cpp` - HTTP serving logic in WebSocket callback

### HTTP Callback

The gateway handles HTTP requests in the WebSocket callback:

```cpp
case LWS_CALLBACK_HTTP:
    if (strcmp(requested, "/") == 0) {
        // Serve embedded HTML
        lws_write(wsi, CONTROL_UI_HTML, html_len, LWS_WRITE_HTTP);
    }
```

### Protocol Flow

```
Browser → HTTP GET / → Gateway (Crow HTTP)
       ← HTML Control UI ←

Browser → WS /ws → Gateway (Crow WebSocket)
       ← hello-ok ←

Browser → call health.get → Gateway
       ← result {...} ←
```

## UI Features

### Gateway Status Card
- Connection state (Connected/Disconnected)
- Protocol version
- Server version
- Connected clients count

### Health Card
- Status (ok/error)
- Port number

### Chat Interface
- Full duplex chat with all registered channel plugins
- Real-time message display
- Channel selector (Telegram/WhatsApp)
- Recipient input field

### Activity Log
- Real-time event logging
- Color-coded by severity (info/warn/error)
- Auto-scroll to latest

## Development

### Modifying the UI

1. Edit `src/plugins/gateway/ui/index.html`

2. Regenerate the C++ header:
   ```bash
   python3 /tmp/html2cpp.py src/plugins/gateway/ui/index.html > src/plugins/gateway/control_ui.h
   ```

3. Rebuild the gateway plugin:
   ```bash
   make bin/plugins/gateway.so
   ```

### Adding New Features

The UI uses vanilla JavaScript - no build step required. Add features by editing `index.html`:

- New protocol methods: Add to `callMethod()`
- New UI sections: Add HTML + update handlers
- Styling: Modify CSS variables in `:root`

## Comparison with TypeScript UI

| Feature | TypeScript UI | C++ Embedded UI |
|---------|--------------|----------------|
| Framework | Lit (Web Components) | Vanilla JS |
| Build | Vite + npm | None (embedded) |
| Size | ~2MB (dist/) | ~10KB (embedded) |
| Dependencies | Many npm packages | Zero |
| Deployment | Separate files | Single .so |
| Features | Full-featured | Core monitoring |

## Technical Details

### Embedded String Size

- HTML: 10,378 bytes
- C++ header: 338 lines
- Gzip equivalent: ~2KB

### Browser Compatibility

- Modern browsers (ES6+)
- WebSocket support required
- No polyfills needed

### Security

- Served on same port as WebSocket
- No CORS issues
- Auth token in WebSocket (not HTTP)
- Recommended: Use with `bind: "127.0.0.1"` for local-only access

## Future Enhancements

1. **Chat Interface** - Full chat UI with message history
2. **Configuration Editor** - Edit gateway config via UI
3. **Agent Management** - Start/stop agents, view logs
4. **Session Inspector** - View active sessions and history
5. **Real-time Metrics** - Charts for CPU, memory, request rates
6. **Log Filtering** - Filter logs by level, search
7. **Authentication UI** - Token/password entry form
8. **Multi-language** - i18n support

## Troubleshooting

### UI doesn't load
- Check gateway is running: `nc -z localhost 18789`
- Check logs for HTTP errors
- Try: http://127.0.0.1:18789/

### WebSocket won't connect
- Browser dev console will show WS errors
- Check if gateway auth is enabled
- Verify no firewall blocking port

### Styles broken
- Hard refresh: Ctrl+Shift+R
- Check browser console for errors

## Credits

- Based on OpenClaw TypeScript control UI
- Simplified for embedded C++ deployment
- Uses libwebsockets HTTP capabilities
