#ifndef DATA_EVENT_H
#define DATA_EVENT_H

/* A data event a client asked for (`{"cmd":"data"}`) that carries a
   whole set of values and is sent again only when they changed: AUTO's
   settings (auto_settings.cpp) and the main numerics
   (numerics_settings.cpp). The front end gives the line's sink once
   (init); subscribe() says whether the client wants it and sends it at
   once whatever it holds; update() sends it when its text changed. The
   event's text is built from Args (the Session it is about, W47d), which
   subscribe() and update() pass on. C++ only. */

#include <cstddef>
#include <new>
#include <string>

#include "xpp_mem.h"

namespace xpp {

template <class... Args>
class ChangedEvent {
public:
    using Emit = void (*)(const char *line, size_t len);
    /* the event's line now; empty: none to send (nothing to say yet) */
    using Build = std::string (*)(const Args &...);

    constexpr ChangedEvent(Build build, const char *what) : build_(build), what_(what) {}

    void init(Emit emit) { emit_ = emit; }
    void subscribe(bool on, const Args &...args)
    {
        subscribed_ = on;
        sent_valid_ = false;
        update(args...);
    }
    void update(const Args &...args)
    {
        if (!emit_ || !subscribed_) return;
        try {
            std::string text = build_(args...);
            if (text.empty() || (sent_valid_ && text == sent_)) return;
            emit_(text.c_str(), text.size());
            sent_ = std::move(text);
            sent_valid_ = true;
        } catch (const std::bad_alloc &) {
            xpp::out_of_memory(what_);
        }
    }

private:
    Build build_;
    const char *what_;
    Emit emit_ = nullptr;
    bool subscribed_ = false;
    std::string sent_;
    bool sent_valid_ = false;
};

} // namespace xpp

#endif
