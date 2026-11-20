// What fusion --version --json prints, and what run.json records beside it (section 11, "Versions in
// run.json"; E1, S7.02; cross-section item 1.14): versions, tools, platform, install, environment, and the
// bundled data files with the reference maps' source and date (10's start-up check, S8.31).
#include <sys/utsname.h>

#include <cstdlib>
#include <fstream>
#include <thread>

#include "fusion/core/build_info.hpp"
#include "fusion/core/install.hpp"
#include "state.hpp"

namespace fusion::batch {

namespace {

core::Json env(const char* name) {
  const char* v = std::getenv(name);
  return v && *v ? core::Json(std::string(v)) : core::Json(nullptr);
}

std::string first_value(const std::filesystem::path& file, std::string_view key, char separator) {
  std::ifstream in(file);
  for (std::string line; std::getline(in, line);) {
    if (!line.starts_with(key)) continue;
    const auto at = line.find(separator);
    if (at == std::string::npos) continue;
    std::string value = line.substr(at + 1);
    const auto b = value.find_first_not_of(" \t\"");
    const auto e = value.find_last_not_of(" \t\"");
    return b == std::string::npos ? std::string() : value.substr(b, e - b + 1);
  }
  return {};
}

const std::filesystem::path* home_or_null(std::string& error) {
  try {
    return &core::install_dir().home;
  } catch (const std::exception& e) {
    error = e.what();
    return nullptr;
  }
}

}  // namespace

core::Json versions_block() {
  const auto& b = core::build_info();
  core::Json libraries = core::Json::object();
  libraries["z3"] = solve::z3_version();
  for (const auto& [name, version] : b.libraries) libraries[std::string(name)] = std::string(version);
  return core::Json{{"fusion", {{"version", std::string(b.version)}, {"git_commit", std::string(b.git_commit)},
                                {"git_dirty", b.git_dirty}, {"build_type", std::string(b.build_type)},
                                {"compiler", std::string(b.compiler)}, {"target", std::string(b.target)}}},
                    {"libraries", std::move(libraries)}};
}

core::Json platform_block() {
  core::Json p = core::Json::object();
  std::string os = first_value("/etc/os-release", "PRETTY_NAME=", '=');
  struct utsname u{};
  const bool named = ::uname(&u) == 0;
  p["os"] = os.empty() ? (named ? core::Json(std::string(u.sysname)) : core::Json(nullptr)) : core::Json(os);
  p["kernel"] = named ? core::Json(std::string(u.release)) : core::Json(nullptr);
  p["arch"] = named ? core::Json(std::string(u.machine)) : core::Json(nullptr);
  const std::string cpu = first_value("/proc/cpuinfo", "model name", ':');
  p["cpu"] = cpu.empty() ? core::Json(nullptr) : core::Json(cpu);
  p["logical_cpus"] = std::thread::hardware_concurrency();
  return p;
}

core::Json install_block() {
  try {
    const auto& dir = core::install_dir();
    return core::Json{{"prefix", core::to_utf8(dir.prefix)}, {"home", core::to_utf8(dir.home)},
                      {"source", dir.source == core::InstallDir::Source::env_override ? "FUSION_HOME" : "executable"}};
  } catch (const std::exception& e) {
    return core::Json{{"prefix", nullptr}, {"home", nullptr}, {"source", nullptr}, {"error", e.what()}};
  }
}

core::Json environment_block() {
  return core::Json{{"FUSION_HOME", env("FUSION_HOME")}, {"FUSION_GNUPLOT", env("FUSION_GNUPLOT")},
                    {"FUSION_DOT", env("FUSION_DOT")}, {"FUSION_CACHE_DIR", env("FUSION_CACHE_DIR")}};
}

// Each probe with a Probe's fields (section 11): only the tools a run used, in run.json.
core::Json probes_block(std::span<const core::deps::Probe> probes) {
  core::Json tools = core::Json::object();
  for (const auto& p : probes) {
    core::Json t{{"ok", p.ok}, {"version", p.version}, {"origin", p.origin}, {"features", p.features},
                 {"needed_by", p.needed_by}, {"path", core::to_utf8(p.path)}};
    if (!p.missing.empty()) t["missing"] = p.missing;
    if (!p.message.empty()) t["message"] = p.message;
    tools[p.id] = std::move(t);
  }
  return tools;
}

// The home's fusion-data.json (section 11): every listed file checked against its SHA-256, and the two
// reference maps with their source and date (S8.31, 10's start-up check). A development build without a
// staged home has no manifest: nothing is damaged, and the maps are reported missing.
DataCheck data_block() {
  DataCheck check;
  std::string error;
  const std::filesystem::path* home = home_or_null(error);
  core::Json files = core::Json::object();
  core::Json manifest_json;
  if (home) {
    std::ifstream in(*home / "fusion-data.json");
    if (in) {
      try {
        manifest_json = core::Json::parse(in);
      } catch (const std::exception& e) {
        check.damaged = true;
        error = "fusion-data.json cannot be read: " + std::string(e.what());
      }
    }
  }
  const bool listed = manifest_json.is_object() && manifest_json.contains("files");
  if (listed) {
    for (const auto& [rel, entry] : manifest_json["files"].items()) {
      const auto path = *home / core::path_from_utf8(rel);
      std::error_code ec;
      const bool present = std::filesystem::is_regular_file(path, ec);
      const std::string sha = present ? core::Sha256::of_file(path).hex() : std::string();
      const bool ok = present && (!entry.contains("sha256") || entry["sha256"] == sha);
      if (!ok) check.damaged = true;
      core::Json f{{"ok", ok}, {"path", core::to_utf8(path)}, {"sha256", sha}};
      if (present) f["bytes"] = std::filesystem::file_size(path, ec);
      for (const char* field : {"source", "date"}) f[field] = entry.contains(field) ? entry[field] : core::Json(nullptr);
      files[rel] = std::move(f);
    }
    if (manifest_json.contains("tools"))
      for (const auto& [id, rel] : manifest_json["tools"].items()) {
        std::error_code ec;
        if (!std::filesystem::exists(*home / core::path_from_utf8(rel.get<std::string>()), ec)) check.damaged = true;
      }
  }
  core::Json maps = core::Json::object();
  for (const auto& [name, rel] : {std::pair{"symbols", "data/hsa_to_gene_symbol_map.txt"},
                                  std::pair{"pathway_names", "data/hsa_path_to_path_name_map.txt"}}) {
    core::Json m{{"file", rel}};
    if (files.contains(rel)) {
      for (const char* field : {"ok", "path", "bytes", "sha256", "source", "date"})
        if (files[rel].contains(field)) m[field] = files[rel][field];
    } else {
      std::error_code ec;
      const bool present = home && std::filesystem::is_regular_file(*home / rel, ec);
      m["ok"] = present;
      m["path"] = home ? core::Json(core::to_utf8(*home / rel)) : core::Json(nullptr);
      m["source"] = nullptr;
      m["date"] = nullptr;
    }
    maps[name] = std::move(m);
  }
  check.json = core::Json{{"manifest", home && listed ? core::Json(core::to_utf8(*home / "fusion-data.json")) : core::Json(nullptr)},
                          {"ok", !check.damaged}, {"files", std::move(files)}, {"reference_maps", std::move(maps)}};
  if (!error.empty()) check.json["error"] = error;
  return check;
}

// The probe of every tool an installation provides (section 11's --version).
std::vector<core::deps::Probe> installation_probes() {
  core::deps::Registry tools;
  tools.require({.id = "sh"});
  tools.require({.id = "gnuplot", .min_version = "5.4", .features = {"pngcairo", "svg"}});
  tools.require({.id = "font:Liberation Sans", .kind = core::deps::Kind::font});
  tools.require({.id = "dot"});
  return tools.check();
}

// run.json's blocks, filled when the run starts (section 11; 09 leaves them to the batch layer).
void describe_run(run::Run& run, const Plan& plan) {
  run.set("versions", versions_block());
  run.set("tools", probes_block(plan.tools));
  run.set("platform", platform_block());
  run.set("install", install_block());
  run.set("environment", environment_block());
}

}  // namespace fusion::batch
