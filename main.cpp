/**
 * @file main.cpp
 * @brief Main entry point for the high-performance cryptocurrency matching engine server.
 */

// We disable OpenSSL support in cpp-httplib to avoid the dependency on the OpenSSL library,
// as HTTPS is not required for this local server assignment.
// #define CPPHTTPLIB_OPENSSL_SUPPORT

#include <iostream>           // For logging to the console
#include <string>             // For using std::string
#include <vector>             // For std::vector
#include <stdexcept>          // For std::invalid_argument
#include <chrono>             // For std::chrono::seconds
#include <thread>             // For std::this_thread::sleep_for
#include <fstream>            // For reading the HTML file
#include <streambuf>          // For reading the HTML file

#include "httplib.h"          // The single-header HTTP server library
#include "json.hpp"           // The single-header JSON library for C++
#include "MatchingEngine.h"   // Our main engine class that orchestrates everything

// Create a convenient alias for the nlohmann::json type.
using json = nlohmann::json;

// --- Helper Functions: Data Validation and Conversion ---

/**
 * @brief Converts a string to a Side enum.
 * @param s The input string ("buy" or "sell").
 * @return The corresponding Side enum value.
 * @throws std::invalid_argument if the string is not a valid side.
 */
Side StringToSide(const std::string& s) {
    if (s == "buy") return Side::Buy;
    if (s == "sell") return Side::Sell;
    throw std::invalid_argument("Invalid side specified: '" + s + "'. Must be 'buy' or 'sell'.");
}

/**
 * @brief Converts a string to an OrderType enum.
 * @param s The input string ("market", "limit", "ioc", "fok").
 * @return The corresponding OrderType enum value.
 * @throws std::invalid_argument if the string is not a valid order type.
 */
OrderType StringToOrderType(const std::string& s) {
    if (s == "market") return OrderType::Market;
    if (s == "limit") return OrderType::Limit;
    if (s == "ioc") return OrderType::IOC;
    if (s == "fok") return OrderType::FOK;
    throw std::invalid_argument("Invalid order_type specified: '" + s + "'.");
}

// --- Main Server Application ---

int main() {
    // 1. Instantiate the Server and the Engine
    httplib::Server svr;
    MatchingEngine engine;

    // --- Read the HTML file into a string for serving ---
    std::string html_content;
    std::ifstream html_file("index.html");
    if (html_file) {
        // Read the entire file into the string
        html_content.assign((std::istreambuf_iterator<char>(html_file)),
                              std::istreambuf_iterator<char>());
        std::cout << "Successfully loaded index.html." << std::endl;
    } else {
        std::cerr << "Error: Could not open index.html. Serving a basic error message instead." << std::endl;
        html_content = "<h1>Error 500: index.html not found</h1><p>Please make sure index.html is in the same directory as the executable.</p>";
    }

    // --- Middleware to add CORS headers to all responses ---
    // This allows web pages from any origin (including your local file system) to make requests.
    svr.set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // --- Handler for OPTIONS requests (part of CORS "pre-flight" checks) ---
    svr.Options("/(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204; // "No Content" is the standard response
    });

    // --- Handler to serve the main HTML page ---
    svr.Get("/", [&html_content](const httplib::Request&, httplib::Response& res) {
        res.set_content(html_content, "text/html");
    });
     svr.Get("/index.html", [&html_content](const httplib::Request&, httplib::Response& res) {
        res.set_content(html_content, "text/html");
    });

    std::cout << "Starting matching engine server..." << std::endl;

    // --- Handler for the REST API endpoint: /order ---
    auto order_handler = [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::cout << "Received order request: " << j.dump(2) << std::endl;
            std::string symbol = j.at("symbol");
            OrderType type = StringToOrderType(j.at("order_type"));
            Side side = StringToSide(j.at("side"));
            double quantity = j.at("quantity");
            double price = j.value("price", 0.0);

            Order order(type, side, quantity, symbol, price);
            int order_id = order.getOrderID();
            engine.process(order); // The engine handles matching and broadcasting

            json response_json;
            response_json["status"] = "Order Received";
            response_json["order_id"] = order_id;
            res.set_content(response_json.dump(2), "application/json");

        } catch (const std::exception& e) {
            res.status = 400; // Bad Request
            json error_response;
            error_response["status"] = "Error";
            error_response["message"] = e.what();
            res.set_content(error_response.dump(2), "application/json");
            std::cerr << "Error processing request: " << e.what() << std::endl;
        }
    };

    svr.Post("/order", order_handler);
    svr.Post("/order/", order_handler);

    // --- Handlers for the real-time data feeds using Server-Sent Events (SSE) ---
    
    // Trade Feed Endpoint
    svr.Get("/ws/trades", [&](const httplib::Request&, httplib::Response& res) {
        res.set_chunked_content_provider("text/event-stream", 
            [&](size_t, httplib::DataSink& sink) {
                std::cout << "New client connected to trade feed." << std::endl;
                engine.add_trade_client(sink);
                while (sink.is_writable()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                std::cout << "Trade feed client disconnected." << std::endl;
                return true;
            }
        );
    });

    // Market Data Feed Endpoint
    svr.Get("/ws/marketdata", [&](const httplib::Request&, httplib::Response& res) {
        res.set_chunked_content_provider("text/event-stream", 
            [&](size_t, httplib::DataSink& sink) {
                std::cout << "New client connected to market data feed." << std::endl;
                engine.add_market_data_client(sink);
                while (sink.is_writable()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                std::cout << "Market data client disconnected." << std::endl;
                return true;
            }
        );
    });

    // --- Start the Server ---
    std::cout << "Server listening on http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}
