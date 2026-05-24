//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 30/04/2026.
//

#ifndef JOSEPH_RATELIMIT_H
#define JOSEPH_RATELIMIT_H

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "Helpers.h"
#include "Request.h"
#include "Route.h"

namespace Joseph {

    struct RatelimitData {
        int requestCount = 0;

        std::chrono::steady_clock::time_point windowStart;
        std::chrono::steady_clock::time_point lastSeen;

        RatelimitData()
            : requestCount(0),
              windowStart(std::chrono::steady_clock::now()),
              lastSeen(windowStart) {}
    };

    class Ratelimit {
    private:
        bool trustProxy = false;
        int maxRequests = 100;
        int timePeriod = 60; // seconds
        std::string ipHeader;

        struct State {
            std::unordered_map<std::string, RatelimitData> data;
            std::mutex mutex;
        };

        std::shared_ptr<State> state;
    public:
        Ratelimit() = default;

        Ratelimit(const int maxReq, const int periodSeconds, const bool trustProxy = false, std::string ipHeader = "X-Forwarded-For")
            : trustProxy(trustProxy),
              maxRequests(maxReq),
              timePeriod(periodSeconds),
              ipHeader(std::move(ipHeader)),
              state(std::make_shared<State>()) {}

        void operator()(const Request& req, Response& res, const NextFunc& next) {
            std::string key = req.getClientIP();
            if (key.empty()) {
                res.setStatus(400);
                res.send("Failed to identify client IP address");
                return;
            }

            if (trustProxy) {
                const auto ipFromHeader = req.getHeader(ipHeader);
                if (!ipFromHeader) {
                    res.setStatus(400);
                    res.send("Failed to identify client IP address");
                    return;
                }

                key = Helpers::extractClientIPFromXFF(ipFromHeader->getValue());
                if (key.empty()) {
                    res.setStatus(400);
                    res.send("Failed to identify client IP address");
                    return;
                }
            }

            if (!allowRequest(key)) {
                res.setStatus(429);
                res.send("Too Many Requests");
                return;
            }

            next();
        }

        bool allowRequest(const std::string& key) {
            const auto now = std::chrono::steady_clock::now();

            std::lock_guard<std::mutex> lock(state->mutex);
            auto& entry = state->data[key];

            entry.lastSeen = now;
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - entry.windowStart
            ).count();

            if (elapsed >= timePeriod) {
                entry.windowStart = now;
                entry.requestCount = 0;
            }

            if (entry.requestCount >= maxRequests) {
                return false;
            }

            entry.requestCount++;
            return true;
        }

        void cleanup(const int ttlSeconds) {
            const auto now = std::chrono::steady_clock::now();

            std::lock_guard<std::mutex> lock(state->mutex);
            for (auto it = state->data.begin(); it != state->data.end(); ) {
                const auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second.lastSeen
                ).count();

                if (age >= ttlSeconds) {
                    it = state->data.erase(it);
                } else {
                    ++it;
                }
            }
        }

        void setMaxRequests(const int value) { maxRequests = value; }
        void setTimePeriod(const int seconds) { timePeriod = seconds; }
        void setTrustProxy(const bool value) { trustProxy = value; }

        [[nodiscard]] int getMaxRequests() const { return maxRequests; }
        [[nodiscard]] int getTimePeriod() const { return timePeriod; }
        [[nodiscard]] bool getTrustProxy() const { return trustProxy; }
    };

} // namespace Joseph

#endif // JOSEPH_RATELIMIT_H