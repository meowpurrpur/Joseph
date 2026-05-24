//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_HELPERS_H
#define JOSEPH_HELPERS_H

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

namespace Joseph::Helpers {
    inline std::string getMimeType(const std::string& path)
    {
        static const std::unordered_map<std::string_view, std::string_view> mimeTypes = {
            // HTML & text
            { ".html", "text/html" },
            { ".htm",  "text/html" },
            { ".css",  "text/css" },
            { ".txt",  "text/plain" },
            { ".csv",  "text/csv" },
            { ".xml",  "application/xml" },
            { ".json", "application/json" },
            { ".hpp",  "text/plain" },
            { ".h",    "text/plain" },
            { ".cpp",  "text/plain" },
            { ".cs",   "text/plain" },
            // JavaScript & WebAssembly
            { ".js",   "application/javascript" },
            { ".mjs",  "application/javascript" },
            { ".wasm", "application/wasm" },
            // Images
            { ".png",  "image/png" },
            { ".jpg",  "image/jpeg" },
            { ".jpeg", "image/jpeg" },
            { ".gif",  "image/gif" },
            { ".svg",  "image/svg+xml" },
            { ".webp", "image/webp" },
            { ".ico",  "image/x-icon" },
            { ".bmp",  "image/bmp" },
            // Fonts
            { ".woff",  "font/woff" },
            { ".woff2", "font/woff2" },
            { ".ttf",   "font/ttf" },
            { ".otf",   "font/otf" },
            // Audio
            { ".mp3", "audio/mpeg" },
            { ".wav", "audio/wav" },
            { ".ogg", "audio/ogg" },
            // Video
            { ".mp4",  "video/mp4" },
            { ".webm", "video/webm" },
            { ".ogv",  "video/ogg" },
            // Archives
            { ".zip", "application/zip" },
            { ".tar", "application/x-tar" },
            { ".gz",  "application/gzip" },
            { ".7z",  "application/x-7z-compressed" },
            { ".rar", "application/vnd.rar" },
            // PDFs & misc
            { ".pdf", "application/pdf" },
            { ".exe", "application/octet-stream" },
            { ".bin", "application/octet-stream" },
        };

        if (const auto dot = path.rfind('.'); dot != std::string::npos)
        {
            const std::string_view ext{ path.data() + dot, path.size() - dot };
            if (const auto it = mimeTypes.find(ext); it != mimeTypes.end())
                return std::string{ it->second };
        }

        return "application/octet-stream";
    }

    inline std::vector<std::string> splitPath(const std::string& path) {
        std::vector<std::string> parts;
        std::stringstream ss(path);
        std::string segment;
        while (std::getline(ss, segment, '/'))
            if (!segment.empty()) parts.push_back(segment);
        return parts;
    }

    inline std::string toLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return result;
    }

    inline bool matchRoute(const std::string& pattern, const std::string& path, std::unordered_map<std::string, std::string>& params)
    {
        std::string lowerPattern = toLower(pattern);

        std::vector<std::string> patternParts = splitPath(lowerPattern);
        std::vector<std::string> pathParts = splitPath(path); // keep original case here

        if (patternParts.size() != pathParts.size()) return false;

        for (size_t i = 0; i < patternParts.size(); ++i)
        {
            const auto& pp = patternParts[i];
            const auto& original = pathParts[i];

            if (pp.size() >= 2 && pp.front() == '{' && pp.back() == '}')
            {
                params[pp.substr(1, pp.size() - 2)] = original;
            }
            else
            {
                if (pp != toLower(original))
                    return false;
            }
        }

        return true;
    }

    inline std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            tokens.push_back(item);
        }
        return tokens;
    }

    inline std::vector<std::string> split(const std::string& s, const std::string& delimiter)
    {
        std::vector<std::string> tokens;
        size_t start = 0;

        while (true)
        {
            size_t pos = s.find(delimiter, start);
            if (pos == std::string::npos)
            {
                tokens.push_back(s.substr(start));
                break;
            }

            tokens.push_back(s.substr(start, pos - start));
            start = pos + delimiter.size();
        }

        return tokens;
    }

    inline std::string urlDecode(const std::string& str) {
        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%' && i + 2 < str.size()) {
                int value = 0;
                std::istringstream iss(str.substr(i + 1, 2));
                if (iss >> std::hex >> value) {
                    result.push_back(static_cast<char>(value));
                    i += 2;
                }
            } else if (str[i] == '+') {
                result.push_back(' ');
            } else {
                result.push_back(str[i]);
            }
        }
        return result;
    }

    inline std::string trim(const std::string& s) {
        const auto whitespace = " \t\r\n";
        const size_t start = s.find_first_not_of(whitespace);
        if (start == std::string::npos) return "";

        const size_t end = s.find_last_not_of(whitespace);
        return s.substr(start, end - start + 1);
    }

    inline std::string trimWhitespace(const std::string& s) {
        const size_t first = s.find_first_not_of(' ');
        if (first == std::string::npos) return "";

        const size_t last = s.find_last_not_of(' ');
        return s.substr(first, (last - first + 1));
    }

    inline std::string extractClientIPFromXFF(const std::string& xff) {
        if (xff.empty()) return "";

        const size_t comma = xff.find(',');
        const std::string first = (comma == std::string::npos)
            ? xff
            : xff.substr(0, comma);

        return trim(first);
    }

}

#endif // JOSEPH_HELPERS_H