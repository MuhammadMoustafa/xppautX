/* The model's features (model_features.h): each one's presence is decided here, once. */
#include "model_features.h"
#include "xpp_io.h"
#include "session.h"
#include "model.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace xpp {

const FeatureInfo &feature_info(Feature f)
{
    return *std::ranges::find(FEATURES, f, &FeatureInfo::feature);
}

bool has_feature(const Session &s, Feature f)
{
    const Model &m = s.model();
    switch (f) {
    case Feature::none: return true;
    case Feature::parameters: return m.nupar > 0;
    case Feature::delays: return m.ndelays > 0;
    case Feature::boundary_conditions: return m.bc_defined > 0;
    case Feature::integral_terms: return m.nkernel > 0;
    case Feature::formula_tables: return !s.tables.empty();
    }
    return true;
}

std::string model_features_json(const Session &s)
{
    std::string out = "[";
    for (const FeatureInfo &info : FEATURES) {
        if (out.size() > 1) out += ',';
        out += "{\"name\":";
        json_append_string(out, info.name);
        out += ",\"present\":";
        out += has_feature(s, info.feature) ? "true" : "false";
        out += ",\"needs\":";
        json_append_string(out, info.needs);
        out += '}';
    }
    return out + ']';
}

} // namespace xpp
