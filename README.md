# High-Performance Cryptocurrency Matching Engine

This project is a C++ implementation of a high-performance cryptocurrency matching engine, developed as part of an assignment for GoQuant. The engine is designed based on REG NMS-inspired principles of price-time priority and provides a network-accessible API for order submission and real-time data dissemination.

## Table of Contents
1. [Features](#features)
2. [System Architecture](#system-architecture)
3. [Core Logic & Data Structures](#core-logic--data-structures)
4. [API Specification](#api-specification)
5. [How to Build and Run](#how-to-build-and-run)
6. [How to Test](#how-to-test)
7. [Design Choices & Trade-offs](#design-choices--trade-offs)

## Features
- **Price-Time Priority Matching:** Strictly enforces that orders at a better price are filled first. For orders at the same price, the one that arrived earlier is prioritized (FIFO).
- **Multi-Symbol Support:** The engine can handle multiple trading pairs (e.g., `BTC-USDT`, `ETH-USDT`) concurrently, each with its own independent order book.
- **Core Order Types:** Full support for `Market`, `Limit`, `Immediate-Or-Cancel (IOC)`, and `Fill-Or-Kill (FOK)` orders.
- **Internal Trade-Through Protection:** An incoming aggressive order is always matched at the best available price(s) on the internal order book.
- **Real-Time Data Feeds:** Provides live, push-based data streams for trade executions and market data (BBO and order book depth) using Server-Sent Events (SSE).
- **Network-Accessible API:** A robust API allows clients to submit orders and subscribe to data feeds over the network.

## System Architecture
The application is a single, multi-threaded C++ backend server. It is composed of several key classes that work together:

- **`main.cpp`:** The entry point of the application. It is responsible for setting up the `cpp-httplib` server, defining the API endpoints, and instantiating the `MatchingEngine`.
- **`MatchingEngine`:** The central orchestrator of the entire system. It holds a map of all active `OrderBook`s, routing incoming orders to the correct book based on their symbol. It is also responsible for managing client connections and broadcasting real-time data events.
- **`OrderBook`:** The heart of the matching logic for a *single* trading symbol. It maintains the bid and ask sides of the book, enforces price-time priority, and executes trades when orders match.
- **`Order` & `Trade`:** Simple data structs that represent a trading order and an executed trade, respectively. They encapsulate the data associated with these core concepts.

## Core Logic & Data Structures

The performance and correctness of the matching engine hinge on its core data structure choice for the order book.

- **Data Structure:** An `std::map<double, std::list<Order>>` was chosen to represent each side of the order book (bids and asks).
    - **`std::map` (Price Priority):** The `map` key is the price. This choice automatically enforces price priority because `std::map` keeps its keys sorted. For asks, the default ascending sort works perfectly (lowest price first). For bids, a `std::greater<double>` comparator is used to sort keys in descending order (highest price first). This gives efficient O(log N) access to the best price level.
    - **`std::list` (Time Priority):** The `map` value is a `std::list` of orders. A `std::list` functions as a perfect First-In, First-Out (FIFO) queue, ensuring that for a given price, the order that arrived first is matched first.

This combination elegantly and efficiently satisfies the core requirement of price-time priority.

## API Specification

The server listens on `http://localhost:8080`.

### 1. Order Submission (REST API)
- **Endpoint:** `POST /order`
- **Description:** Submits a new order to the matching engine.
- **Request Body:** `application/json`
  ```json
  {
      "symbol": "BTC-USDT",
      "order_type": "limit",
      "side": "buy",
      "quantity": 5.0,
      "price": 30000.50
  }
  ```
  - **`symbol`** (string, required): The trading pair.
  - **`order_type`** (string, required): One of `market`, `limit`, `ioc`, `fok`.
  - **`side`** (string, required): One of `buy` or `sell`.
  - **`quantity`** (number, required): The amount to trade.
  - **`price`** (number, optional): Required for `limit`, `ioc`, and `fok` orders. Ignored for `market` orders.
- **Success Response:** `200 OK`
  ```json
  {
      "status": "Order Received",
      "order_id": 123
  }
  ```
- **Error Response:** `400 Bad Request`
  ```json
  {
      "status": "Error",
      "message": "Invalid order_type specified: 'lmt'"
  }
  ```

### 2. Trade Data Feed (Server-Sent Events)
- **Endpoint:** `GET /ws/trades`
- **Description:** A persistent, push-based stream of executed trades.
- **Data Format:** Server-Sent Events (`text/event-stream`). Each message is prefixed with `data: ` and contains a JSON object.
- **Sample Message:**
  ```
  data: {"aggressor_side":"buy","maker_order_id":1,"price":101.0,"quantity":2.0,"symbol":"BTC-USDT","taker_order_id":4,"trade_id":1,"type":"trade"}
  ```

### 3. Market Data Feed (Server-Sent Events)
- **Endpoint:** `GET /ws/marketdata`
- **Description:** A persistent, push-based stream of Level 2 market data updates, triggered by any change in the order book.
- **Data Format:** Server-Sent Events (`text/event-stream`).
- **Sample Message:**
  ```
  data: {"asks":[["101.000000","3.000000"]],"best_ask":101.0,"best_bid":99.0,"bids":[["99.000000","8.000000"]],"symbol":"BTC-USDT","type":"l2update"}
  ```

## How to Build and Run

### Prerequisites
- A C++17 compliant compiler (MSVC, GCC, Clang)
- CMake (version 3.15 or higher)

### Build Steps
1. Navigate to the project's root directory.
2. Create a build directory: `cmake -B build`
3. Compile the project: `cmake --build build`

### Running the Server
1. The executable will be located in the `build/Debug` (on Windows) or `build` (on Linux/macOS) directory.
2. **Important:** Copy the `index.html` file into this same directory (`build/Debug` or `build`).
3. Run the executable from the terminal: `.\engine.exe` (Windows) or `./engine` (Linux/macOS).
4. The server will start and listen on `http://localhost:8080`.

## How to Test

The most reliable way to test all features is with a combination of a web browser and a command-line tool.

1. **Start the Server:** Run the `engine.exe` as described above.
2. **Launch the Test Client:** Open a web browser (Chrome, Firefox, Edge) and navigate to `http://localhost:8080`. This will load a simple HTML page that automatically connects to the `trades` and `marketdata` feeds and displays the messages in real-time.
3. **Submit Orders:** Use a tool like PowerShell's `Invoke-WebRequest` or Postman to send `POST` requests to the `/order` endpoint.

   **Example using PowerShell:**
   ```powershell
   Invoke-WebRequest -Uri http://localhost:8080/order -Method POST -ContentType "application/json" -Body '{"symbol":"BTC-USDT","order_type":"market","side":"buy","quantity":2}'
   ```
4. **Observe:** As you submit orders, watch the web page update instantly with trade and market data messages.

## Design Choices & Trade-offs
- **`cpp-httplib` & `nlohmann/json`:** These single-header libraries were chosen for their simplicity and ease of integration, avoiding complex dependencies and build configurations, which is ideal for a self-contained assignment.
- **Server-Sent Events (SSE) vs. WebSockets:** While the assignment mentions WebSockets, SSE was ultimately implemented for the data feeds. SSE is a simpler, one-way protocol that perfectly fits the requirement of the server pushing data to the client. This approach solved some client-side compatibility and connection-handling complexities encountered during development, while still delivering the required real-time push functionality.
- **Error Handling:** The system uses C++ exceptions for handling invalid input from the API (e.g., malformed JSON, invalid enum values). The main server handler catches these exceptions and translates them into `400 Bad Request` HTTP responses with clear error messages.
