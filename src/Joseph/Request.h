//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_REQUEST_H
#define JOSEPH_REQUEST_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

#include "Helpers.h"
#include "KVList.h"
#include "MultipartParser.h"
#include "../../external/minjson.hpp"

namespace Joseph {
    class Response;

    enum Method {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE,
        PATCH,
        UNKNOWN = -1
    };

    class Request
    {
    private:
        Response* response{};

        bool isValid = true;
        bool isHEADFallback = false;

        Method method = UNKNOWN;
        std::string body;
        std::string path;

        KVList headers;
        KVList query;
        KVList params;
        KVList cookies;
    public:
        Request(
            const Method method,
            std::string path,
            std::string body,
            KVList headers,
            KVList query
        ) :
        method(method), body(std::move(body)), path(std::move(path)),
        headers(std::move(headers)), query(std::move(query))
        {
            cookies = KVList();
            if (KVPair* cookieHeader = this->headers.get("Cookie"))
            {
                std::stringstream ss(cookieHeader->getValue());
                std::string item;

                while (std::getline(ss, item, ';')) {
                    if (size_t sep = item.find('='); sep != std::string::npos) {
                        std::string key = Helpers::trim(item.substr(0, sep));
                        std::string value = Helpers::trim(item.substr(sep + 1));

                        cookies.set(key, value);
                    }
                }
            }
        }

        void setRelatedResponse(Response* _response) { response = _response; }

        Request() : isValid(false) {};
        bool getIsValid() const { return isValid; };
        bool getIsHEADFallback() const { return isHEADFallback; };

        Method getMethod() const { return method; };
        std::string getPath() const { return path; };
        std::string getClientIP() const;

        std::vector<KVPair> getHeaders() const { return headers.getAll(); };
        std::vector<KVPair> getQueryList() const { return query.getAll(); };
        std::vector<KVPair> getParams() const { return params.getAll(); };
        std::vector<KVPair> getCookies() const { return cookies.getAll(); };

        void setParams(const KVList& _params)
        {
            if (params.getAll().empty())
                params = _params;
        };
        void markIsHEADFallback() { isHEADFallback = true; };

        std::string getBody() const { return body; }

        minjson::Object getBodyJson() const {
            auto result = minjson::parse(getBody());
            if (result.status != minjson::ParsingResultStatus::Success)
                throw std::runtime_error("Failed to parse JSON");

            return result.value.asObject();
        }

        std::vector<FormDataField> getFormData() const {
            MultipartParser parser(this);
            if (bool success = parser.parse(); !success)
                throw std::runtime_error("Failed to parse form data");

            return parser.getResult();
        }

        const KVPair* getHeader(const std::string &name) const {
            return headers.get(name);
        };

        const KVPair* getQuery(const std::string &name) const {
            return query.get(name);
        };

        const KVPair* getParam(const std::string &name) const {
            return params.get(name);
        };

        const KVPair* getCookie(const std::string& name) const
        {
            return cookies.get(name);
        }
    };

    namespace RequestParser {
        Request parseRequest(const std::string& data);
        Method parseRequestMethod(const std::string& method);
        std::string methodToString(Method method);
        std::unordered_map<std::string, std::string> parseQueryString(const std::string& query);
    }
}

#endif //JOSEPH_REQUEST_H
