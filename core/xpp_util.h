#pragma once


#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>


namespace xpp {

/* now in UTC as 2026-09-30T10:14:02Z: when a recording begins, when a file of
   values was saved */
std::string utc_now();

/* s without its white space, in place */
void de_space(char *s);

/* an equilibrium's stability from its eigenvalues' counts: UNSTABLE,
   NEUTRAL or STABLE */
const char *eq_stability(int cp, int rp, int im);


/* name, shortened for a fixed-width display of width characters: a longer
   one keeps its start and ends in '~' so it cannot pass for another name */
std::string short_name(std::string_view name, int width);

/* The connected components of a graph of n nodes (W433), `edges[i]` the
   nodes node i is joined to (an edge counts both ways): each component's
   nodes ascending, the components by their least node. By union-find. */
std::vector<std::vector<int>> connected_components(const std::vector<std::vector<int>> &edges);

/* a name's label: its underscores as spaces and a capital first letter
   ("mesh_intervals" is "Mesh intervals"), derived and never written apart
   (W381; web2's choiceLabel is the page's) */
std::string name_label(std::string_view name);


/* s without its white space (de_space's, up to a NUL in it) */
void de_space(std::string &s);

/* "name:formula" (do_calc's "set this name to that"): 1 with name the
   text before the first ':' (of any length) and *where the formula's
   start; 0 when z has no ':' */
int has_eq(std::string_view z, std::string &name, int *where);

/* a fingerprint of values, to tell whether they are what they were
   without keeping a copy (a phase record against what the client got, a
   session against what was saved): 64-bit FNV-1a over 8-byte words of its
   bits (a flow's breaks are NaN), so a change goes unseen with odds of
   one in 2^64 */
class Fingerprint {
public:
    void bytes(const void *p, std::size_t n)
    {
        const unsigned char *b = static_cast<const unsigned char *>(p);
        for (; n >= sizeof(std::uint64_t); n -= sizeof(std::uint64_t), b += sizeof(std::uint64_t)) {
            std::uint64_t w;
            std::memcpy(&w, b, sizeof w);
            mix(w);
        }
        for (; n > 0; n--, b++) mix(*b);
    }
    template <class T> void value(const T &v) { bytes(&v, sizeof v); }
    void floats(const std::vector<float> &v)
    {
        value(v.size());
        bytes(v.data(), v.size() * sizeof(float));
    }
    std::uint64_t result() const { return h_; }
private:
    void mix(std::uint64_t w) { h_ = (h_ ^ w) * 1099511628211ULL; /* the FNV 64-bit prime */ }
    std::uint64_t h_ = 14695981039346656037ULL; /* the FNV 64-bit offset basis */
};

} // namespace xpp
