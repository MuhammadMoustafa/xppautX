/* The kernel's pure helpers: the time stamp, white space, names shortened and labelled, the
   equilibrium's stability word, connected components. What needs a Session or a Model is
   session_values.cpp's (W726: the kernel includes nothing outside it). */
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "xpp_text.h"
#include "xpp_util.h"

namespace xpp {

void check_val(double *x1, double *x2, double *xb, double *xd)
{
  if (*x1 == *x2) {
    const double size = std::fabs(*x1);
    const double temp = .05 * (size > 1.0 ? size : 1.0); /* not std::max: a NaN gives 1, as the macro this was did */
    *x1 -= temp;
    *x2 += temp;
  }
  if (*x1 > *x2) std::swap(*x1, *x2);
  *xb = .5 * (*x1 + *x2);
  *xd = 2.0 / (*x2 - *x1);
}

std::string utc_now()
{
  return xpp::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

std::string name_label(std::string_view name)
{
    std::string label(name);
    std::replace(label.begin(), label.end(), '_', ' ');
    std::replace(label.begin(), label.end(), '.', ' ');
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    return label;
}

namespace {
/* Levenshtein distance of a and b: the fewest insertions, deletions and substitutions turning one into the other */
std::size_t edit_distance(std::string_view a, std::string_view b)
{
    std::vector<std::size_t> row(b.size() + 1);
    std::iota(row.begin(), row.end(), std::size_t{0});
    for (std::size_t i = 1; i <= a.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()];
}
} // namespace

std::vector<std::string> nearest_names(std::string_view name, const std::vector<std::string> &candidates, std::size_t count)
{
    /* a third of the name's length, at least one edit: a name needing more changes than that is a different name */
    const std::size_t limit = std::max<std::size_t>(1, name.size() / 3);
    std::vector<std::pair<std::size_t, std::string>> scored;
    for (const std::string &candidate : candidates) {
        const std::size_t distance = edit_distance(name, candidate);
        if (distance <= limit) scored.emplace_back(distance, candidate);
    }
    std::stable_sort(scored.begin(), scored.end(), [](const auto &x, const auto &y) { return x.first < y.first; });
    std::vector<std::string> out;
    for (std::size_t i = 0; i < scored.size() && i < count; ++i) out.push_back(scored[i].second);
    return out;
}

std::string short_name(std::string_view name, int width)
{
  const size_t w=width>0?static_cast<size_t>(width):0;
  if(name.size()<=w)
    return std::string(name);
  return w?std::string(name.substr(0,w-1))+'~':std::string();
}

std::vector<std::vector<int>> connected_components(const std::vector<std::vector<int>> &edges)
{
  const size_t n=edges.size();
  std::vector<int> parent(n);
  std::iota(parent.begin(),parent.end(),0);
  auto root=[&parent](int i){
    while(parent[static_cast<size_t>(i)]!=i){
      parent[static_cast<size_t>(i)]=parent[static_cast<size_t>(parent[static_cast<size_t>(i)])];
      i=parent[static_cast<size_t>(i)];
    }
    return i;
  };
  for(size_t i=0;i<n;i++)
    for(const int j:edges[i]){
      const int a=root(static_cast<int>(i)),b=root(j);
      if(a!=b)parent[static_cast<size_t>(std::max(a,b))]=std::min(a,b); /* the least node stays the root */
    }
  std::vector<std::vector<int>> components;
  std::vector<int> component(n,-1); /* by root */
  for(size_t i=0;i<n;i++){
    int &c=component[static_cast<size_t>(root(static_cast<int>(i)))];
    if(c<0){
      c=static_cast<int>(components.size());
      components.emplace_back();
    }
    components[static_cast<size_t>(c)].push_back(static_cast<int>(i));
  }
  return components;
}

void de_space(std::string &s)
{
  de_space(s.data());
  s.resize(strlen(s.c_str()));
}

void de_space(char *s)
{
  int n=strlen(s);
  int i,j=0;
  char ch;
  for(i=0;i<n;i++){
    ch=s[i];
    if(!isspace(ch)){
      s[j]=ch;
      j++;
    }
  }
  s[j]=0;
}

int has_eq(std::string_view z, std::string &name, int *where)
{
  size_t i=z.find(':');
  if(i==std::string_view::npos)return(0);
  name.assign(z.substr(0,i));
  *where=static_cast<int>(i)+1;
  return(1);
 }

/* cp/rp: complex/real eigenvalues with positive real part, im: imaginary */
const char *eq_stability(int cp, int rp, int im)
{
 if(cp>0||rp>0)return "UNSTABLE";
 else if(im>0)return "NEUTRAL";
 else return "STABLE";
}

} // namespace xpp
