# Joseph

Joseph is a lightweight C++ HTTP web server designed to provide fast and efficient web services. This server includes built-in support for JSON, making it easy to handle JSON data in requests and responses. 

## Features
- **Lightweight**: Designed for performance and low resource usage.
- **JSON Support**: Easily parse and generate JSON data.
- **Request Routing**: Flexible routing options to manage incoming requests.
- **Static File Serving**: Serve static files seamlessly alongside dynamic content.
- **Middleware Support**: Global and route-specific middleware for request processing.
- **Multi-threaded**: Built-in thread pool for handling concurrent requests.

## Getting Started

To get started with Joseph, clone the repository and build the application using CMake:

```bash
git clone https://github.com/meowpurrpur/Joseph.git
cd joseph
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

Here's a basic example of creating a server with Joseph:

```cpp
#include "Joseph/Server.h"

int main() {
    Joseph::Config config;
    config.port = 8080;
    config.requestLogging = true;

    Joseph::Server server{config};
    server.addStaticDirectory("./static");

    // GET route
    server.addRoute(Joseph::Method::GET, "/", [](const Joseph::Request& req, Joseph::Response& res) {
        res.send("Hello, World!");
    });

    // POST route with JSON handling
    server.addRoute(Joseph::Method::POST, "/api", [](const Joseph::Request& req, Joseph::Response& res) {
        minjson::Object body = req.getBodyJson();
        // Process your JSON data here
        res.sendJson(minjson::Object{ {"success", true} });
    });

    // Route with parameters
    server.addRoute(Joseph::Method::GET, "/user/{id}", [](const Joseph::Request& req, Joseph::Response& res) {
        auto userId = req.getParam("id");
        res.sendJson(minjson::Object{ {"user_id", userId->getValue()} });
    });

    // Route with middleware
    server.addRoute(
        Joseph::Method::GET, 
        "/protected", 
       { 
            [](const Joseph::Request& req, Joseph::Response& res, const Joseph::NextFunc& next) {
                if (req.getQuery("token")) {
                    next();
                } else {
                    res.setStatus(401);
                    res.send("Unauthorized");
                }
            } 
        },
        [](Joseph::Request& req, Joseph::Response& res) {
            res.send("Protected content");
        }
    );

    // Global middleware
    server.addMiddleware([](const Joseph::Request& req, Joseph::Response& res, const Joseph::NextFunc& next) {
        std::cout << "Request received\n";
        next();
    });

    server.start();
    return 0;
}
```

Joseph makes it easy to build REST APIs and serve web content with minimal overhead.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
