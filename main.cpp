#include <_stdio.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include <memory>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include "main.h"

class ItchMessageParser {
public:
    void parse(char *buf) {
        char msg_type = buf[0];
        switch (msg_type) {
            case 'S':
                on_system_event(reinterpret_cast<ItchSystemEventMessage*>(buf+1));
                break;
            case 'R':
                on_stock_dir(reinterpret_cast<ItchStockDirectoryMessage*>(buf+1));
                break;
            case 'A':
                on_add(reinterpret_cast<ItchAddOrderMessage*>(buf+1));
                break;
            case 'F':
                on_add_attr(reinterpret_cast<ItchAddOrderAttrMessage*>(buf+1));
                break;
            case 'E':
                on_executed(reinterpret_cast<ItchOrderExecutedMessage*>(buf+1));
                break;
            case 'C':
                on_executed_price(reinterpret_cast<ItchOrderExecutedPriceMessage*>(buf+1));
                break;
            case 'X':
                on_cancel(reinterpret_cast<ItchCancelOrderMessage*>(buf+1));
                break;
            case 'D':
                on_delete(reinterpret_cast<ItchDeleteOrderMessage*>(buf+1));
                break;
            case 'U':
                on_replace(reinterpret_cast<ItchOrderReplaceMessage*>(buf+1));
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

void on_system_event(ItchSystemEventMessage* msg) {
    switch (msg->event) {
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

void on_stock_dir(ItchStockDirectoryMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    stock->market = msg->market;
    memcpy(stock->name, msg->stock, 8);
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
        std::cout << g_total_order_count << '\n';
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

void on_add(ItchAddOrderMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    add_order(stock, ntohl(msg->price), ntohl(msg->shares), ntohll(msg->order_ref_num), msg->side);

    // std::cout << std::endl;
    // std::cout << "Locate: " << ntohs(msg->stock_locate) << std::endl;
    // std::cout << "Tracking Num: " << ntohs(msg->tracking_num) << std::endl;
    // std::cout << "Timestamp: " << parse_timestamp(msg->timestamp) << std::endl;
    // std::cout << "Order Ref Num: " << ntohll(msg->order_ref_num) << std::endl;
    // std::cout << "Side: " << msg->side << std::endl;
    // std::cout << "Shares: " << ntohl(msg->shares) << std::endl;
    // std::cout << "Stock: " << std::string(msg->stock, 8) << std::endl;
    // std::cout << "Price: " << ntohl(msg->price) << std::endl;
};

void on_add_attr(ItchAddOrderAttrMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    add_order(stock, ntohl(msg->price), ntohl(msg->shares), ntohll(msg->order_ref_num), msg->side);
}

void on_executed(ItchOrderExecutedMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *o = g_orders_by_ref_num[ntohll(msg->order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    o->shares -= ntohl(msg->executed_shares);
    if (o->shares == 0) {
        // remove order
        remove_order(o);
    }
}

void on_executed_price(ItchOrderExecutedPriceMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *o = g_orders_by_ref_num[ntohll(msg->order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    o->shares -= ntohl(msg->executed_shares);
    if (o->shares == 0) {
        // remove order
        remove_order(o);
    }
}

void on_cancel(ItchCancelOrderMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *o = g_orders_by_ref_num[ntohll(msg->order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    o->shares -= ntohl(msg->cancelled_shares);
    if (o->shares == 0) {
        // remove order
        remove_order(o);
    }

}

void on_delete(ItchDeleteOrderMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }
    //std::cout << "Order delete for stock " << std::string(stock->name, 8) << std::endl;

    Order *o = g_orders_by_ref_num[ntohll(msg->order_ref_num)].get();
    if (o == nullptr) {
        std::cerr << "Order is null" << std::endl;
        return;
    }
    remove_order(o);
}

void on_replace(ItchOrderReplaceMessage* msg) {
    StockImpl *stock = g_stocks[ntohs(msg->stock_locate)].get();
    if (g_filter) {
        if (memcmp(stock->name, "AAPL    ", 8) != 0) {
            return;
        }
    }

    Order *old = g_orders_by_ref_num[ntohll(msg->orig_order_ref_num)].get();
    if (old == nullptr) {
        std::cerr << "Old order is null" << std::endl;
        return;
    }

    char side = old->side;

    remove_order(old);

    add_order(stock, ntohl(msg->price), ntohl(msg->shares), ntohll(msg->new_order_ref_num), side);

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

    constexpr size_t BUF_SIZE = 256 * 1024;
    alignas(64) char buf[BUF_SIZE];

    int fd = open("data/01302019.NASDAQ_ITCH50", O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open file" << std::endl;
        return 1;
    }

    ItchMessageParser parser;

    // start timer
    g_start = std::chrono::high_resolution_clock::now();

    size_t remaining = 0;
    for (;;) {
        ssize_t bytes_read = read(fd, buf + remaining, BUF_SIZE - remaining);
        if (bytes_read <= 0) break;
        size_t avail = remaining + bytes_read;
        size_t pos = 0;
        while (pos + 2 <= avail) {
            uint16_t msg_len = ntohs(*reinterpret_cast<const uint16_t*>(buf + pos));
            if (pos + 2 + msg_len > avail) break;
            pos += 2;
            parser.parse(buf + pos);
            pos += msg_len;
        }
        remaining = avail - pos;
        if (remaining > 0) {
            memmove(buf, buf + pos, remaining);
        }
    }

    close(fd);

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