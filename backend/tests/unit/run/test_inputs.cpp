// Inputs copied and read once (09.C3), the checks before the first step (decision 0021, 2A.7),
// inputs the batch makes (01.C12), the frontend's adopted directory (A1, S8.15) and the check-only
// mode (09.C12). Real inputs come from FuSION 1's toy case kept in the repository (booking 1).
#include <algorithm>
#include <set>

#include "support.hpp"

namespace fusion::run::test {

namespace {

const fs::path toy_batch = fs::path(FUSION_TEST_SOURCE_DIR) / "fixtures/toy-batches/toy-hsa5-wni-tsv-b10.batch";
const fs::path toy_inputs = fs::path(FUSION_TEST_SOURCE_DIR) / "reference/synthetic/toy-hsa5-wni-tsv-b10/inputs";

std::vector<std::string> codes(const core::CheckReport& report) {   // run.low_disk depends on the machine
    std::vector<std::string> out;
    for (const core::Diagnostic& d : report.diagnostics)
        if (d.code != "run.low_disk") out.push_back(d.code);
    return out;
}

// One path per line, as FuSION 1's KGML lists are; the owning module's reader stands behind this (02.C12).
std::vector<fs::path> one_path_per_line(const fs::path& copy) {
    std::vector<fs::path> members;
    std::istringstream in(read_file(copy));
    for (std::string line; std::getline(in, line);)
        if (!line.empty()) members.emplace_back(line);
    return members;
}

const core::Json* input_entry(const core::Json& manifest, std::string_view copy) {
    for (const core::Json& entry : manifest["inputs"])
        if (entry["copy"] == copy) return &entry;
    return nullptr;
}

}  // namespace

TEST_CASE("run: decision 0021's four cases: missing required, absent optional, empty optional, missing named optional",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    write_file(data / "values.tsv", "hsa1\t1.5\n");
    write_file(data / "empty.txt", "");
    StepSpec cudf = step("cudf", "cudf", 3);
    cudf.inputs = {input("values", data / "values.tsv"),
                   input("lfc", data / "missing.tsv"),                 // required, named, missing
                   input("not_de", "", false),                         // optional, not given: feature off
                   input("confirmed_up", data / "empty.txt", false),   // optional, given but empty: an empty list
                   input("confirmed_down", data / "gone.txt", false),  // optional, named but missing
                   input("symbols", "", true)};                        // required, not given
    auto console = std::make_shared<Capture>();
    Options options = options_in(tmp.path());
    options.sinks.push_back(console);
    Run run = Run::start(options);
    const std::vector<StepSpec> plan{cudf};
    run.copy_inputs(plan);
    const core::CheckReport report = run.check(plan);
    CHECK_FALSE(report.ok());
    std::vector<std::string> failing;
    for (const core::Diagnostic& d : report.diagnostics)
        if (d.severity == core::Severity::error) {
            CHECK(d.code == "input.missing");
            CHECK(d.line == 3);
            CHECK(d.command == "cudf");
            failing.push_back(d.setting.value_or(""));
        }
    std::sort(failing.begin(), failing.end());
    CHECK(failing == std::vector<std::string>{"confirmed_down", "lfc", "symbols"});
    CHECK(read_file(run.dir() / "inputs/values.tsv") == "hsa1\t1.5\n");
    CHECK(fs::is_regular_file(run.dir() / "inputs/empty.txt"));
    CHECK(fs::file_size(run.dir() / "inputs/empty.txt") == 0);
    CHECK(console->has(core::Level::error, "input.missing", "line 3: cudf: lfc: " + core::to_utf8(data / "missing.tsv")));
    CHECK(run.finish(RunStatus::failed) == 1);   // any error ends the run before a step runs (09.C14)

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["inputs"].size() == 2);
    CHECK(manifest["check"]["ok"] == false);
    CHECK(manifest["steps"][0]["status"] == "skipped");
    const auto events = read_lines(run.dir() / "events.jsonl");
    check_events(events);
    REQUIRE(of_type(events, "inputs.checked").size() == 1);
    const core::Json checked = of_type(events, "inputs.checked")[0];
    CHECK(checked["inputs"] == 2);
    CHECK(checked["bytes"] == 9);
    CHECK(checked["errors"] == 3);
    CHECK(of_type(events, "error").size() == 3);
    // inputs.checked comes before any step's event
    const auto first_step = std::find_if(events.begin(), events.end(),
                                         [](const core::Json& e) { return e["type"] == "step.skipped"; });
    CHECK(std::find_if(events.begin(), first_step, [](const core::Json& e) { return e["type"] == "inputs.checked"; }) !=
          first_step);
}

TEST_CASE("run: each input is copied and hashed in one read; the hash is sha256sum's, and a later edit of the original "
          "changes nothing the step reads (09.C3, 1.13)",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    write_file(data / "abc.txt", "abc");
    StepSpec cudf = step("cudf", "cudf", 3);
    cudf.inputs = {input("values", data / "abc.txt")};
    Run run = Run::start(options_in(tmp.path()));
    const std::vector<StepSpec> plan{cudf};
    run.copy_inputs(plan);
    CHECK(run.check(plan).ok());
    core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    REQUIRE(manifest["inputs"].size() == 1);
    const core::Json& entry = manifest["inputs"][0];
    CHECK(entry["sha256"] == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");   // sha256sum
    CHECK(entry["size"] == 3);
    CHECK(entry["copy"] == "inputs/abc.txt");
    CHECK(entry["path"] == core::to_utf8(data / "abc.txt"));
    CHECK(entry["step"] == "cudf");
    CHECK(entry["argument"] == "values");

    write_file(data / "abc.txt", "edited during the run");
    Run::Step s = run.begin(cudf);
    const fs::path copy = s.context().files.input(data / "abc.txt");
    CHECK(copy == run.dir() / "inputs/abc.txt");
    CHECK(read_file(copy) == "abc");
    run.end(s, StepStatus::ok);
    run.finish(RunStatus::ok);
}

TEST_CASE("run: a FuSION 1 batch and its inputs are copied byte for byte, with the batch in run.json's batch block "
          "(09.C3, E1)",
          "[unit][run]") {
    TempDir tmp;
    Options options = options_in(tmp.path());
    options.batch = toy_batch;
    StepSpec cudf = step("cudf", "cudf", 120);
    cudf.inputs = {input("values", toy_inputs / "with_node_included.tsv"),
                   input("additional_edges", toy_inputs / "empty_file", false)};
    StepSpec mff = step("mff", "mff", 125);
    mff.inputs = {input("list", toy_inputs / "report_example_with_pathways.xml")};
    Run run = Run::start(options);
    const std::vector<StepSpec> plan{cudf, mff};
    const core::CheckReport report = run.check(plan);   // copies first, then checks (1.13)
    CHECK(report.ok());
    CHECK(report.batch == toy_batch);
    CHECK(report.batch_sha256 == core::Sha256::of_file(toy_batch));
    run.finish(RunStatus::ok);

    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["batch"]["path"] == core::to_utf8(toy_batch));
    CHECK(manifest["batch"]["copy"] == "inputs/toy-hsa5-wni-tsv-b10.batch");
    CHECK(manifest["batch"]["sha256"] == core::Sha256::of_file(toy_batch).hex());
    CHECK(read_file(run.dir() / "inputs/toy-hsa5-wni-tsv-b10.batch") == read_file(toy_batch));
    for (const char* name : {"with_node_included.tsv", "empty_file", "report_example_with_pathways.xml"}) {
        INFO(name);
        CHECK(read_file(run.dir() / "inputs" / name) == read_file(toy_inputs / name));
        const core::Json* entry = input_entry(manifest, std::string("inputs/") + name);
        REQUIRE(entry);
        CHECK((*entry)["sha256"] == core::Sha256::of_file(toy_inputs / name).hex());
        CHECK((*entry)["size"] == fs::file_size(toy_inputs / name));
    }
}

TEST_CASE("run: relative inputs resolve against the batch file's folder, and two inputs with one name get two copies "
          "(01.C2, 09.C3)",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    write_file(data / "exp/egfr.batch", "cudf values=lfc/values.tsv\n");
    write_file(data / "exp/lfc/values.tsv", "1\n");
    write_file(data / "other/values.tsv", "2\n");
    Options options = options_in(tmp.path());
    options.batch = data / "exp/egfr.batch";
    StepSpec a = step("cudf", "cudf", 1);
    a.inputs = {input("values", "lfc/values.tsv")};
    StepSpec b = step("cudf-2", "cudf", 2);
    b.inputs = {input("values", data / "other/values.tsv")};
    Run run = Run::start(options);
    const std::vector<StepSpec> plan{a, b};
    CHECK(run.check(plan).ok());
    {
        Run::Step s = run.begin(a);
        CHECK(read_file(s.context().files.input("lfc/values.tsv")) == "1\n");
        run.end(s, StepStatus::ok);
    }
    {
        Run::Step s = run.begin(b);
        CHECK(s.context().files.input(data / "other/values.tsv") == run.dir() / "inputs/values-2.tsv");
        run.end(s, StepStatus::ok);
    }
    run.finish(RunStatus::ok);
    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK((*input_entry(manifest, "inputs/values.tsv"))["path"] == core::to_utf8(data / "exp/lfc/values.tsv"));
    CHECK((*input_entry(manifest, "inputs/values-2.tsv"))["path"] == core::to_utf8(data / "other/values.tsv"));
}

TEST_CASE("run: a list's members are copied to inputs/<list stem>/ with the list they came from; a missing member is "
          "input.list_member (09.C3, 02.C12)",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    write_file(data / "kegg/hsa04010.xml", "<pathway name=\"a\"/>\n");
    write_file(data / "kegg/hsa04151.xml", "<pathway name=\"b\"/>\n");
    write_file(data / "list_of_kegg_xml_files.txt", core::to_utf8(data / "kegg/hsa04010.xml") + "\n" +
                                                        core::to_utf8(data / "kegg/hsa04151.xml") + "\n" +
                                                        core::to_utf8(data / "kegg/hsa00000.xml") + "\n");
    InputSpec list = input("candidates", data / "list_of_kegg_xml_files.txt");
    list.is_list = true;
    list.members = one_path_per_line;
    StepSpec gsea = step("gsea", "gsea", 8);
    gsea.inputs = {list};
    Run run = Run::start(options_in(tmp.path()));
    const std::vector<StepSpec> plan{gsea};
    const core::CheckReport report = run.check(plan);
    CHECK(codes(report) == std::vector<std::string>{"input.list_member"});
    CHECK(report.diagnostics[0].path == core::to_utf8(data / "kegg/hsa00000.xml"));
    CHECK(report.diagnostics[0].setting == "candidates");
    CHECK(fs::is_regular_file(run.dir() / "inputs/list_of_kegg_xml_files.txt"));
    CHECK(read_file(run.dir() / "inputs/list_of_kegg_xml_files/hsa04010.xml") == "<pathway name=\"a\"/>\n");
    CHECK(fs::is_regular_file(run.dir() / "inputs/list_of_kegg_xml_files/hsa04151.xml"));
    {
        Run::Step s = run.begin(gsea);
        CHECK(s.context().files.input(data / "kegg/hsa04151.xml") ==
              run.dir() / "inputs/list_of_kegg_xml_files/hsa04151.xml");
        run.end(s, StepStatus::ok);
    }
    run.finish(RunStatus::ok);
    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["inputs"].size() == 3);
    CHECK((*input_entry(manifest, "inputs/list_of_kegg_xml_files/hsa04010.xml"))["member_of"] ==
          "inputs/list_of_kegg_xml_files.txt");

    // The owning module's reader may refuse the list itself, with its own code (02.C12, 1.24).
    TempDir again;
    InputSpec refused = list;
    refused.members = [](const fs::path&) -> std::vector<fs::path> {
        throw core::Failure("list.duplicate_pathway", "two files hold pathway hsa04010");
    };
    StepSpec other = step("mff", "mff", 9);
    other.inputs = {refused};
    Run second = Run::start(options_in(again.path()));
    const std::vector<StepSpec> other_plan{other};
    CHECK(codes(second.check(other_plan)) == std::vector<std::string>{"list.duplicate_pathway"});
    second.finish(RunStatus::failed);
}

TEST_CASE("run: an input the batch makes is checked and copied just before its step, and copied again when it changes "
          "(01.C12, decision 0017)",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    const fs::path made = data / "connect_pairs";   // written by an exec line, as FuSION 1's batches do
    InputSpec pairs = input("connect_pairs", made);
    pairs.made_by_line = 4;
    StepSpec first = step("pathz3", "pathz3", 5);
    first.inputs = {pairs};
    StepSpec second = step("pathz3-2", "pathz3", 7);
    second.inputs = {pairs};
    StepSpec third = step("pathz3-3", "pathz3", 8);
    third.inputs = {pairs};
    Run run = Run::start(options_in(tmp.path()));
    const std::vector<StepSpec> plan{first, second, third};
    CHECK(run.check(plan).ok());                 // not there yet, and not an error
    CHECK_FALSE(fs::exists(run.dir() / "inputs"));

    write_file(made, "hsa1 hsa22\n");            // the exec line runs
    {
        Run::Step s = run.begin(first);
        CHECK(read_file(s.context().files.input(made)) == "hsa1 hsa22\n");
        run.end(s, StepStatus::ok);
    }
    write_file(made, "hsa1 hsa5\n");             // a later exec line rewrites it
    {
        Run::Step s = run.begin(second);
        CHECK(read_file(s.context().files.input(made)) == "hsa1 hsa5\n");
        run.end(s, StepStatus::ok);
    }
    {
        Run::Step s = run.begin(third);          // unchanged: its copy is reused
        CHECK(s.context().files.input(made) == run.dir() / "inputs/connect_pairs-2");
        run.end(s, StepStatus::ok);
    }
    core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    REQUIRE(input_entry(manifest, "inputs/connect_pairs"));
    REQUIRE(input_entry(manifest, "inputs/connect_pairs-2"));
    CHECK((*input_entry(manifest, "inputs/connect_pairs"))["sha256"] != (*input_entry(manifest, "inputs/connect_pairs-2"))["sha256"]);
    CHECK_FALSE(fs::exists(run.dir() / "inputs/connect_pairs-3"));

    // An earlier step's output in this run directory is read where it lies.
    {
        Run::Step s = run.begin(step("cudf", "cudf", 10));
        core::OutputFile up = s.context().files.create("up_reg");
        up.out() << "hsa9\n";
        up.commit();
        run.end(s, StepStatus::ok);
    }
    InputSpec own = input("up", run.dir() / "cudf/up_reg");
    own.made_by_line = 10;
    StepSpec reader = step("pathz3-4", "pathz3", 11);
    reader.inputs = {own};
    {
        Run::Step s = run.begin(reader);
        CHECK(s.context().files.input(run.dir() / "cudf/up_reg") == run.dir() / "cudf/up_reg");
        run.end(s, StepStatus::ok);
    }
    manifest = read_json(run.dir() / "run.json");
    CHECK((*input_entry(manifest, "cudf/up_reg"))["step"] == "pathz3-4");

    // Missing when its step comes: the step fails with input.missing before it runs.
    fs::remove(made);
    StepSpec late = step("pathz3-5", "pathz3", 13);
    late.inputs = {pairs};
    CHECK(failure_code([&] { run.begin(late); }) == "input.missing");
    CHECK(run.finish(RunStatus::failed) == 1);
    manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    CHECK(manifest["error"]["code"] == "input.missing");
    CHECK(manifest["error"]["line"] == 13);
    CHECK(manifest["steps"].back()["status"] == "failed");
}

TEST_CASE("run: an input a step opens without the plan naming it is copied and recorded when first opened (09.C3)",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    write_file(data / "symbols.txt", "hsa1\tEGFR\n");
    Run run = Run::start(options_in(tmp.path()));
    Run::Step s = run.begin(step("start", "start", 1));
    CHECK(s.context().files.input(data / "symbols.txt") == run.dir() / "inputs/symbols.txt");
    CHECK(s.context().files.input(data / "symbols.txt") == run.dir() / "inputs/symbols.txt");   // once
    CHECK(failure_code([&] { s.context().files.input(data / "nope.txt"); }) == "input.missing");
    run.end(s, StepStatus::ok);
    run.finish(RunStatus::ok);
    const core::Json manifest = read_json(run.dir() / "run.json");
    check_manifest(manifest);
    REQUIRE(manifest["inputs"].size() == 1);
    CHECK(manifest["inputs"][0]["step"] == "start");
}

TEST_CASE("run: the frontend's directory is adopted, with run.batch and overrides/ recorded where they lie; a used one "
          "is refused (A1, S8.15, 09.C9)",
          "[unit][run]") {
    TempDir tmp;
    const fs::path dir = tmp / "egfr-session_20261112-184400";
    write_file(dir / "run.batch", "cudf values=/data/lfc.tsv not_de=overrides/not_de.txt\n");
    write_file(dir / "overrides/not_de.txt", "hsa7\n");
    Options options = options_in(tmp.path());
    options.adopt = dir;
    options.batch = dir / "run.batch";
    StepSpec cudf = step("cudf", "cudf", 1);
    cudf.inputs = {input("not_de", "overrides/not_de.txt", false)};   // relative to the batch's folder
    {
        Run run = Run::start(options);
        CHECK(run.dir() == dir);
        const std::vector<StepSpec> plan{cudf};
        CHECK(run.check(plan).ok());
        run.finish(RunStatus::ok);
        const core::Json manifest = read_json(dir / "run.json");
        check_manifest(manifest);
        CHECK(manifest["name"] == "egfr-session_20261112-184400");
        CHECK(manifest["batch"]["copy"] == "run.batch");
        CHECK(manifest["batch"]["sha256"] == core::Sha256::of_file(dir / "run.batch").hex());
        CHECK(manifest["inputs"][0]["copy"] == "overrides/not_de.txt");
        CHECK_FALSE(fs::exists(dir / "inputs"));   // both lie in the run directory already
        CHECK(fs::exists(dir / "egfr-session_20261112-184400.log"));
    }
    const std::string manifest = read_file(dir / "run.json");
    CHECK(failure_code([&] { Run::start(options); }) == "run.dir_used");
    CHECK(read_file(dir / "run.json") == manifest);   // nothing written
}

TEST_CASE("run: an output that is another output or an input, by file identity, is output.clash (2A.7)", "[unit][run]") {
    TempDir tmp;
    TempDir data;
    write_file(data / "values.tsv", "1\n");
    fs::create_symlink(data / "values.tsv", data / "alias.tsv");
    StepSpec a = step("cudf", "cudf", 3);
    a.inputs = {input("values", data / "values.tsv")};
    a.outputs = {data / "up.txt", "down.txt"};
    StepSpec b = step("cudf-2", "cudf", 4);
    b.outputs = {data / "up.txt"};               // the same absolute output
    StepSpec c = step("wgx", "wgx", 5);
    c.outputs = {data / "alias.tsv"};            // the input, through a link
    StepSpec d = step("pex", "pex", 6);
    d.outputs = {"page.html", data / "x/page.html"};   // one name in its step directory
    StepSpec e = step("fb_rch", "fb_rch", 7);
    e.outputs = {"corridor.json"};
    StepSpec f = step("fb_rch-2", "fb_rch", 8);
    f.outputs = {"corridor.json"};               // another step directory: no clash
    const std::vector<StepSpec> plan{a, b, c, d, e, f};
    Run run = Run::start(options_in(tmp.path()));
    const core::CheckReport report = run.check(plan);
    std::vector<std::uint32_t> lines;
    for (const core::Diagnostic& diagnostic : report.diagnostics)
        if (diagnostic.code == "output.clash") lines.push_back(diagnostic.line);
    CHECK(lines == std::vector<std::uint32_t>{3, 4, 6});
    CHECK_FALSE(report.ok());
    run.finish(RunStatus::failed);
    CHECK(codes(Run::check_only(plan)) == std::vector<std::string>{"output.clash", "output.clash", "output.clash"});
}

TEST_CASE("run: an input that is not a regular file is input.not_file, and one that cannot be read input.unreadable "
          "(0021)",
          "[unit][run]") {
    TempDir tmp;
    TempDir data;
    fs::create_directories(data / "a_folder");
    write_file(data / "secret.txt", "x");
    fs::permissions(data / "secret.txt", fs::perms::none);
    StepSpec s = step("cudf", "cudf", 3);
    s.inputs = {input("values", data / "a_folder"), input("map", data / "secret.txt")};
    Run run = Run::start(options_in(tmp.path()));
    const std::vector<StepSpec> plan{s};
    const std::vector<std::string> found = codes(run.check(plan));
    run.finish(RunStatus::failed);
    fs::permissions(data / "secret.txt", fs::perms::owner_all);
    CHECK(std::count(found.begin(), found.end(), "input.not_file") == 1);
    if (::geteuid() != 0) CHECK(std::count(found.begin(), found.end(), "input.unreadable") == 1);
}

TEST_CASE("run: the check-only mode checks the originals and writes nothing (09.C12)", "[unit][run]") {
    TempDir data;
    write_file(data / "values.tsv", "1\n");
    write_file(data / "kegg/hsa04010.xml", "<pathway/>\n");
    write_file(data / "list.txt", core::to_utf8(data / "kegg/hsa04010.xml") + "\n" +
                                      core::to_utf8(data / "kegg/hsa09999.xml") + "\n");
    InputSpec list = input("candidates", data / "list.txt");
    list.is_list = true;
    list.members = one_path_per_line;
    InputSpec made = input("connect_pairs", data / "not_yet");
    made.made_by_line = 4;
    StepSpec cudf = step("cudf", "cudf", 3);
    cudf.inputs = {input("values", data / "values.tsv"), input("lfc", data / "missing.tsv"),
                   input("not_de", "", false), made};
    StepSpec gsea = step("gsea", "gsea", 8);
    gsea.inputs = {list};
    std::set<fs::path> before;
    for (const auto& entry : fs::recursive_directory_iterator(data.path())) before.insert(entry.path());
    const core::CheckReport report = Run::check_only(std::vector<StepSpec>{gsea, cudf});
    CHECK(codes(report) == std::vector<std::string>{"input.missing", "input.list_member"});   // sorted by line
    CHECK(report.diagnostics[0].line == 3);
    CHECK(report.diagnostics[1].path == core::to_utf8(data / "kegg/hsa09999.xml"));
    std::set<fs::path> after;
    for (const auto& entry : fs::recursive_directory_iterator(data.path())) after.insert(entry.path());
    CHECK(after == before);
}

}  // namespace fusion::run::test
