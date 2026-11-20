// exec lines: the POSIX shell (S1.14, 01.C10), window plots without a display (01.C20) and FuSION 1's
// six fixed-name moves (01.C11). FuSION 1 joined the tokens with single spaces and called system(),
// discarding its status (Linux/src/main.cc:1318-1330); 2.0 passes the rest of the line as written,
// through core's process layer in its own process group, captures its output into the log, and fails
// on a non-zero status.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <random>
#include <thread>

#include "fusion/core/process.hpp"
#include "internal.hpp"

namespace fusion::batch {

namespace {

std::filesystem::path capture_file() {
  static std::atomic<std::uint64_t> counter{0};
  static const std::uint64_t salt = [] {
    std::random_device device;   // names a scratch file only; never a result (09's determinism)
    return (static_cast<std::uint64_t>(device()) << 32) ^ device();
  }();
  return std::filesystem::temp_directory_path() /
         ("fusion-exec-" + std::to_string(salt) + "-" + std::to_string(counter.fetch_add(1)) + ".out");
}

// Passes every complete line of the shell's output through the log as it appears (row 1.15).
class LineReader {
 public:
  explicit LineReader(std::filesystem::path file) : file_(std::move(file)) {}
  void drain(core::Log& log, bool final) {
    std::ifstream in(file_, std::ios::binary);
    if (!in) return;
    in.seekg(static_cast<std::streamoff>(offset_));
    std::string chunk((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    offset_ += chunk.size();
    pending_ += chunk;
    std::size_t start = 0;
    while (true) {
      const auto nl = pending_.find('\n', start);
      if (nl == std::string::npos) break;
      std::string_view line(pending_.data() + start, nl - start);
      if (line.ends_with('\r')) line.remove_suffix(1);
      log.info(line);
      start = nl + 1;
    }
    pending_.erase(0, start);
    if (final && !pending_.empty()) {
      log.info(pending_);
      pending_.clear();
    }
  }
 private:
  std::filesystem::path file_;
  std::uint64_t offset_ = 0;
  std::string pending_;
};

}  // namespace

std::optional<Shell> Shell::find(const std::filesystem::path& home) {
#ifdef _WIN32
  // 01.C10: busybox-w32's sh, installed with FuSION; section 11's Windows build adds it.
  const auto busybox = home / "tools" / "busybox.exe";
  if (!std::filesystem::exists(busybox)) return std::nullopt;
  Shell shell;
  shell.path_ = busybox;
  shell.tools_dir_ = home / "tools";
  return shell;
#else
  if (!std::filesystem::exists("/bin/sh")) return std::nullopt;
  Shell shell;
  shell.path_ = "/bin/sh";
  if (!home.empty()) shell.tools_dir_ = home / "tools";
  return shell;
#endif
}

std::string Shell::describe() const { return core::to_utf8(path_); }

ShellResult Shell::run(std::string_view text, const std::filesystem::path& cwd, core::Log& log,
                       const core::StopToken& stop) const {
  core::ProcessSpec spec;
  spec.program = path_;
#ifdef _WIN32
  spec.args = {"sh", "-c", std::string(text)};
#else
  spec.args = {"-c", std::string(text)};
#endif
  spec.working_dir = cwd;
  // 11.C20: the user's PATH first, so a system gnuplot wins; FuSION's bundled tools last.
  std::string path = std::getenv("PATH") ? std::getenv("PATH") : "/usr/bin:/bin";
  if (!tools_dir_.empty() && std::filesystem::is_directory(tools_dir_)) path += ":" + core::to_utf8(tools_dir_);
  spec.env_set = {{"PATH", path}};
  spec.input = core::ProcessSpec::Input::null;          // a batch run never reads stdin (09.C7)
  spec.output = core::ProcessSpec::Output::file;
  spec.output_file = capture_file();
  spec.own_group = true;                                // a stop ends the shell and all it started (A1)
  LineReader reader(spec.output_file);
  ShellResult result;
  auto process = core::Process::spawn(spec);
  // Short shell lines end in milliseconds: poll from 1 ms, backing off to 50 ms for long ones.
  std::chrono::milliseconds wait{1};
  while (true) {
    if (process.poll()) break;
    if (stop.stop_requested()) {
      process.stop();
      result.stopped = true;
      break;
    }
    reader.drain(log, false);
    std::this_thread::sleep_for(wait);
    wait = std::min(wait * 2, std::chrono::milliseconds(50));
  }
  const auto finished = process.wait();
  reader.drain(log, true);
  std::error_code ec;
  std::filesystem::remove(spec.output_file, ec);
  result.status = finished.exit_code;
  if (!finished.error.empty()) log.warning("exec.start", "exec: " + finished.error);
  return result;
}

// 01.C20: "gnuplot -p" (or --persist) without "set term" opens a window. gnuplot must be a command:
// the first word of the line or of a part after ';', '&', '|' or '('.
bool is_window_plot(std::string_view text) {
  if (text.find("set term") != std::string_view::npos) return false;
  const auto separator = [](char c) { return c == ';' || c == '&' || c == '|' || c == '(' || c == '\n'; };
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t end = i;
    while (end < text.size() && !separator(text[end])) ++end;
    std::vector<std::string_view> words;
    for (std::size_t k = i; k < end;) {
      while (k < end && (text[k] == ' ' || text[k] == '\t')) ++k;
      const std::size_t start = k;
      while (k < end && text[k] != ' ' && text[k] != '\t') ++k;
      if (k > start) words.push_back(text.substr(start, k - start));
    }
    if (!words.empty() && (words[0] == "gnuplot" || words[0].ends_with("/gnuplot")))
      for (std::size_t w = 1; w < words.size() && words[w].starts_with('-'); ++w)
        if (words[w] == "-p" || words[w] == "--persist" || words[w] == "-persist") return true;
    i = end + 1;
  }
  return false;
}

bool display_available() {
  const auto set = [](const char* name) {
    const char* v = std::getenv(name);
    return v && *v;
  };
#if defined(__APPLE__)
  return !(set("SSH_CONNECTION") || set("SSH_CLIENT") || set("SSH_TTY"));
#elif defined(_WIN32)
  return true;
#else
  return set("DISPLAY") || set("WAYLAND_DISPLAY");
#endif
}

std::string_view legacy_file_name(LegacyName name) noexcept {
  switch (name) {
    case LegacyName::po_dat: return "PO.dat";
    case LegacyName::limits_timefile: return "limits_timefile.txt";
    case LegacyName::edges_merged: return "edges_merged_filename";
    case LegacyName::fb_rch: return "fb_rch";
    case LegacyName::fb_rch_back: return "fb_rch_back";
    case LegacyName::mylog: return "mylog.txt";
  }
  return {};
}

// 01.C11: exactly "mv <name> <dest>", with <name> one of FuSION 1's six fixed-name outputs.
std::optional<LegacyMove> match_legacy_move(std::string_view text) {
  std::vector<std::string_view> words;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    const std::size_t start = i;
    while (i < text.size() && text[i] != ' ' && text[i] != '\t') ++i;
    if (i > start) words.push_back(text.substr(start, i - start));
  }
  if (words.size() != 3 || words[0] != "mv") return std::nullopt;
  for (const auto name : {LegacyName::po_dat, LegacyName::limits_timefile, LegacyName::edges_merged,
                          LegacyName::fb_rch, LegacyName::fb_rch_back, LegacyName::mylog}) {
    if (words[1] != legacy_file_name(name)) continue;
    std::string_view dest = words[2];
    if (dest.size() >= 2 && (dest.front() == '"' || dest.front() == '\'') && dest.back() == dest.front())
      dest = dest.substr(1, dest.size() - 2);
    if (dest.empty()) return std::nullopt;
    return LegacyMove{name, core::path_from_utf8(dest)};
  }
  return std::nullopt;
}

}  // namespace fusion::batch
