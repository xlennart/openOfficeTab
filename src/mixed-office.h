#pragma once
#include "mixed-store.h"
struct View;class Session;
namespace mixedoffice {
class Client {
    Store store;
    uint64_t owner=0,start=0;
    std::vector<Member> snapshot;
    bool ticking=false;
    uint64_t localKey(const Session&,uint64_t id)const;
public:
    Client();~Client();
    void tick(Session&);
    void stop(Session&);
    bool members(const Session&,HWND,std::vector<std::shared_ptr<View>>&)const;
    std::shared_ptr<View> remote(uint64_t)const;
    bool contains(HWND)const;
    bool request(Session&,uint64_t,Command,HWND);
    bool reorder(Session&,uint64_t,uint64_t);
    bool drop(Session&,uint64_t,POINT);
    bool joinInto(Session&,uint64_t,HWND);
    bool setWidth(Session&,uint64_t,int);
    void activated(Session&,HWND);
    void choose(Session&,HWND,uint64_t,POINT);
    void leave(Session&,uint64_t);
};
}
