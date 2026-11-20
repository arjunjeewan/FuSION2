package fusion.frontend.pipeline;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * The points of a swept session: the product of every swept input's values, in declaration order
 * with the first dimension outermost, as section 01 numbers a sweep's points (01.C3). Submit makes
 * one run per point (10.C21).
 */
public final class SweepPoints {
    private SweepPoints() {}

    /** One point: its 0-based index, the value it fixes for each swept input, and its label. */
    public record Point(int index, Map<String, InputValue> fixed, String label) {
        public Point {
            fixed = java.util.Collections.unmodifiableMap(new LinkedHashMap<>(fixed));
            Objects.requireNonNull(label, "label");
        }
    }

    /** The swept inputs, in declaration order. */
    public static List<String> sweptInputs(PipelineDocument doc, Map<String, InputValue> inputs) {
        List<String> swept = new ArrayList<>();
        for (InputDecl d : doc.inputs()) {
            if (inputs.get(d.name()) instanceof InputValue.Sweep) {
                swept.add(d.name());
            }
        }
        return swept;
    }

    /**
     * Every point; one unlabelled point when nothing is swept. A point's label is each swept
     * input's name and value, such as {@code bound-6}, which the run directory's label appends to
     * the session's name (10.C18).
     */
    public static List<Point> points(PipelineDocument doc, Map<String, InputValue> inputs) {
        List<String> swept = sweptInputs(doc, inputs);
        List<Point> points = new ArrayList<>();
        if (swept.isEmpty()) {
            points.add(new Point(0, Map.of(), ""));
            return points;
        }
        List<List<InputValue>> dims = new ArrayList<>();
        for (String name : swept) {
            List<InputValue> values = ((InputValue.Sweep) inputs.get(name)).values();
            if (values.isEmpty()) {
                throw new PipelineException("input.empty_sweep", "input " + name + " sweeps no value");
            }
            dims.add(values);
        }
        int total = 1;
        for (List<InputValue> d : dims) {
            total = Math.multiplyExact(total, d.size());
        }
        for (int index = 0; index < total; index++) {
            Map<String, InputValue> fixed = new LinkedHashMap<>();
            List<String> label = new ArrayList<>();
            int rest = index;
            int[] pick = new int[dims.size()];
            for (int k = dims.size() - 1; k >= 0; k--) {
                pick[k] = rest % dims.get(k).size();
                rest /= dims.get(k).size();
            }
            for (int k = 0; k < dims.size(); k++) {
                InputValue v = dims.get(k).get(pick[k]);
                fixed.put(swept.get(k), v);
                label.add(swept.get(k) + "-" + v.text());
            }
            points.add(new Point(index, fixed, String.join("-", label)));
        }
        return points;
    }

    /** The session's values with one point's values fixed in place of the sweeps. */
    public static Map<String, InputValue> fix(Map<String, InputValue> inputs, Point point) {
        Map<String, InputValue> out = new LinkedHashMap<>(inputs);
        out.putAll(point.fixed());
        return out;
    }
}
