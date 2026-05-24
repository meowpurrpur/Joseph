//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#include "Request.h"

#include <iostream>
#include <vector>

#include "Helpers.h"
#include "Response.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
#endif

namespace Joseph {
    std::string Request::getClientIP() const {
        if (response == nullptr) return "";

        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);

        if (getpeername(response->getClientFd(), reinterpret_cast<sockaddr *>(&addr), &addr_len) != 0)
            return "";

        char ipstr[INET6_ADDRSTRLEN];
        if (addr.ss_family == AF_INET) {
            const auto s = reinterpret_cast<sockaddr_in*>(&addr);
            inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
        } else if (addr.ss_family == AF_INET6) {
            const auto s = reinterpret_cast<sockaddr_in6*>(&addr);
            inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
        } else
            return "";

        return { ipstr };
    }

    std::unordered_map<std::string, std::string> RequestParser::parseQueryString(const std::string& query) {
        std::unordered_map<std::string, std::string> params;

        auto pairs = Helpers::split(query, '&');
        for (const auto& pair : pairs) {
            auto kv = Helpers::split(pair, '=');
            if (!kv.empty()) {
                std::string key = Helpers::urlDecode(kv[0]);
                std::string value = kv.size() > 1 ? Helpers::urlDecode(kv[1]) : "";
                params[key] = value;
            }
        }
        return params;
    }

    Method RequestParser::parseRequestMethod(const std::string& method) {
        if (method == "GET")
            return Method::GET;
        if (method == "POST")
            return Method::POST;
        if (method == "PUT")
            return Method::PUT;
        if (method == "DELETE")
            return static_cast<Method>(4); // weird workaround as including some windows header makes a macro called DELETE, and it fucks it up
        if (method == "HEAD")
            return Method::HEAD;
        if (method == "OPTIONS")
            return Method::OPTIONS;
        if (method == "TRACE")
            return Method::TRACE;
        if (method == "CONNECT")
            return Method::CONNECT;
        if (method == "PATCH")
            return Method::PATCH;

        return Method::UNKNOWN;
    }

    std::string RequestParser::methodToString(const Method method) {
        switch (method) {
            case GET:
                return "GET";
            case POST:
                return "POST";
            case PUT:
                return "PUT";
            case DELETE:
                return "DELETE";
            case HEAD:
                return "HEAD";
            case OPTIONS:
                return "OPTIONS";
            case TRACE:
                return "TRACE";
            case CONNECT:
                return "CONNECT";
            case PATCH:
                return "PATCH";
            default:
                return "UNKNOWN";
        }
    }

    Request RequestParser::parseRequest(const std::string& data) {
        auto lines = Helpers::split(data, "\r\n");
        if (lines.empty()) return {};

        const std::string& firstLine = lines[0];
        auto firstLineSplit = Helpers::split(firstLine, ' ');
        if (firstLineSplit.size() != 3) return {};

        const std::string& methodStr = firstLineSplit[0];
        const std::string& fullPath = firstLineSplit[1];
        const std::string& httpVersion = firstLineSplit[2];

        std::string path;
        std::unordered_map<std::string, std::string> queryParams;

        auto qPos = fullPath.find('?');
        if (qPos != std::string::npos) {
            path = fullPath.substr(0, qPos);
            std::string queryString = fullPath.substr(qPos + 1);
            queryParams = parseQueryString(queryString);
        } else {
            path = fullPath;
        }

        const Method method = parseRequestMethod(methodStr);
        if (method == UNKNOWN) return {};

        std::vector<std::string> headers;
        for (size_t i = 1; i < lines.size(); ++i) {
            std::string line = lines[i];

            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty())
                break;

            headers.push_back(line);
        }

        std::unordered_map<std::string, std::string> headerMap;
        for (const auto& header : headers) {
            auto parts = Helpers::split(header, ':');

            if (parts.size() >= 2) {
                const std::string& key = parts[0];
                std::string value = header.substr(key.size() + 1);

                if (!value.empty() && value[0] == ' ')
                    value.erase(0, 1);

                headerMap[key] = value;
            }
        }

        size_t contentLength = 0;
        auto it = headerMap.find("Content-Length");
        if (it != headerMap.end()) {
            contentLength = std::stoul(it->second);
        }

        const std::string delimiter = "\r\n\r\n";
        size_t bodyStart = data.find(delimiter);

        std::string body;
        if (bodyStart != std::string::npos && contentLength > 0) {
            bodyStart += delimiter.size();

            if (bodyStart + contentLength <= data.size())
                body = data.substr(bodyStart, contentLength);
        }

        return { method, path, body, KVList(headerMap), KVList(queryParams) };
    }
}
