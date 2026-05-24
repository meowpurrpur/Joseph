//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_HEADERS_H
#define JOSEPH_HEADERS_H

#include <unordered_map>
#include <string>
#include <vector>
#include <cctype>

namespace Joseph {
    struct CaseInsensitiveHash {
        size_t operator()(const std::string& s) const noexcept {
            size_t hash = 0;
            for (unsigned char c : s) {
                hash = hash * 31 + std::tolower(c);
            }
            return hash;
        }
    };

    struct CaseInsensitiveEqual {
        bool operator()(const std::string& a,
                        const std::string& b) const noexcept {
            if (a.size() != b.size()) return false;

            for (size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }
    };

    class KVPair {
    private:
        std::string key;
        std::string value;

    public:
        KVPair(std::string key, std::string value)
            : key(std::move(key)), value(std::move(value)) {}

        [[nodiscard]] const std::string& getKey() const noexcept {
            return key;
        }

        [[nodiscard]] const std::string& getValue() const noexcept {
            return value;
        }

        void setValue(const std::string& newValue) {
            value = newValue;
        }
    };

    class KVList {
    private:
        std::unordered_map<
            std::string,
            KVPair,
            CaseInsensitiveHash,
            CaseInsensitiveEqual
        > internalList;

    public:
        KVList() = default;

        explicit KVList(const std::unordered_map<std::string, std::string>& map) {
            for (const auto& [k, v] : map) {
                internalList.emplace(k, KVPair{k, v});
            }
        }

        KVPair* get(const std::string& key) {
            auto it = internalList.find(key);
            return it != internalList.end() ? &it->second : nullptr;
        }

        const KVPair* get(const std::string& key) const {
            auto it = internalList.find(key);
            return it != internalList.end() ? &it->second : nullptr;
        }

        void set(const std::string& key, const std::string& value) {
            auto it = internalList.find(key);
            if (it != internalList.end()) {
                it->second.setValue(value);
            } else {
                internalList.emplace(key, KVPair{key, value});
            }
        }

        [[nodiscard]] std::vector<KVPair> getAll() const {
            std::vector<KVPair> all;
            all.reserve(internalList.size());

            for (const auto& [_, pair] : internalList) {
                all.push_back(pair);
            }

            return all;
        }
    };

} // namespace Joseph

#endif // JOSEPH_HEADERS_H