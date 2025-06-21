

/**
 * @brief Defines the core Order class and related enumerations for the matching engine.
 *
 * This file contains the fundamental data structures that represent a single trading order
 * and its various properties like side, type, and symbol.
 */

// The #pragma once directive is a modern header guard, ensuring that the contents
// of this file are only included once per compilation unit, preventing redefinition errors.
#pragma once

#include <string> // Required for using std::string to store the order's symbol.

// --- Enumerations for Order Properties ---

/**
 * @enum Side
 * @brief Represents the side of an order (Buy or Sell).
 * Using a 'scoped enum' (enum class) is safer than a plain enum as it prevents
 * accidental integer conversions and naming conflicts.
 */
enum class Side {
    Buy,  // An order to purchase an asset.
    Sell  // An order to sell an asset.
};

/**
 * @enum OrderType
 * @brief Represents the execution type and time-in-force policy of an order.
 */
enum class OrderType {
    Market, // An order to execute immediately at the best available market price.
    Limit,  // An order to execute at a specific price or better.
    IOC,    // Immediate-Or-Cancel: Executes all or part of the order immediately, cancels the rest.
    FOK     // Fill-Or-Kill: Executes the entire order immediately, or cancels if it can't be fully filled.
};

/**
 * @class Order
 * @brief Represents a single trading order within the system.
 *
 * This class encapsulates all the necessary data for an order, such as its unique ID,
 * type, side, quantity, price, and symbol. It is designed to be a simple data
 * container with controlled access to its properties.
 */
class Order {
public: // Public interface of the class.

    /**
     * @brief Constructs a new Order object.
     * The constructor is responsible for initializing an order into a valid state.
     * It automatically assigns a new, unique order ID.
     * @param type The type of the order (Market, Limit, etc.).
     * @param side The side of the order (Buy or Sell).
     * @param quantity The quantity of the asset to be traded.
     * @param symbol The trading symbol (e.g., "BTC-USDT").
     * @param price The limit price for the order. Defaults to 0.0 for Market orders.
     */
    Order(OrderType type, Side side, double quantity, const std::string& symbol, double price = 0.0) {
        // Assign a unique ID from the shared counter and then increment the counter.
        this->orderID_ = next_order_id_++;
        this->type_ = type;
        this->side_ = side;
        this->price_ = price;
        this->quantity_ = quantity;
        this->symbol_ = symbol;
    }

    // --- Public "Getter" Methods ---
    // These methods provide read-only access to the order's private data members.
    // The 'const' keyword at the end is a promise that these methods will not modify the object's state.

    int getOrderID() const { return this->orderID_; }
    OrderType getType() const { return this->type_; }
    Side getSide() const { return this->side_; }
    double getPrice() const { return this->price_; }
    double getQuantity() const { return this->quantity_; }
    // Returning a const reference (&) to the symbol is an optimization that avoids
    // making an unnecessary and potentially expensive copy of the string.
    const std::string& getSymbol() const { return this->symbol_; }

    // --- Public "Setter" Method ---
    /**
     * @brief Reduces the quantity of the order, typically after a partial fill.
     * This method is not 'const' because its purpose is to modify the object's state.
     * @param amount The amount by which to reduce the quantity.
     */
    void reduceQuantity(double amount) {
        if (amount <= this->quantity_) {
            this->quantity_ -= amount;
        }
    }

private: // Private members can only be accessed by methods within this class.

    // --- Member Variables ---
    // The trailing underscore is a common C++ coding style to distinguish private member variables.
    int orderID_;
    OrderType type_;
    Side side_;
    double price_;
    double quantity_;
    std::string symbol_;

    // --- Static ID Counter ---
    // The 'static' keyword means this variable is shared across ALL instances of the Order class.
    // It belongs to the class itself, not to any single object. This is how we ensure
    // every order created gets a unique, incrementing ID.
    static int next_order_id_;
};
