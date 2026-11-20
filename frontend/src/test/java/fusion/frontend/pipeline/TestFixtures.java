package fusion.frontend.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.io.UncheckedIOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

/** Shared test data: the shipped default pipeline, the fixture catalog and fixture sessions. */
public final class TestFixtures {
    private TestFixtures() {}

    public static Path repository() {
        String p = System.getProperty("fusion.test.repository");
        if (p == null) {
            throw new IllegalStateException("run the tests through Gradle, which sets fusion.test.repository");
        }
        return Path.of(p);
    }

    public static Path standardPipelineFile() {
        return repository().resolve("share/fusion/pipelines/standard.json");
    }

    private static PipelineDocument standard;

    /** The shipped default pipeline, read once. */
    public static synchronized PipelineDocument standard() {
        if (standard == null) {
            try {
                standard = PipelineJson.read(Files.readString(standardPipelineFile()), "standard.json");
            } catch (IOException e) {
                throw new UncheckedIOException(e);
            }
        }
        return standard;
    }

    public static String resource(String name) {
        try (InputStream in = TestFixtures.class.getResourceAsStream(name)) {
            if (in == null) {
                throw new IllegalStateException("no test resource " + name);
            }
            return new String(in.readAllBytes(), StandardCharsets.UTF_8);
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    /**
     * The command table as the backend exports it (01.C9): {@code catalog.json} is the real output of
     * {@code fusion --commands}, regenerated from the binary with
     * {@code build/dev/stage/bin/fusion --commands > frontend/src/test/resources/fusion/frontend/pipeline/catalog.json}.
     */
    public static String catalogText() {
        return resource("/fusion/frontend/pipeline/catalog.json");
    }

    public static Catalog catalog() {
        return CatalogJson.read(catalogText());
    }

    public static InputValue path(String p) {
        return new InputValue.PathValue(Path.of(p));
    }

    public static InputValue gene(String id) {
        return new InputValue.GeneId(id);
    }

    public static InputValue genes(String... ids) {
        return new InputValue.Items(java.util.Arrays.stream(ids).map(TestFixtures::gene).toList());
    }

    public static InputValue real(double v) {
        return new InputValue.Real(v);
    }

    public static InputValue integer(long v) {
        return new InputValue.Int(v);
    }

    public static InputValue text(String v) {
        return new InputValue.Text(v);
    }

    public static InputValue choice(String v) {
        return new InputValue.Choice(v);
    }

    /**
     * Section 10's example session, "EGFR to CXCL8, FSC JUN" (Files this section writes): the
     * reference run's inputs, with the values the example batch writes.
     */
    public static Map<String, InputValue> egfSession() {
        Map<String, InputValue> v = new LinkedHashMap<>();
        v.put("results_dir", path("/home/ks-149-u24/FuSION/runs"));
        v.put("lfc", path("/home/ks-149-u24/data/egf_hela_t120_lfc.tsv"));
        v.put("up", real(1.01));
        v.put("down", real(-1.01));
        v.put("network_source", choice("kgml_list"));
        v.put("kgml_list", new InputValue.InstallPath("data/kegg_pathways.txt"));
        v.put("merged_graph", InputValue.UNSET);
        v.put("kgml_dir", path("/home/ks-149-u24/kegg/hsa_pathways_with_exp_score"));
        v.put("use_gsea", new InputValue.Flag(true));
        v.put("gsea_selector", text("fdr:0.25"));
        v.put("split_threshold", integer(1));
        v.put("not_merged", path("/home/ks-149-u24/data/hsa_ids_not_to_be_merged.txt"));
        v.put("maps", choice("shipped"));
        v.put("symbols", InputValue.UNSET);
        v.put("pathway_names", InputValue.UNSET);
        v.put("source", gene("hsa1956"));
        v.put("target", gene("hsa3576"));
        v.put("bound", integer(10));
        v.put("edges_to_target", choice("any"));
        v.put("fsc", genes("hsa3725"));
        v.put("node_min", integer(0));
        v.put("node_max", integer(8));
        v.put("edge_min", integer(0));
        v.put("edge_max", integer(8));
        v.put("timeout", text("4h"));
        v.put("budget", InputValue.UNSET);
        v.put("solutions", InputValue.UNSET);
        v.put("coexpression_csv", path("/home/ks-149-u24/data/hsa_stringdb_coexpression_links_over400.csv"));
        v.put("coexpression_threshold", real(700));
        v.put("frozen_threshold", real(800));
        v.put("essential_edges", InputValue.UNSET);
        v.put("avoid_edges", InputValue.UNSET);
        v.put("nonrelaxed_edges", InputValue.UNSET);
        return v;
    }

    /** FOSB set to Not DE, which sits at 1.04, just above the 1.01 cut (section 10's example). */
    public static List<GeneOverride> fosbNotDe() {
        return List.of(new GeneOverride("hsa2354", Optional.of(Observation.NOT_DE), Optional.empty(), Optional.empty()));
    }

    /**
     * FuSION 1's regenerated toy case {@code toy-hsa13-wni-tsv-b10} in 2.0's form: the pre-merged
     * graph of {@code backend/tests/reference/synthetic/toy-hsa13-wni-tsv-b10/inputs/}, at the paths
     * its batch names ({@code backend/tests/fixtures/toy-batches/toy-hsa13-wni-tsv-b10.batch}).
     */
    public static Map<String, InputValue> toyPremergedSession() {
        Map<String, InputValue> v = new LinkedHashMap<>();
        v.put("lfc", path("/fusion-test/case/inputs/with_node_included.tsv"));
        v.put("up", real(0.9));
        v.put("down", real(-0.9));
        v.put("network_source", choice("merged_graph"));
        v.put("merged_graph", path("/fusion-test/case/inputs/report_example_with_pathways.xml"));
        v.put("split_threshold", integer(1));
        v.put("source", gene("hsa1"));
        v.put("target", gene("hsa22"));
        v.put("bound", integer(10));
        v.put("fsc", genes("hsa13"));
        v.put("node_max", integer(7));
        v.put("edge_max", integer(7));
        v.put("timeout", text("150s"));
        return v;
    }

    public static Provenance provenance() {
        return new Provenance("0.1.0", "0b6f2c1e-6a4d-4f0e-9d1a-5c3e8f2b7a90");
    }
}
