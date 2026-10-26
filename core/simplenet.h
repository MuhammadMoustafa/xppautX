#pragma once

#include "expr_program.h"
#include "xpp_error.h"
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Session; /* session.h */

/* what a network computes (the Model's networks, simplenet.cpp's table of
   forms): a convolution of a variable with a weight table (its ends:
   Ends), a sparse or full matrix product, a Fourier transform's
   convolution, the delayed products, a Gillespie chain, an interpolation
   and the extrema of a list of variables */
enum class NetworkKind { convolution, sparse, fft_convolution, matrix_product, delayed_matrix_product,
                         delayed_sparse, gillespie, interpolation, extremum };

/* how a convolution or a vector treats its ends */
enum class Ends { even, zero, periodic };

/* which extremum an extremum network finds */
enum class Extremum { both, maximum, minimum };
inline constexpr std::array<std::string_view, 3> extremum_names{"both", "maximum", "minimum"};
constexpr std::optional<Extremum> extremum_from_xppaut(int code) {
    if(code < -1 || code > 1) return std::nullopt;
    return code==1 ? Extremum::maximum : code==-1 ? Extremum::minimum : Extremum::both;
}

/* whether s (blanks removed and upper-cased in place) is the call of a
   network: its keyword names one of simplenet.cpp's forms */
bool is_network(char *s);

/* vector(var,length,e|z|p,e|z|p) in str (blanks removed in place): its
   first variable (ses's), length and ends' kinds */
int get_vector_info(const Session &ses, char *str, std::string_view name, int *root, int *length, Ends *il, Ends *ir);

int add_vectorizer(Session &s, std::string_view name, char *rhs);
void add_vectorizer_name(Session &s, std::string_view name, std::string_view rhs);
double vector_value(Session &s, double x, int i);
double network_value(Session &s, double x, int i);
/* the network name's call rhs: its Model network is filled in (an error is thrown); false when rhs is no network */
bool add_spec_fun(Session &s, std::string_view name, char *rhs);
/* table index is about to take length values (a re-read): the first network
   that reads it needs more is the error, at the network's line; nothing is
   changed by the check */
xpp::Result<> check_network_table_length(const Session &s, int index, long long length);
void add_special_name(Session &s, std::string_view name, char *rhs);
void eval_all_nets(Session &s);
void evaluate_network(Session &s, int ind);
void update_all_ffts(Session &s);
void update_fft(Session &s, int ind);

/* a network (a special function: conv, sparse, fftcon, gill, ...) as
   the model defines it, xpp::Model's (model.h): its kind, sizes, roots
   and compiled f(root,root2) (applies_function: each tap is f(root+unit,
   root2+source) instead of the source's value), and the tables whose
   values are its weights, indices and delays */
struct Network {
  NetworkKind kind=NetworkKind::convolution;
  Ends ends=Ends::even; /* a convolution's */
  Extremum extremum=Extremum::both; /* an extremum network's */
  int gill_method=0; /* a gillespie chain's evaluation method (0: the exact one) */
  int ncon=0,n=0;
  std::string name;
  int root=0,root2=0;
  bool applies_function=false;
  Program f;
  std::vector<int> gcom; /* a gillespie chain's commands */
  /* the tables (Session::tables) of its weights, indices and delays, -1
     for none */
  int weight_table=-1,index_table=-1,taud_table=-1;
};

/* what a network computed last and its work space, a Session's
   (session.h): its values, a fftcon's kernel transform (update_fft) and
   the convolution's scratch, a gillespie chain's weights (made on its
   first step, make_gill_nu) */
struct NetworkValues {
  std::vector<double> values;
  std::vector<double> fftr,ffti,dr,di;
  std::vector<double> gill_nu;
  std::vector<double> gill_rates;
  bool gill_ready=false;
};

/* a vectorizer (a variable's neighbours as one name), xpp::Model's */
struct Vectorizer {
  std::string name;
  int root=0,length=0;
  Ends il=Ends::periodic,ir=Ends::periodic;
};

} // namespace xpp
