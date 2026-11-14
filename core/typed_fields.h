#pragma once

#include "colormap.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace xpp {

/* One reading of the fields typed in a form, all or nothing: the first
   fault is kept with the field it belongs to, no later field is read once
   there is one, and the caller applies nothing when error() is set. */
class TypedFields {
 public:
  /* a whole number (parse_int), within min..max where given; 0 after a fault */
  int integer(std::string_view field, const std::string &text, std::optional<double> min = std::nullopt,
              std::optional<double> max = std::nullopt);
  /* a plain real number (parse_number: no %formula), within min..max where
     given; 0 after a fault */
  double real(std::string_view field, const std::string &text, std::optional<double> min = std::nullopt,
              std::optional<double> max = std::nullopt);
  /* `true` or `false`; false after a fault */
  bool boolean(std::string_view field, const std::string &text);
  /* a color (colormap.h's parse_color); the foreground after a fault */
  Color color(std::string_view field, const std::string &text);
  /* one of choices, by its name, exactly: its index; 0 after a fault */
  int choice(std::string_view field, const std::string &text, std::span<const std::string> choices);
  void fail(std::string_view field, std::string_view what);
  const std::optional<std::string> &error() const { return error_; }

 private:
  std::optional<std::string> error_;
};

} // namespace xpp
