#ifndef XPP_JSON_READER_H
#define XPP_JSON_READER_H
/* The JSON reader (json_reader.cpp): reads text in place through pointers
   into it, with no tree. The protocol's command lines (json_io.cpp's
   get_string, get_num ...) and keymap.json (xpp_keymap.cpp) read with it. */
#include <initializer_list>
#include <string>
#include <string_view>

namespace xpp::json {

const char *skip_ws(const char *p);
const char *skip_value(const char *p);
const char *js_find(const char *obj, const char *key);
/* the JSON text of the value at v (its whitespace after it left out;
   empty for NULL) */
std::string_view js_raw(const char *v);
/* text is one JSON value, strictly (RFC 8259), with nothing after it but
   whitespace: what a file holds before it goes into an event */
bool js_valid(const char *text);
/* js_valid's reason: NULL when text is valid JSON, else what is wrong
   ("is not valid JSON", "is nested too deeply", "has text after its
   value") and `at`, where in the text it stopped */
const char *js_problem(const char *text, const char *&at);
/* the object at obj as JSON text without its members named in drop */
std::string js_object_without(const char *obj, std::initializer_list<std::string_view> drop);
/* The JSON string at v into out, whole; false (out empty) when v is not a string. */
bool js_string(const char *v, std::string &out);
const char *js_elem(const char *arr, int i);
/* The members of the object at `cursor`, in order: each call gives the next
   one's key (decoded) and where its value is, and moves the cursor past the
   value; false at the object's end. Start with the cursor at the object. */
bool js_member(const char *&cursor, std::string &key, const char *&value);
/* the line (from 1) of `at` in text, for an error's place */
int js_line(const char *text, const char *at);

} // namespace xpp::json

#endif
