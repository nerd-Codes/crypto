
/**
 * @brief Defines the main MatchingEngine class, which orchestrates all trading activity.
 *
 * The MatchingEngine acts as the central controller. Its primary responsibilities are:
 * 1. Managing a collection of OrderBook objects, one for each trading symbol.
 * 2. Receiving new orders and routing them to the correct OrderBook.
 * 3. Triggering the broadcast of real-time data (trades, market data) to subscribed clients
 *    after an order has been processed.
 * 4. Handling thread-safe access to the list of connected WebSocket/SSE clients.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>          // For std::mutex and std::lock_guard for thread safety
#include "OrderBook.h"    // The core logic for a single symbol's order book
#include "json.hpp"       // For creating JSON messages for broadcasting
#include "httplib.h"      // For the httplib::DataSink type definition

// Create a convenient alias for the nlohmann::json type.
using json = nlohmann::json;

class MatchingEngine {
private:
    // A map from a symbol string (e.g., "BTC-USDT") to its dedicated OrderBook object.
    // std::unordered_map is used for fast O(1) average-case lookup of the correct book.
    std::unordered_map<std::string, OrderBook> order_books_;

    // --- WebSocket/SSE Broadcasting Members ---
    
    // A mutex is essential for thread safety. The cpp-httplib server processes each
    // request in a separate thread. Therefore, adding a new client connection (from one thread)
    // and broadcasting a message (from another thread) could happen simultaneously.
    // The mutex ensures that the client lists are not modified while being iterated over.
    std::mutex mtx_;

    // A list of pointers to the active client connections (DataSinks) for the trade feed.
    std::vector<httplib::DataSink*> trade_sinks_;
    // A list of pointers to the active client connections for the market data feed.
    std::vector<httplib::DataSink*> market_data_sinks_;

public:
    void process(Order& order) {
        const std::string& symbol = order.getSymbol();
        
        // Access the order book for the given symbol. If it doesn't exist,
        // the [] operator of unordered_map conveniently creates a new, empty OrderBook.
        OrderBook& book = order_books_[symbol];

        // --- State Change Detection ---
        // To decide whether to broadcast a market data update, we must check if the
        // visible state of the book has changed. We capture the top 10 levels of the
        // book *before* the order is processed.
        json old_asks_depth = book.getBookDepthAsJson(10, Side::Sell);
        json old_bids_depth = book.getBookDepthAsJson(10, Side::Buy);

        // The core logic: process the order against the book and get resulting trades.
        std::vector<Trade> trades = book.processOrder(order);

        // --- Broadcast Events After Processing ---
        
        // 1. Broadcast trades if they occurred.
        // This is always done when a trade happens.
        if (!trades.empty()) {
            broadcast_trades(trades);
        }

        // 2. Broadcast market data if the book's visible state has changed.
        // We capture the new state of the book's depth.
        json new_asks_depth = book.getBookDepthAsJson(10, Side::Sell);
        json new_bids_depth = book.getBookDepthAsJson(10, Side::Buy);

        // We compare the string representations of the JSON to see if anything changed.
        // This robustly catches changes in price, quantity, or the number of levels.
        if (old_asks_depth.dump() != new_asks_depth.dump() || old_bids_depth.dump() != new_bids_depth.dump()) {
            broadcast_market_data(symbol, book);
        }
    }

    // --- Client Connection Management ---

    /** @brief Adds a new client connection to the trade feed subscription list. */
    void add_trade_client(httplib::DataSink& sink) {
        std::lock_guard<std::mutex> lock(mtx_); // Automatically lock and unlock the mutex.
        trade_sinks_.push_back(&sink);
    }

    /** @brief Adds a new client connection to the market data feed subscription list. */
    void add_market_data_client(httplib::DataSink& sink) {
        std::lock_guard<std::mutex> lock(mtx_);
        market_data_sinks_.push_back(&sink);
    }

private:
    // --- Private Broadcasting Helper Functions ---

    /** @brief Serializes and broadcasts a list of trades to all trade subscribers using SSE format. */
    void broadcast_trades(const std::vector<Trade>& trades) {
        std::lock_guard<std::mutex> lock(mtx_);

        for (const auto& trade : trades) {
            // --- THIS PART WAS MISSING IN THE PREVIOUS SNIPPET ---
            json trade_json;
            trade_json["type"] = "trade";
            trade_json["trade_id"] = trade.tradeID;
            trade_json["symbol"] = trade.symbol;
            trade_json["price"] = trade.price;
            trade_json["quantity"] = trade.quantity;
            trade_json["aggressor_side"] = (trade.aggressorSide == Side::Buy) ? "buy" : "sell";
            trade_json["maker_order_id"] = trade.makerOrderID;
            trade_json["taker_order_id"] = trade.takerOrderID;
            // --- END OF MISSING PART ---
            
            // Format the message for Server-Sent Events (SSE)
            std::string sse_msg = "data: " + trade_json.dump() + "\n\n";

            std::cout << "[BROADCAST] Sending trade message: " << sse_msg; // Use one endl for tidiness
            for (auto sink : trade_sinks_) {
                sink->write(sse_msg.c_str(), sse_msg.length());
            }
        }
    }

    /** @brief Serializes and broadcasts the current market state to all market data subscribers using SSE format. */
    void broadcast_market_data(const std::string& symbol, const OrderBook& book) {
        std::lock_guard<std::mutex> lock(mtx_);

        // --- THIS PART WAS MISSING IN THE PREVIOUS SNIPPET ---
        json data_json;
        data_json["type"] = "l2update";
        data_json["symbol"] = symbol;
        
        BBO bbo;
        if (book.getBBO(bbo)) {
            data_json["best_bid"] = bbo.bestBid;
            data_json["best_ask"] = bbo.bestAsk;
        } else {
            data_json["best_bid"] = nullptr;
            data_json["best_ask"] = nullptr;
        }
        
        data_json["bids"] = book.getBookDepthAsJson(10, Side::Buy);
        data_json["asks"] = book.getBookDepthAsJson(10, Side::Sell);
        // --- END OF MISSING PART ---

        // Format the message for Server-Sent Events (SSE)
        std::string sse_msg = "data: " + data_json.dump() + "\n\n";

        std::cout << "[BROADCAST] Sending market data: " << sse_msg;
        for (auto sink : market_data_sinks_) {
            sink->write(sse_msg.c_str(), sse_msg.length());
        }
    }
};
