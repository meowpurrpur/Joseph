//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 23/04/2026.
//

#ifndef JOSEPH_CONFIG_H
#define JOSEPH_CONFIG_H

#include <cstring>

namespace Joseph {
    struct Config {
        size_t requestChunkSize = 4069;
        size_t responseChunkSize = 4096;
        size_t maxRequestSize = 8 * 1024 * 1024; // 8MB
        size_t maxRequestsPerConnection = 100;
        size_t timeout = 5;
        size_t maxThreads = 16;

        int port = 80;
        bool requestLogging = false;
        bool sendExceptions = false;
    };
}

#endif //JOSEPH_CONFIG_H
