// fusion expression: the settings of cudf and suggest_thresholds, keys expression.<name> (section 04,
// "Settings" and "Batch commands"; 1.16; 09's registry). Section 01's command table binds the batch
// arguments to them: cudf's five positionals are values, up, down, up_out and down_out (S3.01).
#include "fusion/expression/expression.hpp"

namespace fusion::expression {

namespace {

constexpr std::string_view module = "expression";
constexpr std::string_view repeat_rules[] = {"classify_each", "max_abs"};

constexpr core::SettingSpec table[] = {
    {.name = "values", .module = module, .type = core::Type::input_file, .need = core::Need::required,
     .help = "the per-gene value file: id and value per line (S3.03), or a table with value_column (04.C1)"},
    {.name = "up", .module = module, .type = core::Type::real, .need = core::Need::required,
     .help = "a gene is up when its value >= up; inclusive, and above down (S3.06, S3.07)"},
    {.name = "down", .module = module, .type = core::Type::real, .need = core::Need::required,
     .help = "a gene is down when its value <= down; inclusive, and below up (S3.06, S3.07)"},
    {.name = "up_out", .module = module, .type = core::Type::output_name, .need = core::Need::defaulted,
     .default_text = "up_reg", .affects_results = false,
     .help = "the up list, one id per line; required in the positional form (S3.01, 04.C11)"},
    {.name = "down_out", .module = module, .type = core::Type::output_name, .need = core::Need::defaulted,
     .default_text = "down_reg", .affects_results = false,
     .help = "the down list, one id per line; required in the positional form (S3.01, 04.C11)"},
    {.name = "id_column", .module = module, .type = core::Type::text, .need = core::Need::optional,
     .help = "the table's id column, by header name; the first column when left out (04.C1)"},
    {.name = "value_column", .module = module, .type = core::Type::text, .need = core::Need::optional,
     .help = "the table's value column, by header name; giving it reads the file as a table (04.C1)"},
    {.name = "id_map", .module = module, .type = core::Type::input_file, .need = core::Need::optional,
     .help = "this input's own id map: an S1.16 file, or the keyword symbols; ids it lacks are dropped (04.C1)"},
    {.name = "repeats", .module = module, .type = core::Type::choice, .need = core::Need::defaulted,
     .default_text = "classify_each", .choices = repeat_rules,
     .help = "classify_each: every line of a repeated id is classified (S3.08); max_abs: its largest "
             "magnitude only (04.C3)"},
    {.name = "not_de", .module = module, .type = core::Type::input_file, .need = core::Need::optional,
     .help = "ids set to none before classification, one per line (S8.15, 04.C4)"},
    {.name = "current_up", .module = module, .type = core::Type::real, .need = core::Need::optional,
     .help = "the up threshold in use, shown as the current row; with current_down (S3.15)"},
    {.name = "current_down", .module = module, .type = core::Type::real, .need = core::Need::optional,
     .help = "the down threshold in use, shown as the current row; with current_up (S3.15)"},
    {.name = "plot", .module = module, .type = core::Type::output_name, .need = core::Need::optional,
     .affects_results = false, .help = "the histogram as a PNG; needs gnuplot 5.4 with pngcairo (S3.19, 04.C7)"},
};

}  // namespace

std::span<const core::SettingSpec> settings() { return table; }

}  // namespace fusion::expression
