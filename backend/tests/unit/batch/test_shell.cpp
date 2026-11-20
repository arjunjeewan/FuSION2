// exec's pieces (01.C10, 01.C11, 01.C20): the shell, window plots, displays and FuSION 1's six moves.
#include <chrono>
#include <cstdlib>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "fusion/batch/batch.hpp"

using namespace fusion;

namespace {
class Lines final : public core::LogSink {
 public:
  void write(core::Level, std::string_view, std::string_view text) override { lines.emplace_back(text); }
  std::vector<std::string> lines;
};
}  // namespace

TEST_CASE("batch: window plots are gnuplot -p without set term (01.C20)", "[batch][shell]") {
  CHECK(batch::is_window_plot(R"(cd /w; gnuplot -p -e "set xlabel 'Node relaxn'; plot 'a.dat' with lp")"));
  CHECK(batch::is_window_plot("/usr/bin/gnuplot --persist script.gp"));
  CHECK_FALSE(batch::is_window_plot(R"(cd /w; gnuplot -p -e "set term png; set output 'a.png'; plot 'a.dat'")"));
  CHECK_FALSE(batch::is_window_plot("gnuplot script.gp"));
  CHECK_FALSE(batch::is_window_plot("bash /x/plot_po.sh a.dat b.dat"));
  CHECK_FALSE(batch::is_window_plot("echo gnuplot -p"));   // gnuplot is an argument here, not a command
  CHECK(batch::is_window_plot("echo x; gnuplot -p -e x"));
}

TEST_CASE("batch: a display is DISPLAY or WAYLAND_DISPLAY on Linux (01.C20)", "[batch][shell]") {
  const char* d = std::getenv("DISPLAY");
  const char* w = std::getenv("WAYLAND_DISPLAY");
  const std::string saved_d = d ? d : "", saved_w = w ? w : "";
  ::unsetenv("DISPLAY");
  ::unsetenv("WAYLAND_DISPLAY");
  CHECK_FALSE(batch::display_available());
  ::setenv("WAYLAND_DISPLAY", "wayland-0", 1);
  CHECK(batch::display_available());
  ::unsetenv("WAYLAND_DISPLAY");
  ::setenv("DISPLAY", ":0", 1);
  CHECK(batch::display_available());
  ::unsetenv("DISPLAY");
  if (!saved_d.empty()) ::setenv("DISPLAY", saved_d.c_str(), 1);
  if (!saved_w.empty()) ::setenv("WAYLAND_DISPLAY", saved_w.c_str(), 1);
}

TEST_CASE("batch: FuSION 1's six moves are exactly mv <name> <dest> (01.C11)", "[batch][shell]") {
  const auto po = batch::match_legacy_move("mv PO.dat /w/hsa1_to_hsa22_w.dat");
  REQUIRE(po);
  CHECK(po->name == batch::LegacyName::po_dat);
  CHECK(po->dest == "/w/hsa1_to_hsa22_w.dat");
  CHECK(batch::match_legacy_move("mv limits_timefile.txt /w/l.txt")->name == batch::LegacyName::limits_timefile);
  CHECK(batch::match_legacy_move("mv edges_merged_filename /w/e")->name == batch::LegacyName::edges_merged);
  CHECK(batch::match_legacy_move("mv fb_rch /w/fb_rch")->name == batch::LegacyName::fb_rch);
  CHECK(batch::match_legacy_move("  mv\tfb_rch_back  /w/b ")->name == batch::LegacyName::fb_rch_back);
  CHECK(batch::match_legacy_move("mv mylog.txt '/w/my log.txt'") == std::nullopt);   // four words
  CHECK(batch::match_legacy_move("mv mylog.txt \"/w/mylog.txt\"")->dest == "/w/mylog.txt");
  CHECK_FALSE(batch::match_legacy_move("mv other.dat /w/x"));
  CHECK_FALSE(batch::match_legacy_move("mv PO.dat"));
  CHECK_FALSE(batch::match_legacy_move("cp PO.dat /w/x"));
  CHECK_FALSE(batch::match_legacy_move("mv PO.dat /w/x; ls"));
  CHECK(batch::legacy_file_name(batch::LegacyName::mylog) == "mylog.txt");
}

TEST_CASE("batch: the shell passes the status and the output through (S1.14, row 1.15)", "[batch][shell]") {
  const auto shell = batch::Shell::find({});
  REQUIRE(shell);
  CHECK(shell->describe() == "/bin/sh");
  core::Log log(false);
  auto lines = std::make_shared<Lines>();
  log.add_sink(lines);
  const core::StopSource never;
  const auto dir = std::filesystem::temp_directory_path();
  CHECK(shell->run("exit 7", dir, log, never.token()).status == 7);
  const auto result = shell->run("echo one; echo two 1>&2; printf 'three'", dir, log, never.token());
  CHECK(result.status == 0);
  CHECK_FALSE(result.stopped);
  CHECK(lines->lines == std::vector<std::string>{"one", "two", "three"});
  lines->lines.clear();
  CHECK(shell->run("read x; echo \"[$x]\"", dir, log, never.token()).status == 0);   // stdin is the null device
  CHECK(lines->lines == std::vector<std::string>{"[]"});
}

TEST_CASE("batch: a stop during exec ends the shell and what it started (A1)", "[batch][shell]") {
  const auto shell = batch::Shell::find({});
  REQUIRE(shell);
  core::Log log(false);
  core::StopSource stop;
  std::thread stopper([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.request_stop();
  });
  const auto t0 = std::chrono::steady_clock::now();
  const auto result = shell->run("sleep 30 & sleep 30; wait", std::filesystem::temp_directory_path(), log, stop.token());
  stopper.join();
  CHECK(result.stopped);
  CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(10));
}
