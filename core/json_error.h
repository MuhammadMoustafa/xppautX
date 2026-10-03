#ifndef XPP_JSON_ERROR_H
#define XPP_JSON_ERROR_H
#include "xpp_error.h"
namespace xpp::json {
/* Append the common error and place fields to an event object. */
void append_error(std::string &out, const xpp::Error &error);
}
#endif
