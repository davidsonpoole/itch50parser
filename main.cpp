#include <_stdio.h>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include <memory>
#include <unordered_map>
#include "main.h"

class ItchMessageParser {
private:
    ItchSystemEventMessage system_event;
    ItchStockDirectoryMessage stock_dir;
    ItchAddOrderMessage add_order;
    ItchAddOrderAttrMessage add_order_attr;
    ItchOrderExecutedMessage order_executed;
    ItchOrderExecutedPriceMessage order_executed_price;
    ItchCancelOrderMessage cancel_order;
    ItchDeleteOrderMessage delete_order;
    ItchOrderReplaceMessage order_replace;

public:
    void parse(char *buf) {
        char msg_type = buf[0];
        switch (msg_type) {
            case 'S':
                memcpy(&system_event, buf+1, sizeof(system_event));
                on_system_event(system_event);
                break;
            case 'R':
                memcpy(&stock_dir, buf+1, sizeof(stock_dir));
                on_stock_dir(stock_dir);
                break;
            case 'A':
                memcpy(&add_order, buf + 1, sizeof(add_order));
                on_add(add_order);
                break;
            case 'F':
                memcpy(&add_order_attr, buf + 1, sizeof(add_order_attr));
                on_add_attr(add_order_attr);
                break;
            case 'E':
                memcpy(&order_executed, buf+1, sizeof(order_executed));
                on_executed(order_executed);
                break;
            case 'C':
                memcpy(&order_executed_price, buf+1, sizeof(order_executed_price));
                on_executed_price(order_executed_price);
                break;
            case 'X':
                memcpy(&cancel_order, buf+1, sizeof(cancel_order));
                on_cancel(cancel_order);
                break;
            case 'D':
                memcpy(&delete_order, buf+1, sizeof(delete_order));
                on_delete(delete_order);
                break;
            case 'U':
                memcpy(&order_replace, buf+1, sizeof(order_replace));
                on_replace(order_replace);
                break;

            default:
                //std::cerr << "Message type not yet supported!" << std::endl;
                break;
        }
    }
};

static std::vector<std::unique_ptr<StockImpl>> g_stocks;
static std::unordered_map<uint64_t, OrderPtr> g_orders_by_ref_num;
static long long g_allocations;
static long g_open_orders;
static long g_max_open_orders;
static long g_total_order_count;
static std::chrono::high_resolution_clock::time_point g_start;

// A pool that hands out fixed-size chunks, reuses them on "deallocation"
static bool g_filter = true;

uint64_t parse_timestamp(const char ts[6]) {
    uint64_t val = 0;
    memcpy(reinterpret_cast<char*>(&val) + 2, ts, 6);
    return ntohll(val);
}

void on_system_event(ItchSystemEventMessage &msg) {
    switch (msg.event) {
        case 'O':
            std::cout << "Start of messages" << std::endl;
            break;
        case 'S':
            std::cout << "Start of system hours" << std::endl;
            break;
        case 'Q':
            std::cout << "Start of market hours" << std::endl;
            break;
        case 'M':
            std::cout << "End of market hours" << std::endl;
            break;
        case 'E':
            std::cout << "End of system hours" << std::endl;
            break;
        case 'C':
            std::cout << "End of messages" << std::endl;
            break;
    }
}

void on_stock_dir(ItchStockDirectoryMessage& msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    stock->market = msg.market;
    memcpy(stock->name, msg.stock, 8);
};

Order* add_order(StockImpl* stock, price_t price, uint32_t shares, uint64_t ref_num, char side) {
    Order *o = new_order();
    o->stock = stock;
    o->price = price;
    o->shares = shares;
    o->ref_num = ref_num;
    o->side = side;

    stock->book_order(o);
    g_orders_by_ref_num[ref_num] = OrderPtr(o);

    g_open_orders++;
    g_max_open_orders = std::max(g_max_open_orders, g_open_orders);
    g_total_order_count++;

    if (g_total_order_count % 100000 == 0) {
        std::cout << g_total_order_count << std::endl;
    }

    // if (g_order_count == 100000) {
    //     auto end = std::chrono::high_resolution_clock::now();
    //     auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - g_start);
    //     std::cout << duration.count() << "ns" << std::endl;
    //     exit(0);
    // }

    return o;
}

void remove_order(Order* o) {
    o->stock->remove_order(o);
    g_orders_by_ref_num.erase(o->ref_num); // unique ptr destructs here, no need to free
    g_open_orders--;
}

void on_add(ItchAddOrderMessage& msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    add_order(stock, ntohl(msg.price), ntohl(msg.shares), ntohll(msg.order_ref_num), msg.side);

    // std::cout << std::endl;
    // std::cout << "Locate: " << ntohs(msg.stock_locate) << std::endl;
    // std::cout << "Tracking Num: " << ntohs(msg.tracking_num) << std::endl;
    // std::cout << "Timestamp: " << parse_timestamp(msg.timestamp) << std::endl;
    // std::cout << "Order Ref Num: " << ntohll(msg.order_ref_num) << std::endl;
    // std::cout << "Side: " << msg.side << std::endl;
    // std::cout << "Shares: " << ntohl(msg.shares) << std::endl;
    // std::cout << "Stock: " << std::string(msg.stock, 8) << std::endl;
    // std::cout << "Price: " << ntohl(msg.price) << std::endl;
};

void on_add_attr(ItchAddOrderAttrMessage &msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    add_order(stock, ntohl(msg.price), ntohl(msg.shares), ntohll(msg.order_ref_num), msg.side);
}

void on_executed(ItchOrderExecutedMessage &msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *o = g_orders_by_ref_num[ntohll(msg.order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    o->shares -= ntohl(msg.executed_shares);
    if (o->shares == 0) {
        // remove order
        remove_order(o);
    }
}

void on_executed_price(ItchOrderExecutedPriceMessage &msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *o = g_orders_by_ref_num[ntohll(msg.order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    o->shares -= ntohl(msg.executed_shares);
    if (o->shares == 0) {
        // remove order
        remove_order(o);
    }
}

void on_cancel(ItchCancelOrderMessage& msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *o = g_orders_by_ref_num[ntohll(msg.order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    o->shares -= ntohl(msg.cancelled_shares);
    if (o->shares == 0) {
        // remove order
        remove_order(o);
    }

}

void on_delete(ItchDeleteOrderMessage &msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }
    //std::cout << "Order delete for stock " << std::string(stock->name, 8) << std::endl;

    Order *o = g_orders_by_ref_num[ntohll(msg.order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    remove_order(o);
}

void on_replace(ItchOrderReplaceMessage &msg) {
    StockImpl *stock = g_stocks[ntohs(msg.stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *old = g_orders_by_ref_num[ntohll(msg.orig_order_ref_num)].get();
    if (old == nullptr) {
        std::cerr << "Old order is null" << std::endl;
        return;
    }

    char side = old->side;

    remove_order(old);

    add_order(stock, ntohl(msg.price), ntohl(msg.shares), ntohll(msg.new_order_ref_num), side);

}

int main() { 
    // initialize everything
    g_stocks.resize(10001);
    for (int i=1; i<=10000; i++) {
        g_stocks[i] = std::make_unique<StockImpl>();
        g_stocks[i]->locate = i;
    }

    for (int i=0; i<1000000; i++) {
        Order *o = alloc.new_object<Order>();
        alloc.delete_object(o);
    }

    std::ifstream file("data/01302019.NASDAQ_ITCH50", std::ios::binary);

    uint16_t msg_len;
    char buf[1024];

    ItchMessageParser parser;

    // start timer
    g_start = std::chrono::high_resolution_clock::now();

    while (file.read(reinterpret_cast<char*>(&msg_len), 2)) {
        msg_len = ntohs(msg_len);
        file.read(buf, msg_len);
        parser.parse(buf);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - g_start);
    std::cout << "Total time: " << duration.count() << " ns" << std::endl;
    std::cout << "Total orders: " << g_total_order_count << std::endl;
    std::cout << "Max open orders: " << g_max_open_orders << std::endl;
    std::cout << "Time per order: " << duration.count() / g_total_order_count << std::endl;
    std::cout << "Allocations: " << g_allocations << std::endl;

    // validate orders
    if (!g_orders_by_ref_num.empty()) {
        std::cout << "Did not clean up properly! Size: " << g_orders_by_ref_num.size() << std::endl;
    } else {
        std::cout << "Success!" << std::endl;
    }

    return 0;
}