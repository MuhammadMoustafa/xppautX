#pragma once

/* core/xpp_text.cpp: the kernel's text formatting and text reading (xpp::format, xpp::number, the
   parsers of numbers and words, JSON strings, base64). Nothing here touches a file: the file
   handles are core/xpp_io.h's, which includes this header. W726 split them: the kernel includes
   nothing outside it.

   xpp::format takes a std::format_string, checked against the argument types at compile time, no
   printf %-verb ever mismatching an argument. xpp::number is a double's shortest round-trip text
   (std::to_chars), for when "%g" would do but a guaranteed-reversible digit string is wanted. */

#include <array>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace xpp {

/* the enumerator whose word `names` holds at its index (a choice named, never numbered), nothing for any other text */
template <typename Enum, typename Names>
constexpr std::optional<Enum> enum_from_name(const Names &names, std::string_view word)
{
    for (std::size_t i = 0; i < std::size(names); ++i)
        if (word == names[i]) return static_cast<Enum>(i);
    return std::nullopt;
}

/* std::format can throw (bad_alloc); no exception may cross into the C
   code that calls these C++ functions (AGENTS.md), so a failure is loud
   and final like xpp_mem's: an ERROR naming the call site, exit 1 */
[[noreturn]] void format_failed(const char *file, int line) noexcept;

/* One spelling for a NaN (W408, #470). IEEE 754 leaves a NaN's sign
   unspecified (0/0 is -nan on x86, nan on ARM) and std::format and
   std::to_chars print the sign, so every owner below passes a NaN as the
   positive quiet NaN: it prints as "nan" on every CPU, width and
   alignment kept. Anything else (a sign of 0, an infinity) is unchanged. */
template <class T> constexpr T canonical_nan(T v) noexcept
{
    return std::isnan(v) ? std::numeric_limits<T>::quiet_NaN() : v;
}

/* the format arguments of a call: a floating value is canonical_nan'd
   (a copy, kept by with_format_args for the call), any other argument is
   passed by reference as std::make_format_args does */
template <class T> using canonical_arg_t =
    std::conditional_t<std::is_floating_point_v<std::remove_cvref_t<T>>, std::remove_cvref_t<T>, T &&>;
template <class T> constexpr decltype(auto) canonical_arg(T &&v) noexcept
{
    if constexpr (std::is_floating_point_v<std::remove_cvref_t<T>>)
        return canonical_nan<std::remove_cvref_t<T>>(v);
    else
        return std::forward<T>(v);
}
/* calls f with the packed format arguments of args (canonical, see above);
   the copies live until f returns */
template <class F, class... Args> decltype(auto) with_format_args(F &&f, Args &&...args)
{
    std::tuple<canonical_arg_t<Args>...> values(canonical_arg(std::forward<Args>(args))...);
    return std::apply([&f](auto &...v) { return f(std::make_format_args(v...)); }, values);
}

/* The formatting itself, compiled once, here (xpp_io.cpp): the templates
   below (and xpp::log) only check the format string against the
   arguments at compile time and pack them (std::make_format_args), so a
   file that formats does not compile std::format's machinery again (W37:
   inlined in 81 files, it more than doubled the core's compile time) */
std::string vformat(std::string_view fmt, std::format_args args) noexcept;
/* the same, appended to out */
void vformat_append(std::string &out, std::string_view fmt, std::format_args args) noexcept;

/* Compile-time checked formatting: a bad "{}" against the argument
   types is a compile error, not a WARN at run time. Returns a
   std::string. */
template <class... Args>
std::string format(std::format_string<Args...> fmt, Args &&...args) noexcept
{
    return xpp::with_format_args([&fmt](std::format_args a) { return xpp::vformat(fmt.get(), a); },
                                 std::forward<Args>(args)...);
}

/* the same, appended to out (a file's text built up before it is written) */
template <class... Args>
void format_append(std::string &out, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    xpp::with_format_args([&](std::format_args a) { xpp::vformat_append(out, fmt.get(), a); },
                          std::forward<Args>(args)...);
}

/* A double's shortest round-trip decimal text (std::to_chars): unlike
   "%g" it never loses precision and never needs a chosen digit count;
   prefer this in new C++ code. */
inline std::string number(double v)
{
    std::array<char, 32> buf;
    std::to_chars_result r = std::to_chars(buf.data(), buf.data() + buf.size(), canonical_nan(v));
    if (r.ec == std::errc())
        return std::string(buf.data(), r.ptr);
    return std::to_string(v); /* unreachable for a finite double in 32 bytes */
}

/* A stored value's shortest round-trip text, appended to out: the shortest text that reads back as exactly the float v rounds
   to when single (a float the data store held), else as exactly the double v. The one text of every format of stored data
   (.dat, CSV, the page's numbers), so two formats of one run never differ in digits (W613). */
inline void append_stored(std::string &out, double v, bool single)
{
    std::array<char, 32> buf;
    const std::to_chars_result r = single
        ? std::to_chars(buf.data(), buf.data() + buf.size(), static_cast<float>(canonical_nan(v)))
        : std::to_chars(buf.data(), buf.data() + buf.size(), canonical_nan(v));
    if (r.ec == std::errc()) out.append(buf.data(), r.ptr);
}

/* v appended in the general format with `significant` digits: printf's "%.*g"
   character for character (std::format's "{:.Ng}" too), straight from
   std::to_chars into a stack buffer, without the format string's parse */
inline void append_general(std::string &out, double v, int significant)
{
    std::array<char, 32> buf;
    const std::to_chars_result r = std::to_chars(buf.data(), buf.data() + buf.size(), v, std::chars_format::general, significant);
    if (r.ec == std::errc()) out.append(buf.data(), r.ptr);
}

/* v's decimal digits appended, the text "{}" gives, straight from std::to_chars */
template <std::integral T>
inline void append_integer(std::string &out, T v)
{
    std::array<char, 24> buf;
    const std::to_chars_result r = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    out.append(buf.data(), r.ptr);
}

/* The parts joined into one string: text as it is, an integer as its digits
   (what "{}" gives), with no format string to parse: for the text a loop writes
   thousands of times (generated code), where xpp::format's scan is the cost. */
template <class... Parts>
std::string concat(const Parts &...parts)
{
    std::string out;
    ([&] {
        if constexpr (std::is_integral_v<Parts>) append_integer(out, parts);
        else out.append(std::string_view(parts));
    }(), ...);
    return out;
}

/* An entry of a C table other files read as NUL-terminated text (char *)
   whose text a std::string keeps: store takes text and entry points at
   it, valid until store changes again */
inline void keep_c_text(std::string &store, char *&entry, std::string_view text)
{
    store = text;
    entry = store.data();
}

/* a and b are the same text but for the case of ASCII letters (strcasecmp's
   test in the C locale): a model's names are looked up this way */
inline bool equal_ignoring_case(std::string_view a, std::string_view b)
{
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();i++){
    unsigned char x=static_cast<unsigned char>(a[i]),y=static_cast<unsigned char>(b[i]);
    if(x>='A'&&x<='Z')x=static_cast<unsigned char>(x-'A'+'a');
    if(y>='A'&&y<='Z')y=static_cast<unsigned char>(y-'A'+'a');
    if(x!=y)return false;
  }
  return true;
}

/* text without the white space (isspace's: blank, tab, CR, LF, VT, FF) around
   it: a typed value is read after this, so " 5 " is 5 and "5 x" is still
   refused; the one trim of the core. A view into `text`: copy it before
   `text` goes. */
inline std::string_view trim_blanks(std::string_view text)
{
  const std::string_view blanks=" \t\r\n\v\f";
  const size_t b=text.find_first_not_of(blanks);
  if(b==std::string_view::npos)return {};
  return text.substr(b,text.find_last_not_of(blanks)-b+1);
}

/* text, all of it, as a decimal number (strtod's grammar in the C locale:
   inf and nan too, no hexadecimal, no leading space); false when the text
   is not one number or is out of double's range (a subnormal is in it). Not std::from_chars:
   macOS's libc++ has no floating-point from_chars before macOS 26, and the
   release runs on 13.3 */
bool parse_number(std::string_view text, double &value);
/* text, all of it, as a whole number in int's range (an optional '-',
   then digits); false otherwise, value untouched */
bool parse_int(std::string_view text, int &value);
/* A named boolean, exactly true or false; no numeric choices. */
bool parse_bool(std::string_view text, bool &value);
/* A whole unsigned 64-bit decimal word, for portable generator states. */
bool parse_uint64(std::string_view text, std::uint64_t &value);

/* s in upper (lower) case, in place up to its NUL: ASCII letters only (the
   C locale's toupper/tolower), as the parser keeps a model's names;
   change_case turns the letters from..from+25 into to..to+25 */
inline void change_case(char *s, char from, char to)
{
  for(;*s;s++)
    if(*s>=from&&*s<=from+25)*s=static_cast<char>(*s-from+to);
}
inline void to_upper(char *s) { change_case(s,'a','A'); }
inline void to_lower(char *s) { change_case(s,'A','a'); }
/* s in upper case (ASCII letters only), all of it */
inline std::string upper_case(std::string s)
{
  for(char &c:s)
    if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
  return s;
}
/* s in lower case (ASCII letters only), all of it */
inline std::string lower_case(std::string s)
{
  to_lower(s.data());
  return s;
}

/* strtok's tokens without writing into the text (the core's one
   tokenizer: the parser, the options, the .ani reader, the fit's lists):
   next(delims) passes over the delimiters, returns the text up to the
   next one and passes over that one too, each call naming its own
   delimiters as strtok's did; nullopt once nothing is left. rest() is
   what follows the last token returned. */
class Tokens {
public:
  explicit Tokens(std::string_view text):rest_(text){}
  std::optional<std::string_view> next(std::string_view delims)
  {
    size_t b=rest_.find_first_not_of(delims);
    if(b==std::string_view::npos){
      rest_={};
      return std::nullopt;
    }
    rest_.remove_prefix(b);
    size_t e=rest_.find_first_of(delims);
    std::string_view tok=rest_.substr(0,e);
    rest_.remove_prefix(e==std::string_view::npos?rest_.size():e+1);
    return tok;
  }
  /* the next token as text, "" when there is none */
  std::string text(std::string_view delims)
  {
    return std::string{next(delims).value_or(std::string_view())};
  }
  std::string_view rest() const { return rest_; }
private:
  std::string_view rest_;
};

/* c starts a word (a letter or '_'), c is in one (a digit too): a model's
   names are words */
inline bool is_word_start(char c)
{
  return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';
}
inline bool is_word_char(char c)
{
  return is_word_start(c)||(c>='0'&&c<='9');
}
/* text with each of its words replaced by what f gives for it (a
   std::string for a std::string_view): runs of word characters that start
   with a letter or '_' (a formula's names and calls; the e of a number's
   exponent, 1e-3, is none). dotted: a word goes on through a '.' and a
   letter after it, a .odex name c1.v. */
template<class F>
std::string replace_words(std::string_view text, F f, bool dotted=false)
{
  std::string out;
  size_t i=0;
  while(i<text.size()){
    if(is_word_start(text[i])&&(i==0||!is_word_char(text[i-1]))){
      size_t b=i;
      for(;;){
        while(i<text.size()&&is_word_char(text[i]))i++;
        if(!dotted||i+1>=text.size()||text[i]!='.'||!is_word_start(text[i+1]))break;
        i++;
      }
      out+=f(text.substr(b,i-b));
    }
    else
      out+=text[i++];
  }
  return out;
}

/* the words of text in order, each as written (replace_words') */
inline std::vector<std::string> words_of(std::string_view text, bool dotted=false)
{
  std::vector<std::string> out;
  replace_words(text,[&out](std::string_view w){ out.emplace_back(w); return std::string(w); },dotted);
  return out;
}

/* ---- JSON strings -------------------------------------------------------
   The one place that reads or writes a JSON string's content (the core's
   text is UTF-8): the protocol (json_io.cpp's events and command objects)
   and the files module (xpp_files.cpp's /files listing and its "file"
   command) shared two near-copies of this before, one of them (buf_str)
   treating bytes above 0x7f as Latin-1 instead of decoding UTF-8, which
   sent a model's non-ASCII names back mangled (docs/protocol.md). */

/* Appends `s` (already-decoded UTF-8, as the core's own strings are)
   escaped as a JSON string's *content* to `out` -- no surrounding quotes,
   the caller adds those, since both callers build the quotes into a
   larger buffer around other text. A valid UTF-8 sequence in `s` passes
   through unchanged; '"', '\\' and the control characters are escaped
   (\n \t \r \b \f by name, the rest as \u00XX); a byte that is not part
   of a valid UTF-8 sequence -- and so is not text at all -- is escaped as
   \u00XX too, its own byte value, the same fallback buf_str always used. */
void json_encode_string(std::string &out, std::string_view s);

/* Appends `s` as a whole JSON string, quotes included (json_encode_string
   between them); a null `s` as the empty string "". The data events'
   text (plot_data, autox_data, ani_data, autox_settings) goes through this. */
void json_append_string(std::string &out, const char *s);
void json_append_string(std::string &out, std::string_view s);

/* Decodes the JSON string value at *v (which must point to its opening
   '"'): unescapes \", \\, \/, \n, \t, \r, \b, \f and \uXXXX (a surrogate
   pair combined into one code point, encoded to UTF-8) into `out`
   (cleared first), stopping at the closing '"' or the end of the input.
   At most `max` bytes are appended (SIZE_MAX for no limit); a command
   object's fixed-size destination passes its buffer size.

   `strict` is xpp_files' stricter rule for a file name: false for any
   other escape, control character (embedded raw or via \u), or unpaired
   surrogate, instead of the loose fallback (an unknown escape copied
   literally, a bad \u treated as U+FFFD) callers reading a command's
   ordinary text fields use. Returns false only when *v is not a '"' or,
   in strict mode, one of those rejects. */
bool json_decode_string(const char *v, std::string &out, size_t max, bool strict);

/* The digit a base64 character stands for (A-Z a-z 0-9 + /), -1 for any
   other: the page's uploads (xpp_files.cpp) and its pictures
   (json_windows.cpp) */
int base64_value(int c) noexcept;

/* Base64 (RFC 4648, padded): the one encoder and decoder. base64_append
   (below) encodes bytes all at once (the series event's float32 columns,
   series_enc.cpp; a file's bytes, xpp_files.cpp). Base64Decoder takes digits one at a time
   ('=' padding too; any other character makes feed() false) and appends
   the bytes to `out` (the page's uploads, xpp_files.cpp, a recording's
   binary files, recx.cpp); finish() writes the last bytes, padded or not, and is false for a
   dangling digit or too much padding. */
class Base64Decoder {
public:
    explicit Base64Decoder(std::string &out) : out_(out) {}
    bool feed(char c);
    bool finish();
private:
    std::string &out_;
    std::array<int, 4> q_{};
    int n_ = 0, pad_ = 0;
};
/* bytes as base64, appended to out */
void base64_append(std::string &out, std::string_view bytes);
/* the base64 text decoded, appended to out: false when it is not base64 */
bool base64_decode_append(std::string &out, std::string_view text);

} // namespace xpp
