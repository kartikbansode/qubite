#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#endif
#include <iostream>
#include <memory>
#include <csignal>
#include "database/Database.h"
#include "order/OrderManager.h"
#include "server/HttpServer.h"

std::unique_ptr<HttpServer> gServer;

void signalHandler(int signum) {
    std::cout << "\n[Server] Interrupted by signal (" << signum << "). Shutting down gracefully...\n";
    if (gServer) {
        gServer->stop();
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string dbPath = "data/cafeteria.db";
    std::string webRoot = "frontend";
    std::string host = "0.0.0.0";
    int port = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
        } else if (arg == "--webroot" && i + 1 < argc) {
            webRoot = argv[++i];
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        }
    }

    std::cout << "[System] Initializing Cafe Token Management System...\n";

    // 1. Initialize SQLite Database
    auto db = std::make_shared<Database>(dbPath);
    if (!db->init()) {
        std::cerr << "[Fatal Error] Failed to initialize SQLite database at: " << dbPath << std::endl;
        return 1;
    }
    std::cout << "[System] SQLite database initialized successfully.\n";

    // 2. Initialize OrderManager and restore custom C++ data structures
    auto orderManager = std::make_shared<OrderManager>(db);
    if (!orderManager->init()) {
        std::cerr << "[Fatal Error] Failed to initialize OrderManager and reconstruct data structures.\n";
        return 1;
    }

    // 3. Initialize and Start HTTP Server
    gServer = std::make_unique<HttpServer>(orderManager, host, port, webRoot);
    if (!gServer->start()) {
        std::cerr << "[Fatal Error] Failed to bind/start HTTP server on " << host << ":" << port << std::endl;
        return 1;
    }

    std::cout << "[System] Server shutdown complete. Goodbye!\n";
    return 0;
}
