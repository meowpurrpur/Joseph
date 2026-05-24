//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#include "Server.h"

#include <csignal>
#include <iostream>
#include <fstream>
#include <set>
#include <thread>
#include <vector>

#include "Helpers.h"
#include "Request.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
    #include <netinet/tcp.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
#endif

std::mutex Joseph::Server::registryMutex;
std::vector<Joseph::Server*> Joseph::Server::registry;
std::atomic<bool> Joseph::Server::globalShutdownRequested{false};
std::once_flag signalHandlerOnce;

static void registerSignalHandlers() {
#ifdef _WIN32
    SetConsoleCtrlHandler(Joseph::Server::shutdownConsoleSignalHandler, TRUE);
    std::signal(SIGINT, SIG_IGN);
#else
    std::signal(SIGINT,  Joseph::Server::shutdownSignalHandler);
    std::signal(SIGTERM, Joseph::Server::shutdownSignalHandler);
    std::signal(SIGQUIT, Joseph::Server::shutdownSignalHandler);
    std::signal(SIGHUP,  Joseph::Server::shutdownSignalHandler);
#endif
}

Joseph::Route* findRoute(std::vector<Joseph::Route>& routes, const Joseph::Method method, const std::string& path) {
    for (auto& route : routes) {
        if (route.getMethod() != method) continue;

        std::unordered_map<std::string, std::string> params;
        if (Joseph::Helpers::matchRoute(route.getPath(), path, params))
            return &route;
    }
    return nullptr;
}

static bool sendAll(int fd, const std::string& data) {
    size_t total = 0;
    while (total < data.size()) {
        const int sent = send(fd, data.c_str() + total, data.size() - total, 0);
        if (sent <= 0) return false;
        total += sent;
    }
    return true;
}

static bool isWithinDirectory(const fs::path& root, const fs::path& target) {
    const fs::path canonicalRoot = fs::weakly_canonical(root);
    const fs::path canonicalTarget = fs::weakly_canonical(target);

    auto rootIt = canonicalRoot.begin();
    auto targetIt = canonicalTarget.begin();

    for (; rootIt != canonicalRoot.end(); ++rootIt, ++targetIt) {
        if (targetIt == canonicalTarget.end() || *rootIt != *targetIt)
            return false;
    }

    return true;
}

static std::string findHeader(const std::string& raw, const std::string& headerName) {
    std::string lowerRaw = Joseph::Helpers::toLower(raw);
    const std::string lowerName = Joseph::Helpers::toLower(headerName);

    const size_t pos = lowerRaw.find(lowerName + ": ");
    if (pos == std::string::npos) return "";

    const size_t valueStart = pos + headerName.size() + 2;
    const size_t valueEnd = lowerRaw.find("\r\n", valueStart);
    if (valueEnd == std::string::npos) return "";

    return raw.substr(valueStart, valueEnd - valueStart);
}

static bool isKeepAlive(const std::string& rawRequest) {
    const std::string conn = findHeader(rawRequest, "Connection");
    if (!conn.empty()) {
        const std::string lower = Joseph::Helpers::toLower(conn);
        return lower != "close";
    }

    return true;
}

std::string joinMethods(const std::set<Joseph::Method>& methods) {
    std::string result;

    for (auto it = methods.begin(); it != methods.end(); ++it) {
        if (it != methods.begin()) result += ", ";
        result += Joseph::RequestParser::methodToString(*it);
    }
    return result;
}

void handleOptions(const Joseph::Request& request, Joseph::Response& response, const std::vector<Joseph::Route>& routes) {
    std::set<Joseph::Method> allowed;

    for (const auto& route : routes) {
        std::unordered_map<std::string, std::string> params;

        if (Joseph::Helpers::matchRoute(route.getPath(), request.getPath(), params)) {
            allowed.insert(route.getMethod());

            if (route.getMethod() == Joseph::GET)
                allowed.insert(Joseph::HEAD);
        }
    }

    if (!allowed.empty())
        allowed.insert(Joseph::OPTIONS);

    if (allowed.empty()) {
        response.setStatus(404);
        return;
    }

    response.setStatus(204);
    response.setHeader("Allow", joinMethods(allowed));
    response.send("");
}

#ifdef _WIN32
BOOL WINAPI Joseph::Server::shutdownConsoleSignalHandler(DWORD) {
    requestGlobalShutdown();
    return TRUE;
}
#endif

void Joseph::Server::shutdownSignalHandler(int) {
    requestGlobalShutdown();
}

void Joseph::Server::handleClient(int client_fd) {
    activeConnections.fetch_add(1, std::memory_order_relaxed);
#ifdef _WIN32
    DWORD timeout = config.timeout * 1000;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    struct timeval timeout{};
    timeout.tv_sec = config.timeout;
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

    auto closeSocket = [&]() {
#ifdef _WIN32
        closesocket(client_fd);
#else
        close(client_fd);
#endif
    };

    auto sendErrorAndClose = [&](int status, const std::string& message) {
        requestLog(client_fd, "Sending error: \"" + message + "\" with status: " + std::to_string(status) + " and closing connection.");

        Response response;
        response.setStatus(status);
        response.send(message);
        sendAll(client_fd, response.toRawResponse());
        closeSocket();
    };

    requestLog(client_fd, "Connection opened");

    bool keepAlive = true;
    size_t requestsReceived = 0;
    std::string pendingData;
    pendingData.reserve(config.requestChunkSize);
    while (keepAlive && !stopRequested.load()) {
        char buffer[config.requestChunkSize];
        std::string rawRequest;

        while (true) {
            size_t headerEnd = pendingData.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                size_t requestLength = headerEnd + 4;
                std::string headerBlock = pendingData.substr(0, requestLength);
                std::string contentLengthVal = findHeader(headerBlock, "Content-Length");
                try {
                    if (!contentLengthVal.empty())
                        requestLength += std::stoul(contentLengthVal);
                } catch (const std::exception&) {
                    sendErrorAndClose(400, "Bad Request");
                    return;
                }

                if (requestLength > config.maxRequestSize) {
                    sendErrorAndClose(413, "Payload Too Large");
                    return;
                }

                if (pendingData.size() >= requestLength) {
                    rawRequest = pendingData.substr(0, requestLength);
                    pendingData.erase(0, requestLength);
                    break;
                }
            }

            int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);

            if (bytes_read < 0) {
#ifdef _WIN32
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT)
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
#endif
                {
                    sendErrorAndClose(408, "Request Timeout");
                    return;
                }

                requestLog(client_fd, "No bytes read, closing connection");
                closeSocket();
                return;
            }

            if (bytes_read == 0) {
                requestLog(client_fd, "No bytes read, closing connection");
                closeSocket();
                return;
            }

            pendingData.append(buffer, bytes_read);

            if (pendingData.size() > config.maxRequestSize) {
                sendErrorAndClose(413, "Payload Too Large");
                return;
            }
        }

        Request request;
        Response response;

        try { request = RequestParser::parseRequest(rawRequest); } catch (...) {}

        request.setRelatedResponse(&response);
        response.setRelatedRequest(&request);
        response.setClientFd(client_fd);

        requestLog(client_fd, "Got request:\n" + rawRequest + "\n");

        requestsReceived++;
        keepAlive = request.getIsValid() ? isKeepAlive(rawRequest) : false;

        if (requestsReceived >= config.maxRequestsPerConnection) {
            requestLog(client_fd, "Client has reached the maxRequestsPerConnection limit");
            keepAlive = false;
        }

        if (keepAlive) {
            requestLog(client_fd, "Connection should remain alive after this request");

            response.setHeader("Connection", "keep-alive");
            response.setHeader(
                "Keep-Alive",
                "timeout=" + std::to_string(config.timeout) + ", max=" + std::to_string(config.maxRequestsPerConnection)
            );
        } else {
            requestLog(client_fd, "Connection will close after this request");
            response.setHeader("Connection", "close");
        }

        if (request.getIsValid()) {
            Route* route = findRoute(routes, request.getMethod(), request.getPath());
            Route* routeHEAD = findRoute(routes, HEAD, request.getPath());
            Route* routeGET = findRoute(routes, GET, request.getPath());

            if (request.getMethod() == HEAD && !routeHEAD && routeGET) {
                request.markIsHEADFallback();
                route = routeGET;
            }

            if (request.getMethod() == OPTIONS)
                handleOptions(request, response, routes);

            if (route && !response.isFinalized()) {
                std::unordered_map<std::string, std::string> params;
                Helpers::matchRoute(route->getPath(), request.getPath(), params);
                request.setParams(KVList(params));

                try {
                    std::vector<MiddlewareFunc> chain;

                    chain.insert(chain.end(), globalMiddlewares.begin(), globalMiddlewares.end());
                    chain.insert(chain.end(), route->getMiddlewares().begin(), route->getMiddlewares().end());

                    size_t index = 0;
                    std::function<void()> next = [&]() {
                        if (index < chain.size()) {
                            auto& middleware = chain[index++];
                            middleware(request, response, next);
                        } else {
                            route->getHandler()(request, response);
                        }
                    };

                    next();

                } catch (std::exception& e) {
                    std::cerr << e.what() << std::endl;
                    response.setStatus(500);

                    if (config.sendExceptions)
                        response.send("Internal Server Error\nException details: " + std::string(e.what()));
                    else
                        response.send("Internal Server Error\nException details are hidden, set config.sendExceptions to true or check the output for exception details.");
                }

                if (request.getIsHEADFallback())
                    response.emptyBody();
            } else if (!response.isFinalized()) {
                bool fileFound = false;
                std::string requestPath = request.getPath();

                if (requestPath.find("..") == std::string::npos) {
                    for (const auto& dir : staticDirectories) {
                        fs::path filePath = dir / requestPath.substr(1);

                        if (fs::exists(filePath) && fs::is_regular_file(filePath) && isWithinDirectory(dir, filePath)) {
                            std::ifstream file(filePath, std::ios::binary);
                            if (file) {
                                std::string content((std::istreambuf_iterator<char>(file)),
                                                    std::istreambuf_iterator<char>());

                                response.setStatus(200);
                                response.setHeader("Content-Type", Helpers::getMimeType(filePath.string()));
                                response.send(content);
                                fileFound = true;
                                break;
                            }
                        }
                    }
                }

                if (!fileFound) {
                    response.setStatus(404);
                    response.send("Not Found");
                }
            }
        } else if (!response.isFinalized()) {
            response.setStatus(400);
            response.send("Bad Request");

            requestLog(client_fd, "Client sent unparsable request: " + rawRequest);
        }

        if (!response.hasBeenSent())
        {
            requestLog(client_fd, "Sending response:\n" + response.toRawResponse() + "\n");
            sendAll(client_fd, response.toRawResponse());
        } else
            requestLog(client_fd, "Response already been sent, likely streamed.");

        if (!keepAlive) break;
    }

    requestLog(client_fd, "Closing connection (loop exited)");
    closeSocket();
    activeConnections.fetch_sub(1, std::memory_order_relaxed);
}

void Joseph::Server::start() {
    if (started) throw std::runtime_error("Cannot start an already running server");
    std::call_once(signalHandlerOnce, registerSignalHandlers);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return;
    }
#endif

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        perror("bind");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        return;
    }

    if (::listen(server_fd, SOMAXCONN) < 0) {
        perror("listen");
#ifdef _WIN32
        closesocket(server_fd);
        WSACleanup();
#else
        close(server_fd);
#endif
        return;
    }

    threadPool = std::make_unique<ThreadPool>(config.maxThreads);
    started = true;
    std::cout << "[JOESEPH] Server listening on port " << config.port << " with " << config.maxThreads << " worker threads\n";

    while (!stopRequested.load()) {
        if (globalShutdownRequested.load()) {
            shutdown();
            break;
        }

        sockaddr_in client{};
#ifdef _WIN32
        int client_len = sizeof(client);
#else
        socklen_t client_len = sizeof(client);
#endif

        int client_fd = accept(server_fd, reinterpret_cast<sockaddr*>(&client), &client_len);
        if (client_fd < 0) {
            if (stopRequested.load())
                break;

            perror("accept");
            continue;
        }

#ifdef _WIN32
        int opt_nodelay = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt_nodelay), sizeof(opt_nodelay));
#else
        int opt_nodelay = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt_nodelay, sizeof(opt_nodelay));
#endif

        threadPool->submit([this, client_fd]() {
            handleClient(client_fd);
        });
    }

    std::cout << "[JOESEPH] Server attempting to shut down gracefully, stop requested..." << std::endl;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(config.timeout + 3);
    while (activeConnections.load() > 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (activeConnections.load() == 0)
        std::cout << "[JOSEPH] All requests finished, server stopping...\n";
    else
        std::cout << "[JOSEPH] Timeout reached, forcing stop (" << activeConnections.load() << " active connections)\n";

    stopCompleted.store(true, std::memory_order_relaxed);
    threadPool->shutdown();

#ifdef _WIN32
    WSACleanup();
#endif

    std::cout << "[JOSEPH] Server running on port " << config.port << " has been fully shutdown\n";
}

void Joseph::Server::startNonBlocking() {
    if (started) throw std::runtime_error("Cannot start an already running server");

    std::thread(&Server::start, this).detach();
}

void Joseph::Server::requestLog(int client, const std::string &message) const {
    if (!config.requestLogging) return;

    std::cout << "----------------------------------------------------------------------------\n";
    std::cout << "[CLIENT " << client << "] " << message << "\n";
}

void Joseph::Server::addRoute(const Method method, const std::string& path, const std::function<void(Request&, Response&)>& handler) {
    if (this->started) throw std::runtime_error("You cannot add routes after the server has started");

    const Route route{ method, Helpers::toLower(path), handler };
    routes.push_back(route);
}

void Joseph::Server::addRoute(const Method method, const std::string& path, const std::vector<MiddlewareFunc>& middleware, const RouteFunc& handler) {
    if (started) throw std::runtime_error("You cannot add routes after the server has started");

    const Route route{ method, Helpers::toLower(path), handler, middleware };
    routes.push_back(route);
}

void Joseph::Server::addMiddleware(const MiddlewareFunc& middleware) {
    if (started) throw std::runtime_error("You cannot add middleware after the server has started");

    globalMiddlewares.push_back(middleware);
}

void Joseph::Server::addStaticDirectory(const fs::path& directory) {
    if (!fs::is_directory(directory))
        throw std::invalid_argument("Invalid directory, please check the path");

    if (started)
        throw std::runtime_error("Cannot add static directory when server is running");

    staticDirectories.push_back(directory);
}

void Joseph::Server::shutdown() {
    if (!started || stopRequested.load()) return;
    stopRequested.store(true);

#ifdef _WIN32
    if (server_fd != -1) {
        closesocket(server_fd);
        server_fd = -1;
    }
#else
    close(server_fd);
#endif

    while (!stopCompleted.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Joseph::Server::requestGlobalShutdown() {
    if (bool expected = false; !globalShutdownRequested.compare_exchange_strong(expected, true))
        return;

    std::lock_guard<std::mutex> lock(registryMutex);
    for (Server* server : registry) {
        if (server)
            server->shutdown();
    }
}
