#include "typed_fields.h"

#include "xpp_text.h"
#include "xpp_util.h"

namespace xpp {

/* the fault of v outside min..max, or nothing */
static std::optional<std::string> out_of_range(double v, std::optional<double> min, std::optional<double> max)
{
  if(min&&v<*min)return xpp::format("{:g} is below {:g}",v,*min);
  if(max&&v>*max)return xpp::format("{:g} is above {:g}",v,*max);
  return std::nullopt;
}

int TypedFields::integer(std::string_view field, const std::string &text, std::optional<double> min,
                         std::optional<double> max)
{
  if(error_)return 0;
  int v=0;
  if(!parse_int(trim_blanks(text),v)){
    fail(field,xpp::format("\"{}\" is not a whole number",text));
    return 0;
  }
  if(const auto bad=out_of_range(v,min,max)){
    fail(field,*bad);
    return 0;
  }
  return v;
}

double TypedFields::real(std::string_view field, const std::string &text, std::optional<double> min,
                         std::optional<double> max)
{
  if(error_)return 0;
  double v=0;
  if(!parse_number(trim_blanks(text),v)){
    fail(field,xpp::format("\"{}\" is not a number",text));
    return 0;
  }
  if(const auto bad=out_of_range(v,min,max)){
    fail(field,*bad);
    return 0;
  }
  return v;
}

bool TypedFields::boolean(std::string_view field, const std::string &text)
{
  if(error_)return false;
  if(text=="true")return true;
  if(text!="false")fail(field,xpp::format("\"{}\" is not true or false",text));
  return false;
}

Color TypedFields::color(std::string_view field, const std::string &text)
{
  if(error_)return foreground_color;
  if(const auto c=parse_color(trim_blanks(text)))return *c;
  fail(field,xpp::format("\"{}\" is not a color: write #rrggbb, #rrggbbaa, a CSS color name or foreground",text));
  return foreground_color;
}

int TypedFields::choice(std::string_view field, const std::string &text, std::span<const std::string> choices)
{
  if(error_)return 0;
  for(std::size_t i=0;i<choices.size();i++)
    if(choices[i]==text)return static_cast<int>(i);
  std::string names;
  for(const std::string &c:choices)names+=(names.empty()?"":", ")+c;
  fail(field,xpp::format("\"{}\" is not one of {}",text,names));
  return 0;
}

void TypedFields::fail(std::string_view field, std::string_view what)
{
  if(!error_)error_=xpp::format("{}: {}",field,what);
}

} // namespace xpp
