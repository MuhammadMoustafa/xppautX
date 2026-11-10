#pragma once
/* What a model has that some settings and commands need (W649): the one list of its features, computed from the loaded model in one
   function. An option row (OptionRow::needs) and a command row (Command::needs) name the feature they need; hello gives the list
   (`model_features`), the page hides what the model lacks and Search commands lists it greyed with the reason, and the core refuses
   a command whose feature the model lacks, with the same reason. */
#include <array>
#include <string>
#include <string_view>

namespace xpp {

struct Session; /* session.h */

/* none first: a row that needs no feature */
enum class Feature { none, delays, boundary_conditions, integral_terms, formula_tables };

struct FeatureInfo {
    Feature feature;
    std::string_view name; /* hello's and the .odex-side word, snake_case */
    std::string_view needs; /* why a row needing it is not for a model without it, said once */
};

inline constexpr std::array FEATURES = {
    FeatureInfo{Feature::delays, "delays", "needs a model with delays"},
    FeatureInfo{Feature::boundary_conditions, "boundary_conditions", "needs a model with boundary conditions"},
    FeatureInfo{Feature::integral_terms, "integral_terms", "needs a model with integral equations"},
    FeatureInfo{Feature::formula_tables, "formula_tables", "needs a model with tables"}};

const FeatureInfo &feature_info(Feature f);
/* whether the session's model has the feature (Feature::none: always) */
bool has_feature(const Session &s, Feature f);
/* hello's "model_features": every feature as {name, present, needs} */
std::string model_features_json(const Session &s);

} // namespace xpp
