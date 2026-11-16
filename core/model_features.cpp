/* The model's features (model_features.h): each one's presence is decided here, once. */
#include "model_features.h"
#include "tabular.h"
#include "model.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace xpp {

const FeatureInfo &feature_info(Feature f)
{
    return *std::ranges::find(FEATURES, f, &FeatureInfo::feature);
}

bool has_feature(const Model &m, const std::vector<TABULAR> &tables, Feature f)
{
    switch (f) {
    case Feature::none: return true;
    case Feature::parameters: return m.nupar > 0;
    case Feature::delays: return m.compiler.ndelays > 0;
    case Feature::boundary_conditions: return m.bc_defined > 0;
    case Feature::integral_terms: return m.compiler.nkernel > 0;
    case Feature::formula_tables: return !tables.empty();
    case Feature::output_columns: return !m.only.empty();
    }
    return true;
}

std::string model_features_json(const Model &m, const std::vector<TABULAR> &tables)
{
    std::string out = "[";
    for (const FeatureInfo &info : FEATURES) {
        if (out.size() > 1) out += ',';
        out += "{\"name\":";
        json_append_string(out, info.name);
        out += ",\"present\":";
        out += has_feature(m, tables, info.feature) ? "true" : "false";
        out += ",\"needs\":";
        json_append_string(out, info.needs);
        out += '}';
    }
    return out + ']';
}

} // namespace xpp
