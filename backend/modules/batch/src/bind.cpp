// The binder: positional and named tokens to typed values (decision 0018, 01.C1).
// FuSION 1 read token_cmd[k] by position from a 1,000,000-slot vector, so a missing argument read ""
// and stoi/stof threw std::invalid_argument out of main (Linux/src/main.cc:1154-1156, S1.05, S1.28).
// 2.0 counts the tokens, tells a missing argument from an empty one, and checks every value's type.
#include <algorithm>
#include <charconv>
#include <cmath>

#include "internal.hpp"

namespace fusion::batch {

// ------------------------------------------------------------------ shared helpers

void add_diagnostic(core::CheckReport& report, core::Severity severity, int line, int column,
                    std::string_view code, std::string message, std::string_view command,
                    std::string_view setting, std::string_view path) {
  core::Diagnostic d;
  d.severity = severity;
  d.line = static_cast<std::uint32_t>(std::max(line, 0));
  d.column = static_cast<std::uint32_t>(std::max(column, 0));
  d.code = std::string(code);
  d.message = std::move(message);
  if (!command.empty()) d.command = std::string(command);
  if (!setting.empty()) d.setting = std::string(setting);
  if (!path.empty()) d.path = std::string(path);
  report.diagnostics.push_back(std::move(d));
}

std::string join(std::span<const std::string> items, std::string_view separator) {
  std::string out;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i) out += separator;
    out += items[i];
  }
  return out;
}

std::string join_views(std::span<const std::string_view> items, std::string_view separator) {
  std::string out;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i) out += separator;
    out += items[i];
  }
  return out;
}

std::string_view type_name(core::Type type) noexcept {
  switch (type) {
    case core::Type::boolean: return "boolean";
    case core::Type::integer: return "integer";
    case core::Type::real: return "real";
    case core::Type::duration: return "duration";
    case core::Type::text: return "text";
    case core::Type::choice: return "choice";
    case core::Type::input_file: return "input_file";
    case core::Type::input_list: return "input_list";
    case core::Type::output_name: return "output_name";
  }
  return "?";
}

std::string_view need_name(core::Need need) noexcept {
  switch (need) {
    case core::Need::required: return "required";
    case core::Need::optional: return "optional";
    case core::Need::defaulted: return "defaulted";
    case core::Need::computed: return "computed";
  }
  return "?";
}

std::string_view source_name(core::Source source) noexcept {
  switch (source) {
    case core::Source::default_value: return "default";
    case core::Source::positional: return "positional";
    case core::Source::named: return "named";
    case core::Source::set_option: return "set";
    case core::Source::run_line: return "run_line";
  }
  return "?";
}

std::string generic(const std::filesystem::path& p) {
  return core::to_utf8(p.lexically_normal());
}

// 01.C2: relative input paths resolve against the batch file's directory, not the launch directory.
std::filesystem::path resolve_input(const std::filesystem::path& base, std::string_view text) {
  std::filesystem::path p = core::path_from_utf8(text);
  if (p.is_absolute() || base.empty()) return p.lexically_normal();
  return (base / p).lexically_normal();
}

// ------------------------------------------------------------------ values

namespace {

bool parse_int(std::string_view t, std::int64_t& out) {
  if (t.empty()) return false;
  std::string_view digits = t;
  if (digits.front() == '+') digits.remove_prefix(1);                      // from_chars takes no '+'
  if (digits.empty() || digits.front() == '+') return false;
  const char* first = digits.data();
  const char* last = digits.data() + digits.size();
  const auto [ptr, ec] = std::from_chars(first, last, out);
  return ec == std::errc() && ptr == last;
}

std::string lower_ascii(std::string_view t) {
  std::string out(t);
  for (auto& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

std::string range_text(const core::SettingSpec& spec) {
  std::string out;
  if (spec.min != -std::numeric_limits<double>::infinity()) out += ">= " + core::format_number(spec.min);
  if (spec.max != std::numeric_limits<double>::infinity()) {
    if (!out.empty()) out += " and ";
    out += "<= " + core::format_number(spec.max);
  }
  return out;
}

bool in_range(const core::SettingSpec& spec, double v) { return v >= spec.min && v <= spec.max; }

// A duration: bare milliseconds in FuSION 1's positional slots; a unit in named form (01.C1).
bool parse_duration(std::string_view t, core::Source source, std::int64_t& ms, std::string& error) {
  if (source == core::Source::positional) {
    if (!parse_int(t, ms) || ms < 0) { error = "is not a whole number of milliseconds"; return false; }
    return true;
  }
  if (t == "0") { ms = 0; return true; }
  std::size_t i = 0;
  while (i < t.size() && t[i] >= '0' && t[i] <= '9') ++i;
  std::int64_t n = 0;
  if (i == 0 || !parse_int(t.substr(0, i), n)) {
    error = "is not a duration such as 4h, 50min, 90s or 500ms";
    return false;
  }
  const auto unit = t.substr(i);
  std::int64_t factor = 0;
  if (unit == "ms") factor = 1;
  else if (unit == "s") factor = 1000;
  else if (unit == "min") factor = 60'000;
  else if (unit == "h") factor = 3'600'000;
  else if (unit == "d") factor = 86'400'000;
  else {
    error = unit.empty() ? "needs a unit (ms, s, min, h or d); bare milliseconds are for FuSION 1's positions"
                         : "has an unknown unit '" + std::string(unit) + "' (ms, s, min, h, d)";
    return false;
  }
  if (n > std::numeric_limits<std::int64_t>::max() / factor) { error = "is too long"; return false; }
  ms = n * factor;
  return true;
}

}  // namespace

// One token of its type (01.C1, S3.02, S5.02): a whole token, or an error that says why.
std::optional<core::Value> parse_value(const core::SettingSpec& spec, std::string_view text, core::Source source,
                                       std::string& error) {
  switch (spec.type) {
    case core::Type::integer: {
      std::int64_t v = 0;
      if (!parse_int(text, v)) { error = "is not a whole number"; return std::nullopt; }
      if (!in_range(spec, static_cast<double>(v))) { error = "must be " + range_text(spec); return std::nullopt; }
      return core::Value(v);
    }
    case core::Type::real: {
      const auto d = core::parse_decimal(text);   // 04.C2's grammar through fast_float (11.C5)
      if (d.status != core::DecimalStatus::ok) {
        error = d.status == core::DecimalStatus::not_finite ? "is not a finite number" : "is not a number";
        return std::nullopt;
      }
      if (!in_range(spec, d.value)) { error = "must be " + range_text(spec); return std::nullopt; }
      return core::Value(d.value);
    }
    case core::Type::boolean: {
      const auto t = lower_ascii(text);
      if (t == "y" || t == "yes" || t == "true" || t == "1") return core::Value(true);
      if (t == "n" || t == "no" || t == "false" || t == "0") return core::Value(false);
      error = "is not y, yes, true, 1, n, no, false or 0";
      return std::nullopt;
    }
    case core::Type::duration: {
      std::int64_t ms = 0;
      if (!parse_duration(text, source, ms, error)) return std::nullopt;
      if (!in_range(spec, static_cast<double>(ms))) { error = "must be " + range_text(spec); return std::nullopt; }
      return core::Value(ms);
    }
    case core::Type::choice: {
      for (const auto c : spec.choices)
        if (c == text) return core::Value(std::string(text));
      // FuSION 1's positions may give the choice's number (05.C8: 0, 1, 2 for edges_to_target).
      std::int64_t n = 0;
      if (source == core::Source::positional && parse_int(text, n) && n >= 0 &&
          static_cast<std::size_t>(n) < spec.choices.size())
        return core::Value(std::string(spec.choices[static_cast<std::size_t>(n)]));
      if (spec.choices.empty()) return core::Value(std::string(text));
      error = "is not one of " + join_views(spec.choices, ", ");
      return std::nullopt;
    }
    case core::Type::text:
    case core::Type::input_file:
    case core::Type::input_list:
    case core::Type::output_name:
      return core::Value(std::string(text));
  }
  error = "has an unknown type";
  return std::nullopt;
}

std::string value_text(const core::Value& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::int64_t>) return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>) return core::format_number(v);
        else if constexpr (std::is_same_v<T, std::string>) return v;
        else return join(v, ",");
      },
      value);
}

// 01.C2, for the batch files 2.0 writes: '$' is "$$"; a value with a space, a tab or '"', or an empty
// one, is quoted, with '"' doubled inside.
std::string quote_value(std::string_view value) {
  std::string escaped;
  for (const char c : value) {
    if (c == '$') escaped += "$$";
    else escaped += c;
  }
  const bool quote = value.empty() || value.find_first_of(" \t\"") != std::string_view::npos;
  if (!quote) return escaped;
  std::string out = "\"";
  for (const char c : escaped) {
    if (c == '"') out += "\"\"";
    else out += c;
  }
  return out + "\"";
}

const BoundValue& BoundArgs::at(std::string_view name) const {
  for (const auto& v : values)
    if (v.spec && v.spec->name == name) return v;
  throw core::Failure("internal", std::string(command ? command->name : "?") + " has no argument " +
                                      std::string(name));
}

std::string BoundArgs::normalised() const {
  std::string out(command ? command->name : "");
  for (const auto& v : values) {
    if (!v.given || !v.has_value) continue;
    out += ' ';
    out += v.spec->name;
    out += '=';
    out += quote_value(value_text(v.value));
  }
  return out;
}

// ------------------------------------------------------------------ binding

namespace {

struct Pending {                 // a value as written, before its type is checked
  std::string text;              // expanded; for a list, the elements joined by ','
  std::vector<std::string> items;   // a positional list's elements
  bool list_from_slots = false;
  int column = 0;
  core::Source source = core::Source::positional;
};

const ArgSpec* find_arg(const CommandSpec& spec, std::string_view name, bool by_name) {
  for (const auto& a : spec.args)
    if (a.name == name && (!by_name || !a.positional_only)) return &a;
  return nullptr;
}

std::vector<std::string> split_commas(std::string_view text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const auto comma = text.find(',', start);
    out.emplace_back(text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
    if (comma == std::string_view::npos) break;
    start = comma + 1;
  }
  return out;
}

}  // namespace

BoundArgs bind(const CommandSpec& spec, const Line& line, const Variables& vars, core::CheckReport& report) {
  return bind(spec, line, vars, report, {});
}

BoundArgs bind(const CommandSpec& spec, const Line& line, const Variables& vars, core::CheckReport& report,
               const std::filesystem::path& base) {
  BoundArgs args;
  args.command = &spec;
  args.line = line.number;
  args.values.resize(spec.args.size());
  for (std::size_t i = 0; i < spec.args.size(); ++i) {
    args.values[i].spec = &spec.args[i];
    args.values[i].source = core::Source::default_value;
  }
  const std::string_view cmd = spec.name;
  auto error = [&](int column, std::string_view code, std::string message, std::string_view setting = {}) {
    add_diagnostic(report, core::Severity::error, line.number, column, code, std::string(cmd) + ": " + message,
                   cmd, setting);
  };
  auto expand = [&](const Token& token, std::string_view text) {
    auto e = vars.expand(text, Variables::Mode::argument);
    for (const auto& name : e.unmatched)
      add_diagnostic(report, core::Severity::warning, line.number, token.column, "batch.undefined_variable",
                     name + " matches no defined variable", cmd);
    return std::move(e.text);
  };

  // Positional tokens first, then named ones in any order (01.C1).
  struct Positional { std::string text; int column; };
  std::vector<Positional> positional;
  std::vector<std::pair<const Token*, std::string>> named;   // token, expanded value
  bool seen_named = false, misplaced = false;
  for (std::size_t t = 1; t < line.tokens.size(); ++t) {
    const Token& token = line.tokens[t];
    if (token.name_end) {
      seen_named = true;
      named.emplace_back(&token, expand(token, std::string_view(token.text).substr(*token.name_end + 1)));
    } else if (seen_named) {
      if (!misplaced)
        error(token.column, "batch.bad_argument", "positional token '" + token.text + "' after a named one");
      misplaced = true;
    } else {
      positional.push_back({expand(token, token.text), token.column});
    }
  }

  std::vector<std::optional<Pending>> pending(spec.args.size());
  std::vector<bool> mentioned(spec.args.size(), false);   // given, even if empty
  auto index_of = [&](const ArgSpec* a) { return static_cast<std::size_t>(a - spec.args.data()); };

  if (!positional.empty()) {
    std::vector<std::string> all{std::string(cmd)};
    for (const auto& p : positional) all.push_back(p.text);
    const PositionalForm* form = nullptr;
    for (const auto& f : spec.forms)
      if (!f.fits || f.fits(all)) { form = &f; break; }
    if (!form) {
      std::string names;
      for (const auto& a : spec.args)
        if (!a.positional_only) names += (names.empty() ? "" : " ") + std::string(a.name) + "=";
      error(positional.front().column, "batch.bad_argument",
            "takes no positional arguments" + (names.empty() ? std::string() : "; name them: " + names));
    } else {
      args.form = form;
      std::size_t k = 0;
      bool optional = false, stop = false;
      std::size_t required_slots = 0, total_slots = 0;
      for (const auto& slot : form->slots) {
        if (slot.kind == SlotKind::optional_tail) { optional = true; continue; }
        ++total_slots;
        if (!optional) ++required_slots;
      }
      optional = false;
      for (const auto& slot : form->slots) {
        if (stop) break;
        switch (slot.kind) {
          case SlotKind::optional_tail:
            optional = true;
            break;
          case SlotKind::one: {
            if (k >= positional.size()) {
              if (!optional) {
                error(0, "batch.bad_argument",
                      "expected " + std::to_string(required_slots) + " positional arguments, got " +
                          std::to_string(positional.size()));
              }
              stop = true;
              break;
            }
            const auto& tok = positional[k++];
            const ArgSpec* a = find_arg(spec, slot.arg, false);
            if (!a) break;
            mentioned[index_of(a)] = true;
            // S5.02: each position consumes its token; an empty one keeps only that slot's default.
            if (tok.text.empty() || (!slot.none_token.empty() && tok.text == slot.none_token)) break;
            Pending p;
            p.text = tok.text;
            p.column = tok.column;
            p.source = core::Source::positional;
            pending[index_of(a)] = std::move(p);
            break;
          }
          case SlotKind::list:
          case SlotKind::ignored_list: {
            const int column = k < positional.size() ? positional[k].column : 0;
            std::vector<std::string> items;
            bool terminated = false;
            while (k < positional.size()) {
              const auto& tok = positional[k++];
              if (tok.text == "-1") { terminated = true; break; }
              if (!tok.text.empty()) items.push_back(tok.text);
            }
            if (!terminated) {   // S4.01: a -1 list without its terminator is an error
              error(column, "batch.bad_argument",
                    "the list " + (slot.arg.empty() ? std::string("of ignored tokens") : "'" + std::string(slot.arg) + "'") +
                        " has no -1 terminator");
              stop = true;
              break;
            }
            if (slot.kind == SlotKind::ignored_list) break;
            const ArgSpec* a = find_arg(spec, slot.arg, false);
            if (!a) break;
            mentioned[index_of(a)] = true;
            Pending p;
            p.items = std::move(items);
            p.list_from_slots = true;
            p.text = join(p.items, ",");
            p.column = column;
            p.source = core::Source::positional;
            pending[index_of(a)] = std::move(p);
            break;
          }
          case SlotKind::ignored:
            if (k < positional.size()) ++k;
            break;
        }
      }
      if (!stop && k < positional.size())
        error(positional[k].column, "batch.bad_argument",
              "expected " + (required_slots == total_slots ? std::to_string(total_slots)
                                                           : std::to_string(required_slots) + " to " +
                                                                 std::to_string(total_slots)) +
                  " positional arguments, got " + std::to_string(positional.size()) + " (extra tokens from '" +
                  positional[k].text + "')");
      if (!form->warning.empty())
        add_diagnostic(report, core::Severity::warning, line.number, positional.front().column,
                       "batch.ignored_tokens", std::string(form->warning), cmd);
    }
  }

  for (const auto& [token, value] : named) {
    const std::string name = token->text.substr(0, *token->name_end);
    const ArgSpec* a = find_arg(spec, name, true);
    if (!a) {
      std::string names;
      for (const auto& arg : spec.args)
        if (!arg.positional_only) names += (names.empty() ? "" : ", ") + std::string(arg.name);
      error(token->column, "batch.bad_argument",
            "unknown argument '" + name + "'" +
                (names.empty() ? std::string("; it takes none") : "; its arguments are " + names));
      continue;
    }
    const auto i = index_of(a);
    if (mentioned[i]) {
      error(token->column, "batch.bad_argument",
            pending[i] && pending[i]->source == core::Source::positional
                ? "'" + name + "' is already given by position"
                : "'" + name + "' is given twice",
            a->name);
      continue;
    }
    mentioned[i] = true;
    if (value.empty()) continue;   // 01.C1: an empty value means "not given"
    Pending p;
    p.text = value;
    p.column = token->column;
    p.source = core::Source::named;
    pending[i] = std::move(p);
  }

  // Types (S3.02, S5.02): every value must be a whole token of its type.
  for (std::size_t i = 0; i < spec.args.size(); ++i) {
    const ArgSpec& a = spec.args[i];
    BoundValue& v = args.values[i];
    const core::SettingSpec* s = find_setting(a.setting);
    if (!s) {
      error(0, "internal", "argument " + std::string(a.name) + " names no setting " + std::string(a.setting));
      continue;
    }
    if (!pending[i]) {
      if (s->need == core::Need::defaulted) {
        v.text = std::string(s->default_text);
        std::string why;
        if (a.list) {
          std::vector<std::string> items;
          if (!s->default_text.empty()) items = split_commas(s->default_text);
          v.value = std::move(items);
          v.has_value = true;
        } else if (auto parsed = parse_value(*s, s->default_text, core::Source::named, why)) {
          v.value = std::move(*parsed);
          v.has_value = true;
        }
      } else if (s->need == core::Need::required && !a.takes) {
        error(0, "batch.bad_argument", "missing argument '" + std::string(a.name) + "'", a.name);
      }
      continue;
    }
    Pending& p = *pending[i];
    v.given = true;
    v.source = p.source;
    v.column = p.column;
    v.text = p.text;
    std::vector<std::string> items;
    if (a.list) items = p.list_from_slots ? p.items : split_commas(p.text);
    else items.push_back(p.text);
    std::vector<std::string> converted;
    bool ok = true;
    for (const auto& item : items) {
      if (item.empty()) {
        error(p.column, "batch.bad_value", "'" + std::string(a.name) + "' has an empty item in '" + p.text + "'",
              a.name);
        ok = false;
        break;
      }
      std::string why;
      auto parsed = parse_value(*s, item, p.source, why);
      if (!parsed) {
        error(p.column, "batch.bad_value", std::string(a.name) + " '" + item + "' " + why, a.name);
        ok = false;
        break;
      }
      const bool symbols = a.name == "id_map" && item == "symbols";   // the word, not a file (04.C1, 1.12)
      if ((s->type == core::Type::input_file || s->type == core::Type::input_list) && !symbols)
        parsed = core::Value(generic(resolve_input(base, item)));
      if (a.list) converted.push_back(value_text(*parsed));
      else v.value = std::move(*parsed);
    }
    if (!ok) { v.given = false; continue; }
    if (a.list) v.value = std::move(converted);
    v.has_value = true;
  }
  return args;
}

}  // namespace fusion::batch
