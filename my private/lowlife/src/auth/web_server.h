#pragma once

namespace web_server {
    // Start the background Winsock server thread listening on 127.0.0.1:9876
    bool start();

    // Clean up resources and stop the background server
    void stop();
}
