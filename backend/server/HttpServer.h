#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <memory>
#include "../order/OrderManager.h"
#include "../third_party/httplib.h"

class HttpServer {
private:
    std::shared_ptr<OrderManager> orderManager_;
    std::string host_;
    int port_;
    std::string webRoot_;
    httplib::Server svr_;

    void setupRoutes();
    void setupCors();
    bool authenticateRequest(const httplib::Request& req, DeviceRecord& outDevice);

public:
    HttpServer(std::shared_ptr<OrderManager> orderManager, const std::string& host = "0.0.0.0", int port = 8080, const std::string& webRoot = "./frontend");

    bool start();
    void stop();
};

#endif // HTTP_SERVER_H
