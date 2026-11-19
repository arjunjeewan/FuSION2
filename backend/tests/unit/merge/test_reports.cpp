// The merge's reports (S2.24; section 03, "Files written"): exceptions.tsv and merged_edges.tsv.
#include <catch2/catch_test_macros.hpp>

#include "support.hpp"

using namespace merge_test;
using fusion::merge::build_base_merge;
using fusion::merge::ExceptionSet;
using fusion::merge::MergeKey;
using fusion::merge::Reason;
using fusion::merge::split;
using fusion::network::BuildSettings;

namespace {

fusion::network::Maps symbols(Context& c, const std::string& lines) {
    fusion::network::Maps maps;
    maps.symbols = fusion::network::LabelMap::read(c.scratch.write("symbols.tsv", lines), c.ctx);
    return maps;
}

template <class Write>
std::string written(Context& c, const std::string& name, Write&& write) {
    fusion::core::OutputFile f = c.files.create(name);
    write(f);
    f.commit();
    return read_file(c.files.step_dir() / name);
}

}  // namespace

TEST_CASE("exceptions.tsv lists E with reasons, what each id became and its base node", "[unit][merge][S2.13]") {
    Context c;
    const std::vector<DocPtr> docs = {
        read(c, Kgml("path:hsa04010").entry(1, "hsa:2353 hsa:3725").entry(2, "hsa:5599 hsa:5601 hsa:5602").entry(3, "hsa:1").rel(2, 1, {"phosphorylation"}).rel(1, 3, {"expression"}, "GErel")),
        read(c, Kgml("path:hsa05161").entry(1, "hsa:3725 hsa:5599 hsa:5601 hsa:5602").entry(2, "hsa:7").rel(1, 2, {"activation"})),
        read(c, Kgml("path:hsa05224").entry(1, "hsa:2353 hsa:3725 hsa:6667").entry(2, "hsa:8").rel(1, 2, {"expression"}, "GErel"))};
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    ExceptionSet e;
    e.add(std::string_view("hsa3725"), Reason::up);
    e.add(std::string_view("hsa3725"), Reason::fsc);
    e.add(std::string_view("hsa5599"), Reason::not_merged);
    e.add(std::string_view("hsa6662"), Reason::up);                 // SOX9: in no pathway of the list
    const auto m = split(*base, e);
    const auto maps = symbols(c, "hsa3725\tJUN\nhsa5599\tMAPK8\nhsa6662\tSOX9\nhsa2353\tFOS\n");
    const std::string tsv = written(c, "exceptions.tsv", [&](fusion::core::OutputFile& f) {
        fusion::merge::write_exceptions(m, *base, maps, f);
    });
    CHECK(tsv ==
          "id\tlabel\treasons\tnode\tbase_node\tbase_size\n"
          "hsa3725\tJUN\tup,fsc\thsa3725\thsa2353\t6\n"
          "hsa5599\tMAPK8\tnot_merged\thsa5599\thsa2353\t6\n"
          "hsa6662\tSOX9\tup\t\t\t0\n");
    SECTION("without a base merge (a legacy read) the base columns are empty") {
        const std::string no_base = written(c, "exceptions2.tsv", [&](fusion::core::OutputFile& f) {
            fusion::merge::write_exceptions(m, maps, f);
        });
        CHECK(no_base.find("hsa3725\tJUN\tup,fsc\thsa3725\t\t0\n") != std::string::npos);
    }
}

TEST_CASE("merged_edges.tsv: one row per merged edge and relation behind it", "[unit][merge][S2.24]") {
    Context c;
    const std::vector<DocPtr> docs = {
        read(c, Kgml("path:hsa04010", "MAPK signaling pathway").entry(45, "hsa:5906 hsa:5908").entry(41, "hsa:673").rel(45, 41, {"activation"}, "PPrel", "625")),
        read(c, Kgml("path:hsa04722", "Neurotrophin signaling pathway").entry(57, "hsa:5906 hsa:5908").entry(59, "hsa:673").rel(57, 59, {"activation"}, "PPrel", "625"))};
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    const auto m = split(*base, ExceptionSet{});
    const auto maps = symbols(c, "hsa5906\tRAP1A\nhsa673\tBRAF\n");
    const std::string tsv = written(c, "merged_edges.tsv", [&](fusion::core::OutputFile& f) {
        fusion::merge::write_edge_report(m, maps, f);
    });
    CHECK(tsv ==
          "source\ttarget\ttype\tsubtypes\tsource_label\ttarget_label\tpathway\tpathway_name\tentry1\tentry2\t"
          "entry1_ids\tentry2_ids\tscore\n"
          "hsa5906\thsa673\tPPrel\tactivation\tRAP1A+1\tBRAF\tpath:hsa04010\tMAPK signaling pathway\t45\t41\t"
          "hsa5906 hsa5908\thsa673\t625\n"
          "hsa5906\thsa673\tPPrel\tactivation\tRAP1A+1\tBRAF\tpath:hsa04722\tNeurotrophin signaling pathway\t57\t59\t"
          "hsa5906 hsa5908\thsa673\t625\n");
}

TEST_CASE("merged_edges.tsv: component edges name the group entry that made the complex", "[unit][merge][S2.24][S2.09]") {
    Context c;
    const std::vector<DocPtr> docs = {read(c, Kgml("path:hsa05200", "Pathways in cancer")
                                                  .entry(1, "hsa:4149")
                                                  .entry(2, "hsa:4609")
                                                  .group(3, {1, 2})
                                                  .entry(4, "hsa:1029")
                                                  .rel(3, 4, {"expression"}, "GErel", "0.5"))};
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    const auto m = split(*base, ExceptionSet{});
    const auto maps = symbols(c, "hsa4149\tMYC\nhsa4609\tMAX\nhsa1029\tCDKN2A\n");
    const std::string tsv = written(c, "merged_edges.tsv", [&](fusion::core::OutputFile& f) {
        fusion::merge::write_edge_report(m, maps, f);
    });
    const std::string cx = "c_s_hsa4149_hsa4609_c_e";
    const std::string tail = "\tpath:hsa05200\tPathways in cancer\t";
    CHECK(tsv ==
          "source\ttarget\ttype\tsubtypes\tsource_label\ttarget_label\tpathway\tpathway_name\tentry1\tentry2\t"
          "entry1_ids\tentry2_ids\tscore\n" +
              cx + "\thsa1029\tGErel\texpression\t[MYC+MAX]\tCDKN2A" + tail + "3\t4\thsa4149+hsa4609\thsa1029\t0.5\n" +
              cx + "\thsa4149\tPPrel\tcomponent\t[MYC+MAX]\tMYC" + tail + "3\t1\thsa4149+hsa4609\thsa4149\t\n" +
              cx + "\thsa4609\tPPrel\tcomponent\t[MYC+MAX]\tMAX" + tail + "3\t2\thsa4149+hsa4609\thsa4609\t\n" +
              "hsa4149\t" + cx + "\tPPrel\tcomponent\tMYC\t[MYC+MAX]" + tail + "1\t3\thsa4149\thsa4149+hsa4609\t\n" +
              "hsa4609\t" + cx + "\tPPrel\tcomponent\tMAX\t[MYC+MAX]" + tail + "2\t3\thsa4609\thsa4149+hsa4609\t\n");
}

TEST_CASE("the report survives the split: copied and recomputed edges keep their relations", "[unit][merge][S2.24][S2.22]") {
    Context c;
    const std::vector<DocPtr> docs = {
        read(c, Kgml("path:hsa00500").entry(1, "hsa:1 hsa:2").entry(2, "hsa:3").rel(1, 2, {"activation"}, "PPrel", "7")),
        read(c, Kgml("path:hsa00501").entry(1, "hsa:2").entry(2, "hsa:3").entry(3, "hsa:4").rel(1, 2, {"activation"}).rel(2, 3, {"inhibition"}))};
    const auto base = build_base_merge(docs, BuildSettings{}, MergeKey{}, c.ctx);
    const auto m = split(*base, exceptions({"hsa2"}));
    const std::string tsv = written(c, "merged_edges.tsv", [&](fusion::core::OutputFile& f) {
        fusion::merge::write_edge_report(m, fusion::network::Maps{}, f);
    });
    // hsa1 and hsa2 are apart now; the copied edge hsa3 -> hsa4 keeps its relation.
    CHECK(tsv.find("hsa1\thsa3\tPPrel\tactivation\thsa1\thsa3\tpath:hsa00500\tpath:hsa00500\t1\t2\thsa1 hsa2\thsa3\t7\n") !=
          std::string::npos);
    CHECK(tsv.find("hsa2\thsa3\tPPrel\tactivation\thsa2\thsa3\tpath:hsa00500\tpath:hsa00500\t1\t2\thsa1 hsa2\thsa3\t7\n") !=
          std::string::npos);
    CHECK(tsv.find("hsa2\thsa3\tPPrel\tactivation\thsa2\thsa3\tpath:hsa00501\tpath:hsa00501\t1\t2\thsa2\thsa3\t\n") !=
          std::string::npos);
    CHECK(tsv.find("hsa3\thsa4\tPPrel\tinhibition\thsa3\thsa4\tpath:hsa00501\tpath:hsa00501\t2\t3\thsa3\thsa4\t\n") !=
          std::string::npos);
}
