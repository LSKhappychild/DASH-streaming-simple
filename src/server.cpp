#include "civetweb.h"
#include <iostream>
#include <string.h>

int main() {
    // Server options
    const char *options[] = {
        "document_root", "/home/sklee/DASH-streaming-simple/asset/MPD",
        "listening_ports", "8080",
        "enable_directory_listing", "no",
        NULL
    };

    // Callback functions (not used here)
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    // Start the server
    struct mg_context *ctx = mg_start(&callbacks, NULL, options);

    if (ctx == NULL) {
        std::cerr << "Failed to start server." << std::endl;
        return -1;
    }

    std::cout << "Server started on port 8080." << std::endl;
    std::cout << "Serving content from ./dash_content directory." << std::endl;
    std::cout << "Press Enter to stop the server." << std::endl;

    // Wait until user hits Enter
    getchar();

    // Stop the server
    mg_stop(ctx);

    return 0;
}
