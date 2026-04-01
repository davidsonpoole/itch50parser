#include <iostream>
#include <memory_resource>
#include <cstdint>
#include <map>

#pragma pack(push, 1)

struct ItchSystemEventMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char event;
};

struct ItchStockDirectoryMessage{
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    char market;
    char financial_status;
    uint32_t round_lot_size;
    char round_lots_only;
    char issue_type;
    char issue_subtype[2];
    char authenticity;
    char short_sale_threshold_ind;
    char ipo;
    char luld_ref_price_tier;
    char etp;
    uint32_t etp_leverage_factor;
    char inverse;
};

struct ItchStockTradingActionMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    char trading_state;
    char reserved;
    char reason[4];
};

struct ItchRegShoShortSalePriceIndicatorMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    char reg_sho_action;
};

struct ItchMarketParticipantPositionMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char mpid[4];
    char stock[8];
    char primary;
    char mode;
    char state;
};

struct ItchMWCBDeclineLevelsMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t level1;
    uint64_t level2;
    uint64_t level3;
};

struct ItchMWCBStatusMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char level;
};

struct ItchQuotingPeriodUpdateMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    uint32_t ipo_release_time;
    char ipo_release_qualifier;
    uint32_t ipo_price;
};

struct ItchLULDAuctionCollarMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    uint32_t ref_price;
    uint32_t upper;
    uint32_t lower;
    uint32_t extension;
};

struct ItchOperationalHaltMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    char market;
    char action;
};

struct ItchAddOrderAttrMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
    char side;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    char attr[4];
};

struct ItchAddOrderMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
    char side;
    uint32_t shares;
    char stock[8];
    uint32_t price;
};

struct ItchCancelOrderMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
    uint32_t cancelled_shares;
};

struct ItchDeleteOrderMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
};

struct ItchOrderReplaceMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t orig_order_ref_num;
    uint64_t new_order_ref_num;
    uint32_t shares;
    uint32_t price;
};

struct ItchTradeMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
    char side;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint64_t match_num;
};

struct ItchCrossTradeMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t shares;
    char stock[8];
    uint32_t price;
    uint64_t match_num;
    char cross_type;
};

struct ItchTradeBrokenMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t match_num;
};

struct ItchNOIIMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t paired;
    uint64_t imbalance_shares;
    char imbalance;
    char stock[8];
    uint32_t far_price;
    uint32_t near_price;
    uint32_t ref_price;
    char cross_type;
    char price_variation_ind;
};

struct ItchRPIIMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    char interest_flag;
};

struct ItchDLCRMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    char stock[8];
    char open_elig_status;
    uint32_t min_price;
    uint32_t max_price;
    uint32_t near_price;
    uint64_t near_exec_time;
    uint32_t lower;
    uint32_t upper;
};

struct ItchOrderExecutedMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
    uint32_t executed_shares;
    uint64_t match_num;
};

struct ItchOrderExecutedPriceMessage {
    uint16_t stock_locate;
    uint16_t tracking_num;
    char timestamp[6];
    uint64_t order_ref_num;
    uint32_t executed_shares;
    uint64_t match_num;
    char printable;
    uint32_t price;
};

#pragma pack(pop)

typedef uint32_t price_t;

class OrderSet;
class LinkedOrderSet;
class MapOrderSet;
template<typename OrderSetT>
class Stock;
class Order;

using StockImpl = Stock<LinkedOrderSet>;

std::pmr::unsynchronized_pool_resource pool;
std::pmr::polymorphic_allocator<Order> alloc{&pool};

struct Order {
    StockImpl* stock;
    uint64_t ref_num;
    char side;
    uint32_t shares;
    price_t price;
    bool booked;

    // For pooling
    Order *prev;
    Order *next;

    void clear() {
        stock = nullptr;
        ref_num = 0;
        side = 0;
        shares = 0;
        price = 0;
        booked = false;
    }
};

struct OrderDeleter {
    void operator()(Order* o) const {
        alloc.delete_object(o);
    }
};

using OrderPtr = std::unique_ptr<Order, OrderDeleter>;

void free(Order* order) {
    order->clear();
    alloc.delete_object(order);
}

Order* new_order() {
    return alloc.new_object<Order>();
}

class MapOrderSet {
private:
    std::map<price_t, Order*> levels;

public:
    void book_order(Order* o) {
        Order *l = levels[o->price];
        if (l != nullptr) {
            // insert at end
            Order* prev = nullptr;
            while (l != nullptr) {
                prev = l;
                l = l->next;
            }
            prev->next = o;
            o->prev = prev;
        } else {
            // only order in level
            levels[o->price] = o;
        }
    }

    void remove_order(Order* o) {
        Order *l = levels[o->price];
        if (l == o) {
            // order is head of price level
            if (o->next == nullptr) {
                levels.erase(o->price);
            } else {
                levels[o->price] = o->next;
                o->next->prev = nullptr;
                o->next = nullptr;
            }
        } else {
            // unlink order
            if (o->prev != nullptr) {
                o->prev->next = o->next;
            }
            if (o->next != nullptr) {
                o->next->prev = o->prev;
            }
            o->prev = nullptr;
            o->next = nullptr;
        }
    }
};

class LinkedOrderSet {

private:
    Order* head;
public:
    void book_order(Order* o) {
        Order *prev = nullptr;

        Order *next = head;

        if (o->side == 'B') {
            while (next != nullptr && o->price < next->price) {
                prev = next;
                next = next->next;
            }
        } else {
            while (next != nullptr && o->price > next->price) {
                prev = next;
                next = next->next;
            }
        }

        o->prev = prev;
        if (prev != nullptr) {
            prev->next = o;
        } else {
            head = o;
        }
        o->next = next;
        if (next != nullptr) {
            next->prev = o;
        }
    }

    void remove_order(Order* o) {

        Order *prev = o->prev;
        Order *next = o->next;
        
        if (prev != nullptr) {
            prev->next = next;
        } else {
            // this was the orderHead
            head = o->next;
        }

        if (next != nullptr) {
            next->prev = prev;
        }

        o->next = nullptr;
        o->prev = nullptr;

    }
};

template<typename OrderSetT>
class Stock {
public:
    uint16_t locate;
    char name[8];
    char market;

    OrderSetT bids;
    OrderSetT asks;

    void book_order(Order* o) {

        if (o->booked) return;

        auto& book = o->side == 'B' ? bids : asks;

        book.book_order(o);

        o->booked = true;

        // if (o->prev != nullptr && o->next != nullptr) {
        //     std::cout << "Booked at price " << o->price << " between orders " << 
        //         (o->prev != nullptr ? std::to_string(o->prev->ref_num) : "null") << " and " 
        //         << (o->next != nullptr ? std::to_string(o->next->ref_num) : "null") << std::endl;
        // }
    }

    void remove_order(Order* o) {

        if (!o->booked) return;

        auto& book = o->side == 'B' ? bids : asks;

        book.remove_order(o);

        o->booked = false;
        //std::cout << "Removed order " << std::to_string(o->ref_num) << std::endl;
    }
};

void on_system_event(ItchSystemEventMessage&);
void on_stock_dir(ItchStockDirectoryMessage&);
void on_trading_action(ItchStockTradingActionMessage&);
void on_reg_sho(ItchRegShoShortSalePriceIndicatorMessage&);
void on_market_participant(ItchMarketParticipantPositionMessage&);
void on_mwcb_decline_levels(ItchMWCBDeclineLevelsMessage&);
void on_mwcb_status(ItchMWCBStatusMessage&);
void on_quoting_period_update(ItchQuotingPeriodUpdateMessage&);
void on_luld_auction_collar(ItchLULDAuctionCollarMessage&);
void on_operational_halt(ItchOperationalHaltMessage&);
void on_add_attr(ItchAddOrderAttrMessage&);
void on_add(ItchAddOrderMessage&);
void on_cancel(ItchCancelOrderMessage&);
void on_delete(ItchDeleteOrderMessage&);
void on_replace(ItchOrderReplaceMessage&);
void on_trade(ItchTradeMessage&);
void on_cross_trade(ItchCrossTradeMessage&);
void on_trade_broken(ItchTradeBrokenMessage&);
void on_noii(ItchNOIIMessage&);
void on_rpii(ItchRPIIMessage&);
void on_dlcr(ItchDLCRMessage&);
void on_executed(ItchOrderExecutedMessage&);
void on_executed_price(ItchOrderExecutedPriceMessage&);