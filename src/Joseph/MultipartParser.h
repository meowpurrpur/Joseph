//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 27/04/2026.
//

#ifndef JOSEPH_MULTIPARTPARSER_H
#define JOSEPH_MULTIPARTPARSER_H

#include <utility>
#include <vector>

#include "KVList.h"

namespace Joseph
{
    class Request;

    class FormDataField
    {
    private:
        KVList headers;
        std::string content;
    public:
        FormDataField(KVList headers, std::string content) : headers(std::move(headers)), content(std::move(content)) {}

        const KVList& getHeaders() const { return headers; }
        const std::string& getContent() const { return content; }
    };

    class MultipartParser
    {
    private:
        std::vector<FormDataField> result;
        const Request* request;
    public:
        explicit MultipartParser(const Request* request) : request(request) {}

        [[nodiscard]] bool parse();
        std::vector<FormDataField> getResult() { return result; }
    };
}

#endif //JOSEPH_MULTIPARTPARSER_H
