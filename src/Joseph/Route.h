//
// Copyright (c) 2026 T. B.
// Licensed under the MIT License.
// Created by theo on 21/04/2026.
//

#ifndef JOSEPH_ROUTE_H
#define JOSEPH_ROUTE_H

#include "Request.h"
#include "Response.h"

#include <functional>
#include <utility>

namespace Joseph {
    using NextFunc = std::function<void()>;
    using RouteFunc = std::function<void(Request&, Response&)>;
    using MiddlewareFunc = std::function<void(Request&, Response&, NextFunc)>;

    class Route {
    private:
        Method method = UNKNOWN;
        std::string path;
        RouteFunc handler;
        std::vector<MiddlewareFunc> middlewares;
    public:
        Route(const Method method, std::string path, RouteFunc handler) :
            method(method), path(std::move(path)), handler(std::move(handler)) {};

        Route(
            const Method method, std::string path,
            RouteFunc handler,
            std::vector<MiddlewareFunc> middlewares
        ) : method(method), path(std::move(path)), handler(std::move(handler)), middlewares(std::move(middlewares)) {};

        [[nodiscard]] Method getMethod() const { return method; }
        [[nodiscard]] const std::string& getPath() const { return path; };

        RouteFunc getHandler() { return handler; };
        const std::vector<MiddlewareFunc>& getMiddlewares() { return middlewares; };
    };
}

#endif //JOSEPH_ROUTE_H
