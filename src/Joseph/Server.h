//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_SERVER_H
#define JOSEPH_SERVER_H
#include <functional>
#include <filesystem>

#include "Config.h"
#include "Request.h"
#include "Route.h"
#include "ThreadPool.h"

#ifdef _WIN32
#include <windef.h>
#endif

namespace fs = std::filesystem;

namespace Joseph {
    using NextFunc = std::function<void()>;
    using RouteFunc = std::function<void(Request&, Response&)>;
    using MiddlewareFunc = std::function<void(Request&, Response&, NextFunc)>;

    class Server {
    private:
        std::vector<Route> routes;
        std::vector<MiddlewareFunc> globalMiddlewares;
        std::vector<fs::path> staticDirectories;
        std::unique_ptr<ThreadPool> threadPool;

        bool started = false;

        int server_fd;
        std::atomic<bool> stopRequested{false};
        std::atomic<bool> stopCompleted{false};
        std::atomic<int> activeConnections{0};

        static std::mutex registryMutex;
        static std::vector<Server*> registry;
        static std::atomic<bool> globalShutdownRequested;

        void handleClient(int client_fd);
        void requestLog(int client, const std::string& message) const;

        Config config;
    public:
        Server(const Config& config) : config(config), server_fd(-1) {
            std::lock_guard lock(registryMutex);
            registry.push_back(this);
        }
        ~Server() {
            std::lock_guard lock(registryMutex);
            std::erase(registry, this);
        }

#ifdef _WIN32
        static BOOL WINAPI shutdownConsoleSignalHandler(DWORD signal);
#endif
        static void shutdownSignalHandler(int signal);

        void addRoute(Method method, const std::string& path, const RouteFunc& handler);
        void addRoute(Method method, const std::string& path, const std::vector<MiddlewareFunc>& middleware, const RouteFunc& handler);

        void addMiddleware(const MiddlewareFunc& middleware);
        void addStaticDirectory(const fs::path& directory);

        [[nodiscard]] Config getConfig() const { return config; }
        void start();
        void startNonBlocking();

        void shutdown();
        static void requestGlobalShutdown();
    };
}

#endif //JOSEPH_SERVER_H
