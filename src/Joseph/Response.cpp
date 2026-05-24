//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#include "Response.h"
#include <ranges>
#include <string>
#include <fstream>
#include <iostream>
#include <cctype>

#include "Request.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <netinet/tcp.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#include "Helpers.h"

namespace HeaderHelpers
{
    bool sendRawHeaders(const int fd, const char* data, size_t length) {
        size_t total = 0;
        while (total < length) {
            const int sent = ::send(fd, data + total, length - total, 0);
            if (sent <= 0)
                return false;

            total += sent;
        }

        return true;
    }

    bool containsNewlines(const std::string& value) {
        return value.find('\r') != std::string::npos || value.find('\n') != std::string::npos;
    }

    void validateHeaderName(const std::string& header) {
        if (header.empty())
            throw std::runtime_error("Header name cannot be empty");

        for (const unsigned char c : header) {
            if (!std::isprint(c) || c == ':' || c == '\r' || c == '\n')
                throw std::runtime_error("Header name contains invalid characters");
        }
    }

    void validateHeaderValue(const std::string& value) {
        if (containsNewlines(value))
            throw std::runtime_error("Header value contains invalid characters");
    }

    void validateCookieToken(const std::string& value, const std::string& field) {
        if (value.empty())
            throw std::runtime_error(field + " cannot be empty");

        if (containsNewlines(value))
            throw std::runtime_error(field + " contains invalid characters");
    }
}

std::string http_date()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};

#if defined(_WIN32)
    gmtime_s(&tm, &t);   // Windows
#else
    gmtime_r(&t, &tm);   // POSIX
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

std::string reason_phrase(const int status)
{
    switch (status)
    {
        // 1xx Informational
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing";
        case 103: return "Early Hints";

        // 2xx Successful
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 208: return "Already Reported";
        case 226: return "IM Used";

        // 3xx Redirection
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";

        // 4xx Client Error
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Content Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";
        case 421: return "Misdirected Request";
        case 422: return "Unprocessable Content";
        case 423: return "Locked";
        case 424: return "Failed Dependency";
        case 425: return "Too Early";
        case 426: return "Upgrade Required";
        case 428: return "Precondition Required";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 451: return "Unavailable For Legal Reasons";

        // 5xx Server Error
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 506: return "Variant Also Negotiates";
        case 507: return "Insufficient Storage";
        case 508: return "Loop Detected";
        case 510: return "Not Extended";
        case 511: return "Network Authentication Required";

        default:  return "Unknown";
    }
}

Joseph::Response::Response() {
    headers.set("Server", "Joseph");
    headers.set("X-Powered-By", "Joseph");
    headers.set("Content-Type", "text/html; charset=utf-8");
    headers.set("Date", http_date());
    headers.set("Content-Length", std::to_string(rawBody.length()));
}

std::string Joseph::Response::toRawResponse() const
{
    std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + reason_phrase(statusCode) + "\r\n";

    for (const auto &header: headers.getAll()) {
        response += header.getKey() + ": " + header.getValue() + "\r\n";
    }

    for (const auto &cookieHeader : setCookieHeaders)
    {
        response += "Set-Cookie: " + cookieHeader + "\r\n";
    }

    response += "\r\n";
    response += rawBody;
    return response;
}

void Joseph::Response::finalizedCheck() const {
    if (finalized)
        throw std::runtime_error("Response has already been finalized");
}

bool Joseph::Response::shouldStream(size_t fileSize) const {
    constexpr size_t minStreamSize = 1024 * 1024;
    return client_fd != -1 && fileSize > minStreamSize;
}

void Joseph::Response::send(const std::string& content) {
    finalizedCheck();
    finalized = true;

    rawBody = content;
    headers.set("Content-Length", std::to_string(rawBody.length()));
}

void Joseph::Response::setHeader(const std::string& header, const std::string& value) {
    finalizedCheck();
    HeaderHelpers::validateHeaderName(header);
    HeaderHelpers::validateHeaderValue(value);
    headers.set(header, value);
}

void Joseph::Response::sendFile(const fs::path& path, bool shouldDownload, const std::string& fileName) {
    finalizedCheck();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        setStatus(500);
        send("Error: Failed to open file for writing");

        std::cerr << "Error: Failed to open \"" << path << "\" for writing\n";
        return;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string realFileName = path.filename().string();
    if (!fileName.empty()) realFileName = fileName;
    HeaderHelpers::validateHeaderValue(realFileName);

    std::string mimeType = Helpers::getMimeType(path.filename().string());
    setHeader("Content-Type", mimeType);

    if (shouldDownload)
        setHeader("Content-Disposition", "attachment; filename=\"" + realFileName + "\"");
    else
        setHeader("Content-Disposition", "inline; filename=\"" + realFileName + "\"");

    bool isHEADFallback = false;
    if (request)
        isHEADFallback = request->getIsHEADFallback();

    if (shouldStream(fileSize) && !isHEADFallback) {
        setHeader("Content-Length", std::to_string(fileSize));

        std::string headerResponse = "HTTP/1.1 " + std::to_string(statusCode) + " " + reason_phrase(statusCode) + "\r\n";
        for (const auto& header : headers.getAll()) {
            headerResponse += header.getKey() + ": " + header.getValue() + "\r\n";
        }
        for (const auto& cookieHeader : setCookieHeaders) {
            headerResponse += "Set-Cookie: " + cookieHeader + "\r\n";
        }
        headerResponse += "\r\n";

        if (!HeaderHelpers::sendRawHeaders(client_fd, headerResponse.c_str(), headerResponse.length()))
            throw std::runtime_error("Failed to send file response headers");

        hasSent = true;

        constexpr size_t chunkSize = 65536;
        char buffer[chunkSize];
        while (file.read(buffer, chunkSize) || file.gcount() > 0) {
            if (!HeaderHelpers::sendRawHeaders(client_fd, buffer, file.gcount()))
                throw std::runtime_error("Failed to stream file response");
        }
    } else {
        file.seekg(0, std::ios::end);
        file.seekg(0, std::ios::beg);

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        setHeader("Content-Length", std::to_string(content.length()));
        send(content);
    }

    file.close();
    finalized = true;
}

void Joseph::Response::sendJson(const minjson::Value& json) {
    finalizedCheck();
    setHeader("Content-Type", "application/json; charset=utf-8");

    minjson::SerializationOptions options;
    options.indent = 0;
    options.objectKeyValueSeparator = ": ";

    const auto serialized = minjson::serializeToString(json, options);
    send(serialized);
}

void Joseph::Response::setStatus(const int code) {
    finalizedCheck();
    if (code > 599 || code < 100)
        throw std::runtime_error("Status codes must be between 100 and 599");

    statusCode = code;
}

void Joseph::Response::redirect(const std::string &url, bool perm) {
    finalizedCheck();
    HeaderHelpers::validateHeaderValue(url);
    finalized = true;

    statusCode = perm ? 301 : 302;
    headers.set("Location", url);
}

void Joseph::Response::setCookie(
    const std::string& name,
    const std::string& value,
    const CookieOptions& opts
) {
    finalizedCheck();
    HeaderHelpers::validateCookieToken(name, "Cookie name");
    HeaderHelpers::validateCookieToken(value, "Cookie value");

    std::ostringstream cookie;
    cookie << name << "=" << value;

    if (!opts.domain.empty()) {
        HeaderHelpers::validateHeaderValue(opts.domain);
        cookie << "; Domain=" << opts.domain;
    }
    if (!opts.path.empty()) {
        HeaderHelpers::validateHeaderValue(opts.path);
        cookie << "; Path=" << opts.path;
    }


    if (opts.expires) {
        std::tm gm{};

#ifdef _WIN32
        gmtime_s(&gm, opts.expires);
#else
        gmtime_r(opts.expires, &gm);
#endif

        std::ostringstream date;
        date << std::put_time(&gm, "%a, %d %b %Y %H:%M:%S GMT");
        cookie << "; Expires=" << date.str();
    }


    if (opts.maxAge >= 0) cookie << "; Max-Age=" << opts.maxAge;
    if (opts.secure) cookie << "; Secure";
    if (opts.httpOnly) cookie << "; HttpOnly";
    if (!opts.sameSite.empty()) {
        HeaderHelpers::validateHeaderValue(opts.sameSite);
        cookie << "; SameSite=" << opts.sameSite;
    }
    if (opts.partitioned) cookie << "; Partitioned";

    setCookieHeaders.push_back(cookie.str());
}