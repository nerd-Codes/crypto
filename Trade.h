/**
 * @brief Defines the Trade struct, which represents an executed trade.
 *
 * This file contains the data structure used to record the details of a successful
 * match between two orders. It is a simple data aggregate.
 */

#pragma once

#include <string>
#include "Order.h" // We need access to the Side enum and to reference OrderIDs.

/**
 * @struct Trade
 * @brief Represents a single, atomic trade execution.
 *
 * A Trade object is created every time a part of an aggressive (taker) order is
 * matched with a resting (maker) order on the book. Using a 'struct' is appropriate
 * here as this object primarily serves to bundle public data together.
 */
struct Trade {
    // --- Member Variables ---

    int tradeID;        // A unique identifier for this specific trade execution.
    int makerOrderID;   // The unique ID of the order that was resting on the book (the liquidity provider).
    int takerOrderID;   // The unique ID of the incoming order that initiated the trade (the liquidity taker).
    
    double price;       // The price at which the trade was executed (always the price of the maker order).
    double quantity;    // The quantity of the asset that was exchanged in this trade.

    Side aggressorSide; // The side (Buy or Sell) of the taker order, indicating the direction of the aggression.

    // The trading symbol for which this trade occurred (e.g., "BTC-USDT").
    // This is crucial for multi-symbol engines.
    std::string symbol; 
    
    // A static member variable to ensure every trade across the entire engine
    // gets a unique, incrementing ID. It is shared by all Trade objects.
    static int next_trade_id;

    // --- Constructor ---

    /**
     * @brief Constructs a new Trade object.
     * This is called by the OrderBook whenever a match occurs.
     */
    Trade(int maker_id, int taker_id, double p, double q, Side aggressor, const std::string& sym) {
        // Use `this->` to be explicit that we are assigning to member variables.
        this->tradeID = next_trade_id++;
        this->makerOrderID = maker_id;
        this->takerOrderID = taker_id;
        this->price = p;
        this->quantity = q;
        this->aggressorSide = aggressor;
        this->symbol = sym;
    }
};

// NOTE: The definition for the static variable (e.g., `int Trade::next_trade_id = 1;`)
// must be placed in a corresponding .cpp file (Trade.cpp) to avoid linker errors.
