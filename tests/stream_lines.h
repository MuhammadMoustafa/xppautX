#pragma once
/* The lines of a FILE * a test keeps open and positions itself (tell_stream, seek_stream): what the unit
   tests of autoX's files read their results with, one line at a time. */
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

class StreamLines {
public:
    explicit StreamLines(std::FILE *fp) : fp_(fp) {}
    /* the next line without its \n or \r\n; nothing at the end of the stream (a view valid until the next call) */
    std::optional<std::string_view> next()
    {
        line_.clear();
        bool any = false;
        for (int c; (c = std::fgetc(fp_)) != EOF;) {
            any = true;
            if (c == '\n') break;
            line_ += static_cast<char>(c);
        }
        if (!any) return std::nullopt;
        if (!line_.empty() && line_.back() == '\r') line_.pop_back();
        return std::string_view(line_);
    }

private:
    std::FILE *fp_;
    std::string line_;
};
