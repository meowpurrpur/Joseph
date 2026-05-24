//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 27/04/2026.
//

#include "MultipartParser.h"
#include "Helpers.h"
#include <iostream>

#include "Request.h"

bool Joseph::MultipartParser::parse()
{
    const std::string& body = request->getBody();
    const KVPair* contentType = request->getHeader("Content-Type");

    if (!contentType)
        return false;

    const std::string& ct = contentType->getValue();

    if (ct.find("multipart/form-data") == std::string::npos)
        return false;

    auto boundaryPos = ct.find("boundary=");
    if (boundaryPos == std::string::npos)
        return false;

    std::string boundary = ct.substr(boundaryPos + 9);

    if (!boundary.empty() && boundary.front() == '"')
        boundary = boundary.substr(1, boundary.size() - 2);

    const std::string delimiter = "--" + boundary;
    const std::string closingDelimiter = delimiter + "--";

    std::vector<FormDataField> parseResult;
    size_t pos = 0;
    while (true)
    {
        size_t start = body.find(delimiter, pos);
        if (start == std::string::npos)
            break;

        start += delimiter.length();

        if (body.compare(start, 2, "--") == 0)
            break;

        if (body.compare(start, 2, "\r\n") == 0)
            start += 2;

        const size_t end = body.find(delimiter, start);
        if (end == std::string::npos)
            break;

        std::string part = body.substr(start, end - start);
        pos = end;

        const size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            continue;

        std::string headers = part.substr(0, headerEnd);
        std::string content = part.substr(headerEnd + 4);

        auto headersLines = Helpers::split(headers, "\r\n");
        std::vector<std::string> headersParsed;
        for (auto line : headersLines) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line.empty())
                break;

            headersParsed.push_back(line);
        }

        std::unordered_map<std::string, std::string> headerMap;
        for (const auto& headerString : headersParsed) {
            auto parts = Helpers::split(headerString, ':');

            if (parts.size() >= 2) {
                const std::string& key = parts[0];
                std::string value = headerString.substr(key.size() + 1);

                if (!value.empty() && value[0] == ' ')
                    value.erase(0, 1);

                headerMap[key] = value;
            }
        }

        while (!content.empty() && (content.back() == '\r' || content.back() == '\n')) {
            content.pop_back();
        }

        FormDataField field(KVList(headerMap), content);
        parseResult.push_back(field);
    }

    result = std::move(parseResult);
    return true;
}