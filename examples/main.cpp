#include <Joseph/Ratelimit.h>
#include <Joseph/Server.h>

void middlewareTest(const Joseph::Request& req, Joseph::Response& res, const Joseph::NextFunc& next) {
    if (req.getQuery("middleware")) {
        res.send("Request intercepted by route-specific middleware.");
        return;
    }
    next();
}

int main() {
    Joseph::Config config;
    config.requestLogging = true;
    config.sendExceptions = true;
    config.port = 8080;

    Joseph::Server server{config};

    server.addMiddleware([](const Joseph::Request& req, Joseph::Response& res, const Joseph::NextFunc& next) {
        if (req.getQuery("globalmiddleware")) {
            res.send("Request intercepted by global middleware.");
            return;
        }
        next();
    });

    server.addRoute(Joseph::Method::GET, "/", [](const Joseph::Request& req, Joseph::Response& res) {
        res.setHeader("X-Status", "OK");
        res.send("Service is running.");
    });

    server.addRoute(Joseph::Method::GET, "/redirect", [](const Joseph::Request& req, Joseph::Response& res) {
        res.redirect("https://google.com/ai");
    });

    server.addRoute(Joseph::Method::GET, "/redirect-permanent", [](const Joseph::Request& req, Joseph::Response& res) {
        res.redirect("https://google.com/ai", true);
    });

    server.addRoute(Joseph::Method::GET, "/parameters/{first}/{second}/details", [](const Joseph::Request& req, Joseph::Response& res) {
        minjson::Object output;
        for (const auto& param : req.getParams()) {
            output.emplace(param.getKey(), param.getValue());
        }
        res.sendJson(output);
    });

    server.addRoute(Joseph::Method::GET, "/query", [](const Joseph::Request& req, Joseph::Response& res) {
        minjson::Object output;
        for (const auto& query : req.getQueryList()) {
            output.emplace(query.getKey(), query.getValue());
        }
        res.sendJson(output);
    });

    server.addRoute(Joseph::Method::GET, "/cookies", [](const Joseph::Request& req, Joseph::Response& res) {
        minjson::Object output;
        for (const auto& cookie : req.getCookies()) {
            output.emplace(cookie.getKey(), cookie.getValue());
        }
        res.sendJson(output);
    });

    server.addRoute(Joseph::Method::GET, "/set-cookie", [](const Joseph::Request& req, Joseph::Response& res) {
        res.setCookie("session_id", "example_session_value");
        res.setCookie("preferences", "default");
        res.send("Cookies have been set successfully.");
    });

    server.addRoute(
        Joseph::Method::GET,
        "/middleware",
        { middlewareTest },
        [](const Joseph::Request& req, Joseph::Response& res) {
            res.send("Request processed successfully after middleware.");
        }
    );

    server.addRoute(
        Joseph::Method::GET,
        "/rate-limit",
        { Joseph::Ratelimit(5, 30) },
        [](const Joseph::Request& req, Joseph::Response& res) {
            res.send("Request accepted. Your IP: " + req.getClientIP());
        }
    );

    server.addRoute(Joseph::Method::POST, "/", [](const Joseph::Request& req, Joseph::Response& res) {
        auto body = req.getBodyJson();

        if (!body.contains("test")) {
            res.sendJson(minjson::Object{
                { "success", false },
                { "error", "Missing required field: test" }
            });
            return;
        }

        res.sendJson(minjson::Object{
            { "success", true },
            { "message", "Request processed successfully" },
            { "test", body["test"].asString() }
        });
    });

    server.addRoute(Joseph::Method::POST, "/multipart", [](const Joseph::Request& req, Joseph::Response& res) {
        const auto formData = req.getFormData();
        minjson::Array response;

        for (const auto& part : formData) {
            minjson::Array entry;

            entry.emplace_back(part.getContent());

            minjson::Object headers;
            for (const auto& header : part.getHeaders().getAll()) {
                headers.emplace(header.getKey(), header.getValue());
            }

            entry.emplace_back(headers);
            response.emplace_back(entry);
        }

        res.sendJson(response);
    });

    server.addRoute(Joseph::Method::GET, "/headers", [](const Joseph::Request& req, Joseph::Response& res) {
        minjson::Object output;

        for (const auto& header : req.getHeaders()) {
            output.emplace(header.getKey(), header.getValue());
        }

        res.sendJson(output);
    });

    server.start();
    return 0;
}