package fusion.frontend.pipeline;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertInstanceOf;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import org.junit.jupiter.api.Test;
import tools.jackson.databind.JsonNode;

/** The pipeline document, values and catalog as JSON (10.C12, 10.C8, 01.C9). */
class PipelineJsonTest {
    @Test
    void theShippedDefaultPipelineReadsAndRoundTrips() {
        PipelineDocument doc = TestFixtures.standard();
        assertEquals("standard@0.1.0", doc.origin());
        assertEquals(List.of("maps", "classify", "gsea", "network", "merged_network", "corridor", "solve", "plot",
                "explain"), doc.blocks().stream().map(Block::id).toList());
        PipelineDocument again = PipelineJson.read(Json.pretty(PipelineJson.write(doc)), "round trip");
        assertEquals(doc, again);
        assertEquals(PipelineJson.sha256(doc), PipelineJson.sha256(again));
    }

    @Test
    void theDefaultPipelineCarriesS806sDefaultsAndNoneForTheThresholdsBoundsOrTimeout() {
        PipelineDocument doc = TestFixtures.standard();
        assertEquals(Optional.of(new InputValue.Int(1)), doc.input("split_threshold").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.Choice("any")), doc.input("edges_to_target").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.Int(0)), doc.input("node_min").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.Int(0)), doc.input("edge_min").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.Int(10)), doc.input("bound").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.Text("fdr:0.25")), doc.input("gsea_selector").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.Flag(true)), doc.input("use_gsea").orElseThrow().defaultValue());
        assertEquals(Optional.of(new InputValue.InstallPath("data/kegg_pathways.txt")),
                doc.input("kgml_list").orElseThrow().defaultValue());
        for (String none : List.of("up", "down", "node_max", "edge_max", "timeout")) {
            InputDecl d = doc.input(none).orElseThrow();
            assertTrue(d.required() && d.defaultValue().isEmpty(), none + " (S8.06, 10.C13)");
        }
        assertTrue(doc.input("kgml_dir").orElseThrow().remember(), "kgml_dir is remembered (S8.31)");
        assertTrue(doc.input("bound").orElseThrow().sweepable());
    }

    @Test
    void valuesAreTypedByTheirDeclaration() {
        PipelineDocument doc = TestFixtures.standard();
        JsonNode session = Json.parse("""
                {"source": "hsa1956", "fsc": ["hsa3725"], "lfc": "/data/egf.tsv", "network_source": "kgml_list",
                 "kgml_list": {"install": "data/kegg_pathways.txt"}, "bound": 10, "up": 1, "use_gsea": true,
                 "timeout": "4h", "merged_graph": null, "relaxed_edges": "/old/r.txt",
                 "fdr": {"sweep": [0.25, 0.5]}}""", "session");
        Map<String, InputValue> v = PipelineJson.readValues(session, doc.inputs());
        assertEquals(new InputValue.GeneId("hsa1956"), v.get("source"));
        assertEquals(new InputValue.Items(List.of(new InputValue.GeneId("hsa3725"))), v.get("fsc"));
        assertEquals(new InputValue.PathValue(Path.of("/data/egf.tsv")), v.get("lfc"));
        assertEquals(new InputValue.Choice("kgml_list"), v.get("network_source"));
        assertEquals(new InputValue.InstallPath("data/kegg_pathways.txt"), v.get("kgml_list"));
        assertEquals(new InputValue.Int(10), v.get("bound"));
        assertEquals(new InputValue.Real(1), v.get("up"));
        assertEquals(new InputValue.Flag(true), v.get("use_gsea"));
        assertEquals(new InputValue.Text("4h"), v.get("timeout"));
        assertEquals(InputValue.UNSET, v.get("merged_graph"));
        // Undeclared inputs are kept, untyped (10.C8).
        assertEquals(new InputValue.Text("/old/r.txt"), v.get("relaxed_edges"));
        assertInstanceOf(InputValue.Sweep.class, v.get("fdr"));
        assertThrows(PipelineException.class,
                () -> PipelineJson.readValues(Json.parse("{\"bound\": \"ten\"}", "s"), doc.inputs()));
    }

    /** FuSION 1 lost keys through org.json's put(key, null) (S8.05); an unset value stays a null. */
    @Test
    void anUnsetValueIsWrittenAsAnExplicitNull() {
        String json = Json.compact(PipelineJson.writeValues(Map.of("merged_graph", InputValue.UNSET)));
        assertEquals("{\"merged_graph\":null}", json);
    }

    @Test
    void duplicateKeysAreRefusedSoOneArgumentCannotBeBoundTwice() {
        assertThrows(PipelineException.class, () -> PipelineJson.read("""
                {"format": 1, "origin": "x", "inputs": [], "blocks": [
                 {"id": "a", "command": "rgx", "args": {"file": {"literal": "/a"}, "file": {"literal": "/b"}}}]}""",
                "doc"));
        assertThrows(PipelineException.class, () -> PipelineJson.read("{\"format\": 2, \"origin\": \"x\"}", "doc"));
    }

    @Test
    void overridesRoundTrip() {
        List<GeneOverride> overrides = BatchGeneratorTest.manyOverrides();
        assertEquals(overrides, PipelineJson.readOverrides(PipelineJson.writeOverrides(overrides)));
        String json = Json.compact(PipelineJson.writeOverrides(TestFixtures.fosbNotDe()));
        assertEquals("[{\"gene_id\":\"hsa2354\",\"observation\":\"not_de\",\"activity\":null,\"relaxation\":null}]", json);
    }

    /** The backend's own command table (01.C9), read from fusion --commands' real output. */
    @Test
    void theCatalogReadsTheBackendsExport() {
        Catalog c = TestFixtures.catalog();
        assertEquals(1, c.format());
        CommandSpec corridor = c.command("corridor").orElseThrow();
        assertEquals("fb_rch", corridor.name());
        assertEquals(Optional.of(ValueKind.CORRIDOR), corridor.produces());
        assertEquals(List.of(ValueKind.NETWORK), corridor.needs());
        ArgSpec graph = corridor.arg("graph").orElseThrow();
        assertEquals(Optional.of(ValueKind.NETWORK), graph.takes());
        assertEquals(Optional.of("0"), graph.defaultValue());
        assertEquals("corridor.graph", graph.setting());
        assertTrue(corridor.arg("source").orElseThrow().list());
        assertEquals(Need.REQUIRED, corridor.arg("bound").orElseThrow().need());
        assertEquals(List.of("any", "activation", "expression"), corridor.arg("edges_to_target").orElseThrow().choices());
        assertEquals(List.of(new Rule(RuleKind.LESS, "down", "up")), c.command("cudf").orElseThrow().rules());
        assertEquals(List.of(new Rule(RuleKind.LESS_OR_EQUAL, "node_min", "node_max"),
                        new Rule(RuleKind.LESS_OR_EQUAL, "edge_min", "edge_max"),
                        new Rule(RuleKind.REQUIRES, "coexpression_threshold", "coexpression_csv")),
                c.command("pathz3").orElseThrow().rules());
        // section 06's values, which the screens offer as suggestions (10.C13)
        assertEquals(Optional.of("8"), c.command("pathz3").orElseThrow().arg("node_max").orElseThrow().defaultValue());
        assertEquals(Optional.of("4h"), c.command("pathz3").orElseThrow().arg("timeout").orElseThrow().defaultValue());
        assertEquals(Optional.of(ValueKind.SHORTLIST), c.command("mff").orElseThrow().arg("list").orElseThrow().takes());
        assertEquals("corridor.skip_subtypes", c.runSettings().get(0).key());
        // No block can use what is left out: statements that are not steps, dropped commands (S2.29),
        // and arguments of FuSION 1's positional form only.
        for (String gone : List.of("let", "sweep", "run", "exit", "upfx")) {
            assertFalse(c.command(gone).isPresent(), gone);
        }
        assertTrue(c.command("exec").isPresent());
        assertFalse(c.command("start").orElseThrow().arg("exception_file").isPresent());
        assertFalse(c.command("pathz3").orElseThrow().arg("mode").isPresent());
        // Stage names are the frontend's own display labels.
        assertEquals("Constraint solving (Z3)", c.command("pathz3").orElseThrow().stage());
    }
}
