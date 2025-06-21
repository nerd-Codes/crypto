// --- START OF COMPLETE, CORRECTED, and COMMENTED OrderBook.h ---

/**
 * @file OrderBook.h
 * @brief Defines the OrderBook class, which stores and manages orders for a single trading symbol.
 *
 * This class is the heart of the matching engine's logic for one instrument (e.g., "BTC-USDT").
 * It maintains two sorted lists of orders (bids and asks) and implements the
 * price-time priority matching algorithm.
 */

#pragma once

#include <map>
#include <list>
#include <vector>
#include <iostream>
#include <algorithm> // For std::min
#include <string>    // For std::to_string

#include "Order.h"
#include "Trade.h"
#include "json.hpp"   // Defines the nlohmann::json type, which was missing.

// Create a convenient alias for the nlohmann::json type within this file's scope.
using json = nlohmann::json;

/**
 * @struct BBO
 * @brief A simple data structure to hold the Best Bid and Offer price.
 */
struct BBO {
    double bestBid;
    double bestAsk;
};

/**
 * @class OrderBook
 * @brief Manages the collection of buy (bid) and sell (ask) orders for a single asset.
 */
class OrderBook {
public:
    /**
     * @brief The main entry point for processing an order against this order book.
     * It orchestrates the matching logic based on the order's type and returns any
     * trades that were executed.
     * @param order The order to be processed. Passed by non-const reference because its
     *              quantity may be modified during matching.
     * @return A std::vector of Trade objects that resulted from this order.
     */
    std::vector<Trade> processOrder(Order& order) {
        std::vector<Trade> trades;

        if (order.getType() == OrderType::FOK) {
            if (!canFOKfill(order)) {
                return trades;
            }
        }

        if (order.getSide() == Side::Buy) {
            matchBuyOrder(order, trades);
        } else {
            matchSellOrder(order, trades);
        }

        if (order.getQuantity() > 0) {
            if (order.getType() == OrderType::Limit) {
                addLimitOrder(order);
            }
        }

        return trades;
    }

    /**
     * @brief Retrieves the Best Bid and Offer (BBO) from the book.
     * @param bbo_out A reference to a BBO struct that will be filled with the result.
     * @return True if a valid BBO (both a bid and an ask) exists, false otherwise.
     */
    bool getBBO(BBO& bbo_out) const {
        if (bids_.empty() || asks_.empty()) {
            return false;
        }
        bbo_out.bestBid = bids_.begin()->first;
        bbo_out.bestAsk = asks_.begin()->first;
        return true;
    }

    /**
     * @brief Prints a human-readable representation of the order book state to the console.
     * Used for debugging and demonstration.
     */
    void printOrderBook() const {
        using std::cout;
        using std::endl;

        cout << "--- ORDER BOOK ---" << endl;
        cout << "ASKS:" << endl;
        if (asks_.empty()) {
            cout << "  (empty)" << endl;
        } else {
            for (const auto& [price, orders_at_price] : asks_) {
                double total_quantity = 0;
                for (const auto& order : orders_at_price) {
                    total_quantity += order.getQuantity();
                }
                cout << "  Price: " << price << " | Total Quantity: " << total_quantity << " | Orders: " << orders_at_price.size() << endl;
            }
        }
        cout << "--------------------" << endl;
        cout << "BIDS:" << endl;
        if (bids_.empty()) {
            cout << "  (empty)" << endl;
        } else {
            for (const auto& [price, orders_at_price] : bids_) {
                double total_quantity = 0;
                for (const auto& order : orders_at_price) {
                    total_quantity += order.getQuantity();
                }
                cout << "  Price: " << price << " | Total Quantity: " << total_quantity << " | Orders: " << orders_at_price.size() << endl;
            }
        }
        cout << "--------------------" << endl << endl;
    }

    /**
     * @brief Gets the top N price levels of one side of the book.
     * @param n The number of levels to retrieve.
     * @param side The side of the book (Buy or Sell) to retrieve.
     * @return A json object representing the book depth, formatted as an array of [price_str, quantity_str] pairs.
     */
    json getBookDepthAsJson(int n, Side side) const {
        json depth_json = json::array();

        if (side == Side::Buy) {
            int count = 0;
            for (const auto& [price, orders_at_price] : bids_) {
                if (count++ >= n) break;
                double total_quantity = 0;
                for (const auto& order : orders_at_price) {
                    total_quantity += order.getQuantity();
                }
                depth_json.push_back({std::to_string(price), std::to_string(total_quantity)});
            }
        } else { // Side::Sell
            int count = 0;
            for (const auto& [price, orders_at_price] : asks_) {
                if (count++ >= n) break;
                double total_quantity = 0;
                for (const auto& order : orders_at_price) {
                    total_quantity += order.getQuantity();
                }
                depth_json.push_back({std::to_string(price), std::to_string(total_quantity)});
            }
        }
        
        return depth_json;
    }

private:
    std::map<double, std::list<Order>> asks_;
    std::map<double, std::list<Order>, std::greater<double>> bids_;

    void matchBuyOrder(Order& buyOrder, std::vector<Trade>& trades) {
        while (buyOrder.getQuantity() > 0 && !asks_.empty()) {
            auto bestAskIt = asks_.begin();
            std::list<Order>& askQueue = bestAskIt->second;
            Order& restingAsk = askQueue.front();

            if (buyOrder.getType() == OrderType::Limit && buyOrder.getPrice() < restingAsk.getPrice()) {
                break;
            }

            double tradeQuantity = std::min(buyOrder.getQuantity(), restingAsk.getQuantity());
            trades.emplace_back(restingAsk.getOrderID(), buyOrder.getOrderID(), restingAsk.getPrice(), tradeQuantity, Side::Buy, restingAsk.getSymbol());
            
            restingAsk.reduceQuantity(tradeQuantity);
            buyOrder.reduceQuantity(tradeQuantity);

            if (restingAsk.getQuantity() == 0) {
                askQueue.pop_front();
            }
            if (askQueue.empty()) {
                asks_.erase(bestAskIt);
            }
        }
    }

    void matchSellOrder(Order& sellOrder, std::vector<Trade>& trades) {
        while (sellOrder.getQuantity() > 0 && !bids_.empty()) {
            auto bestBidIt = bids_.begin();
            std::list<Order>& bidQueue = bestBidIt->second;
            Order& restingBid = bidQueue.front();

            if (sellOrder.getType() == OrderType::Limit && sellOrder.getPrice() > restingBid.getPrice()) {
                break;
            }

            double tradeQuantity = std::min(sellOrder.getQuantity(), restingBid.getQuantity());
            trades.emplace_back(restingBid.getOrderID(), sellOrder.getOrderID(), restingBid.getPrice(), tradeQuantity, Side::Sell, restingBid.getSymbol());
            
            restingBid.reduceQuantity(tradeQuantity);
            sellOrder.reduceQuantity(tradeQuantity);

            if (restingBid.getQuantity() == 0) {
                bidQueue.pop_front();
            }
            if (bidQueue.empty()) {
                bids_.erase(bestBidIt);
            }
        }
    }

    void addLimitOrder(const Order& order) {
        if (order.getSide() == Side::Buy) {
            bids_[order.getPrice()].push_back(order);
        } else {
            asks_[order.getPrice()].push_back(order);
        }
    }

    bool canFOKfill(const Order& order) const {
        double quantity_needed = order.getQuantity();
        double quantity_available = 0;

        if (order.getSide() == Side::Buy) {
            for (const auto& pair : asks_) {
                const double price_level = pair.first;
                if (order.getType() == OrderType::Limit && price_level > order.getPrice()) {
                    continue;
                }
                for (const auto& resting_order : pair.second) {
                    quantity_available += resting_order.getQuantity();
                }
                if (quantity_available >= quantity_needed) {
                    return true;
                }
            }
        } else {
            for (const auto& pair : bids_) {
                const double price_level = pair.first;
                if (order.getType() == OrderType::Limit && price_level < order.getPrice()) {
                    continue;
                }
                for (const auto& resting_order : pair.second) {
                    quantity_available += resting_order.getQuantity();
                }
                if (quantity_available >= quantity_needed) {
                    return true;
                }
            }
        }
        
        return quantity_available >= quantity_needed;
    }
};

// --- END OF COMPLETE, CORRECTED, and COMMENTED OrderBook.h ---