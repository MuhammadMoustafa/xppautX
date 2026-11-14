/* The kernel's pure helpers: the time stamp, white space, names shortened and labelled, the
   equilibrium's stability word, connected components. What needs a Session or a Model is
   session_values.cpp's (W726: the kernel includes nothing outside it). */
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_text.h"
#include "xpp_util.h"

namespace xpp {

std::string utc_now()
{
  return xpp::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

std::string name_label(std::string_view name)
{
    std::string label(name);
    std::replace(label.begin(), label.end(), '_', ' ');
    if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    return label;
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
