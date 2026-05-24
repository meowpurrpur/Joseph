//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_RESPONSE_H
#define JOSEPH_RESPONSE_H

#include <utility>
#include "KVList.h"
#include <filesystem>
#include "../../external/minjson.hpp"

namespace fs = std::filesystem;
namespace Joseph {
    class Request;

    struct CookieOptions {
        std::string domain{};
        std::string path{"/"};
        const std::time_t* expires{nullptr};
        int maxAge{-1};
        bool secure{false};
        bool httpOnly{false};
        std::string sameSite{}; // "Lax", "Strict", "None"
        bool partitioned{false};
    };

    class Response {
    private:
        Request* request{};

        int client_fd = -1;
        int statusCode = 200;

        std::string rawBody;
        KVList headers;
        std::vector<std::string> setCookieHeaders;

        bool finalized = false;
        bool hasSent = false;

        void finalizedCheck() const;
        bool shouldStream(size_t fileSize) const;
    public:
        Response();
        Response(const int statusCode, KVList headers, std::string rawBody) :
        statusCode(statusCode), headers(std::move(headers)), rawBody(std::move(rawBody)) {};

        void setRelatedRequest(Request* _request) { request = _request; }

        bool hasBeenSent() const { return hasSent; }
        bool isFinalized() const { return finalized; }

        std::string toRawResponse() const;
        std::vector<KVPair> getHeaders() const { return headers.getAll(); };

        void setCookie(const std::string& name, const std::string& value, const CookieOptions& opts = {});
        void setClientFd(const int fd)
        {
            if (client_fd != -1)
                throw std::runtime_error("Client has already been set");

            client_fd = fd;
        }
        int getClientFd() const { return client_fd; };

        void setStatus(int code);
        void redirect(const std::string& url, bool perm = false);

        void send(const std::string& content);
        void sendFile(const fs::path& path, bool shouldDownload = false, const std::string& fileName = "");
        void sendJson(const minjson::Value &json);

        void emptyBody() { rawBody = ""; };

        KVPair* getHeader(const std::string& header) {
            return headers.get(header);
        };

        void setHeader(const std::string& header, const std::string& value);
    };
}


#endif //JOSEPH_RESPONSE_H
