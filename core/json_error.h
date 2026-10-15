#ifndef XPP_JSON_ERROR_H
#define XPP_JSON_ERROR_H
#include "xpp_error.h"
namespace xpp::json {
/* Append the common error and place fields to an event object. */
void append_error(std::string &out, const xpp::Error &error);
/* an error as the one event line: `ev` ("message", or "error" for a model
   that does not load) with the fields append_error gives. Pure: any thread. */
std::string error_event(const char *ev, const xpp::Error &error);
}
#endif
