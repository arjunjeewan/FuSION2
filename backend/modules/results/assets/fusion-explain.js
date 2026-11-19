/* fusion-explain.js: FuSION 2.0's explanation page (section 07, "The explanation view: the one
   renderer"; 07.C2, 07.C4, S6.04, S6.08, S6.13 to S6.17). It reads the view model from the page's
   <script type="application/json" id="fusion-view"> element and draws it with vis-network, which
   pex writes beside it (S6.18). Every element is built with DOM calls and textContent, so no value
   from the data is ever read as HTML. Ported from FuSION 1's plot_explanation.py
   (Linux/supporting_files): its colours, borders, layout options and legend are kept (S6.13,
   S6.16); the drawing follows the C++ view model and re-derives no polarity (S6.26). */
(function () {
  'use strict';

  // The one style table: the drawing and the legend both read it, so they cannot drift (S6.16).
  // FuSION 1's values (plot_explanation.py:32-46).
  var STYLE = {
    fill: { up: '#8CCB4E', down: '#F2A24C', none: '#DEDEDE' },
    border: { normal: '#555555', source: '#1B5E20', target: '#0D3B66' },
    borderWidth: { normal: 1, ring: 4, relaxed: 2 },
    relaxedDashes: [2, 2],
    edge: { activation: '#8a8a8a', inhibition: '#C62828', flipped: '#8E44AD' },
    removedDashes: [8, 6],
    flippedDashes: [2, 6],
    width: { honoured: 2, removed: 3, flipped: 2, outside: 1.5 },
    fade: 0.45,                               // 07.C4: inactive nodes and edges outside the witness
    sameLevelRoundness: 0.6,
    parallelRoundness: 0.35
  };
  var OBSERVED = { up: 'up-regulated', down: 'down-regulated', none: 'unchanged' };
  var STATUS = {
    counted: 'counted', forced: 'forced', exempt_endpoint: 'exempt endpoint',
    removed_with_fsc: 'removed with the FSC', outside_encoding: 'outside the encoding',
    outside_corridor: 'outside the corridor', not_in_network: 'not in the network'
  };
  var STATUS_ORDER = ['counted', 'forced', 'exempt_endpoint', 'removed_with_fsc', 'outside_encoding',
                      'outside_corridor', 'not_in_network'];

  // ------------------------------------------------------------------ small helpers

  function el(tag, className, text) {
    var node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined && text !== null) node.textContent = String(text);
    return node;
  }

  function svg(tag, attrs) {
    var node = document.createElementNS('http://www.w3.org/2000/svg', tag);
    Object.keys(attrs).forEach(function (k) { node.setAttribute(k, String(attrs[k])); });
    return node;
  }

  function rgba(hex, alpha) {
    var n = parseInt(hex.slice(1), 16);
    return 'rgba(' + ((n >> 16) & 255) + ',' + ((n >> 8) & 255) + ',' + (n & 255) + ',' + alpha + ')';
  }

  function lfcText(value) {
    return value === null || value === undefined ? 'n/a' : Number(value).toFixed(2);
  }

  function effectWord(effect) { return effect === 'inhibition' ? 'inhibition' : 'activation'; }

  function tooltip(lines) {
    var box = el('div', 'fx-tooltip');
    box.textContent = lines.join('\n');
    return box;
  }

  // ------------------------------------------------------------------ the path filter (S6.04)

  function parseMode(text) {
    if (text === 'subgraph' || text === 'all' || text === 'shortest') return { name: text, window: 2 };
    var m = /^alt(?::(\d+))?$/.exec(text || '');
    if (m) return { name: 'alt', window: m[1] === undefined ? 2 : parseInt(m[1], 10) };
    return { name: 'subgraph', window: 2 };
  }

  function modeText(mode) { return mode.name === 'alt' ? 'alt:' + mode.window : mode.name; }

  // kept_by()'s rule, over the slack the view model stores for the solve's endpoints.
  function keptBy(witness, mode) {
    return witness.edges.map(function (e) {
      if (mode.name === 'subgraph') return true;
      if (e.slack === null || e.slack === undefined) return false;
      if (mode.name === 'all') return true;
      if (mode.name === 'shortest') return e.slack === 0;
      return e.slack <= mode.window;
    });
  }

  // The ends of the kept edges plus the endpoints; every present node in subgraph mode.
  function drawnKeys(view, witness, mode, kept) {
    var keys = {};
    if (mode.name === 'subgraph') {
      witness.nodes.forEach(function (n) { keys[n.key] = true; });
      return keys;
    }
    var any = false;
    witness.edges.forEach(function (e, i) {
      if (!kept[i]) return;
      keys[e.source] = true;
      keys[e.target] = true;
      any = true;
    });
    if (any) {
      witness.nodes.forEach(function (n) {
        if (n.key === view.source || n.key === view.target) keys[n.key] = true;
      });
    }
    return keys;
  }

  // ------------------------------------------------------------------ hover texts

  function nodeTooltip(n) {
    var lines = [n.label + '  (' + n.key + ')'];
    if (n.kind && n.kind !== 'gene') lines.push(n.kind);
    lines.push('expression: ' + (OBSERVED[n.observed] || n.observed));
    if (n.members.length > 1) lines.push('members:');
    n.members.forEach(function (m) {
      var name = m.symbol === m.id ? m.id : m.symbol + ' (' + m.id + ')';
      lines.push((n.members.length > 1 ? '  ' : '') + name + '  lfc: ' + lfcText(m.lfc));
    });
    var flags = [];
    if (n.roles.indexOf('source') >= 0) flags.push('SOURCE');
    if (n.roles.indexOf('target') >= 0) flags.push('TARGET');
    if (n.roles.indexOf('fsc') >= 0) flags.push('FSC');
    if (n.relaxed) flags.push('relaxed');
    if (!n.active) flags.push('inactive in this explanation');
    if (flags.length) lines.push(flags.join(' | '));
    return tooltip(lines);
  }

  function edgeNote(e) {
    if (e.draw === 'flipped') {
      return 'net effect FLIPPED ' + effectWord(e.replaced) + ' -> ' + effectWord(e.effect) + ' by relaxation';
    }
    if (e.draw === 'removed') return effectWord(e.effect) + ' effect REMOVED by relaxation';
    return effectWord(e.effect);
  }

  function edgeTooltip(view, labels, e) {
    var lines = [labels[e.source] + ' -> ' + labels[e.target], edgeNote(e)];
    if (!e.in_witness) lines.push('not part of this explanation; counted because both ends are present');
    if (e.ecrel) lines.push('ECrel: enzymes linked through a compound');
    if (e.on_route) lines.push('on a sign-consistent route');
    if (e.slack !== null && e.slack !== undefined) lines.push('slack: ' + e.slack);
    var subtypes = [];
    var pathways = [];
    e.members.forEach(function (m) {
      var text = m.type + ': ' + (m.subtypes.length ? m.subtypes.join(', ') : '(none)');
      if (subtypes.indexOf(text) < 0) subtypes.push(text);
      m.pathways.forEach(function (p) { if (pathways.indexOf(p) < 0) pathways.push(p); });
    });
    lines.push('subtypes: ' + subtypes.join('; '));
    lines.push('pathways:');
    if (!pathways.length) lines.push('  (none recorded)');
    pathways.sort().forEach(function (p) { lines.push('  - ' + (view.pathways[p] || p)); });
    return tooltip(lines);
  }

  // ------------------------------------------------------------------ the drawing

  function nodeStyle(n) {
    var fill = STYLE.fill[n.observed] || STYLE.fill.none;
    var border = STYLE.border.normal;
    var width = STYLE.borderWidth.normal;
    if (n.roles.indexOf('source') >= 0) {
      border = STYLE.border.source;
      width = STYLE.borderWidth.ring;
    } else if (n.roles.indexOf('target') >= 0) {
      border = STYLE.border.target;
      width = STYLE.borderWidth.ring;
    }
    var alpha = n.active ? 1 : STYLE.fade;   // 07.C4: a present, inactive node is drawn faded
    var colour = { background: rgba(fill, alpha), border: rgba(border, alpha) };
    var node = {
      id: n.key, label: n.label, level: n.level, shape: 'box', title: nodeTooltip(n),
      borderWidth: width,
      color: { background: colour.background, border: colour.border,
               highlight: { background: colour.background, border: rgba(border, 1) },
               hover: { background: colour.background, border: rgba(border, 1) } }
    };
    if (n.relaxed) {                          // a dotted border marks a relaxed node
      node.shapeProperties = { borderDashes: STYLE.relaxedDashes };
      node.borderWidth = Math.max(width, STYLE.borderWidth.relaxed);
    }
    return node;
  }

  function edgeStyle(view, labels, e, smooth) {
    var colour = e.draw === 'flipped' ? STYLE.edge.flipped : STYLE.edge[e.effect];
    var dashes = false;
    var width = STYLE.width.honoured;
    var alpha = 1;
    if (e.draw === 'removed') {
      dashes = STYLE.removedDashes;
      width = STYLE.width.removed;
    } else if (e.draw === 'flipped') {
      dashes = STYLE.flippedDashes;
      width = STYLE.width.flipped;
    }
    if (!e.in_witness) {                      // drawn like a removed edge, faded (S6.15)
      dashes = STYLE.removedDashes;
      width = STYLE.width.outside;
      alpha = STYLE.fade;
    }
    var edge = {
      from: e.source, to: e.target, dashes: dashes, width: width, title: edgeTooltip(view, labels, e),
      color: { color: rgba(colour, alpha), highlight: rgba(colour, 1), hover: rgba(colour, 1), inherit: false },
      arrows: { to: { enabled: true, type: e.effect === 'inhibition' ? 'bar' : 'arrow' } }
    };
    if (e.ecrel) edge.label = 'EC';           // S2.31
    if (smooth) edge.smooth = smooth;
    return edge;
  }

  var OPTIONS = {                             // plot_explanation.py:375-392 (S6.13)
    layout: {
      randomSeed: 1,
      hierarchical: { enabled: true, direction: 'LR', sortMethod: 'directed', levelSeparation: 260,
                      nodeSpacing: 150, treeSpacing: 220, blockShifting: true, edgeMinimization: true,
                      parentCentralization: true }
    },
    physics: { enabled: false },
    interaction: { hover: true, navigationButtons: true, keyboard: true, tooltipDelay: 80 },
    nodes: { font: { size: 24, face: 'Helvetica', color: '#1a1a1a' }, margin: 10,
             widthConstraint: { maximum: 230 } },
    edges: { smooth: { type: 'cubicBezier', forceDirection: 'horizontal', roundness: 0.5 },
             font: { size: 16, align: 'middle', strokeWidth: 4, strokeColor: '#ffffff' } }
  };

  // ------------------------------------------------------------------ the legend (S6.16)

  function swatch(fill, border, style) {
    var box = el('span', 'fx-swatch');
    box.style.background = fill;
    box.style.border = (style.width || 2) + 'px ' + (style.dash || 'solid') + ' ' + border;
    return box;
  }

  function line(colour, dashes, head, alpha) {
    var c = rgba(colour, alpha === undefined ? 1 : alpha);
    var pic = svg('svg', { width: 42, height: 14 });
    pic.appendChild(svg('line', { x1: 2, y1: 7, x2: 32, y2: 7, stroke: c, 'stroke-width': 3,
                                  'stroke-dasharray': dashes ? dashes.join(',') : 'none' }));
    if (head === 'bar') {
      pic.appendChild(svg('path', { d: 'M34,2 L34,12', stroke: c, 'stroke-width': 3 }));
    } else {
      pic.appendChild(svg('path', { d: 'M30,3 L38,7 L30,11', fill: 'none', stroke: c, 'stroke-width': 2.5 }));
    }
    return pic;
  }

  function legend() {
    var box = el('div', 'fx-legend');
    function row(group, items) {
      var r = el('div', 'fx-legend-row');
      r.appendChild(el('span', 'fx-legend-group', group + ':'));
      items.forEach(function (item) {
        var cell = el('span', 'fx-legend-item');
        cell.appendChild(item[0]);
        cell.appendChild(document.createTextNode(item[1]));
        r.appendChild(cell);
      });
      box.appendChild(r);
    }
    row('Nodes', [
      [swatch(STYLE.fill.up, STYLE.border.normal, {}), 'up-regulated'],
      [swatch(STYLE.fill.down, STYLE.border.normal, {}), 'down-regulated'],
      [swatch(STYLE.fill.none, STYLE.border.normal, {}), 'unchanged'],
      [swatch('#ffffff', STYLE.border.source, { width: STYLE.borderWidth.ring }), 'source'],
      [swatch('#ffffff', STYLE.border.target, { width: STYLE.borderWidth.ring }), 'target'],
      [swatch('#ffffff', STYLE.border.normal, { dash: 'dotted', width: STYLE.borderWidth.relaxed }), 'relaxed'],
      [swatch(rgba(STYLE.fill.up, STYLE.fade), rgba(STYLE.border.normal, STYLE.fade), {}),
       'inactive in this explanation (faded)']
    ]);
    row('Edges', [
      [line(STYLE.edge.activation, null, 'arrow'), 'activation'],
      [line(STYLE.edge.inhibition, null, 'bar'), 'inhibition'],
      [line(STYLE.edge.activation, STYLE.removedDashes, 'arrow'), 'effect removed by relaxation (dashed)'],
      [line(STYLE.edge.flipped, STYLE.flippedDashes, 'arrow'), 'effect flipped by relaxation (purple dotted)'],
      [line(STYLE.edge.activation, STYLE.removedDashes, 'arrow', STYLE.fade),
       'relaxed, not part of this explanation (faded)'],
      [el('span', 'fx-swatch', 'EC'), 'ECrel edge']
    ]);
    return box;
  }

  // ------------------------------------------------------------------ the tables

  function observationSummary(view) {
    var parts = [];
    STATUS_ORDER.forEach(function (k) {
      if (view.observations[k]) parts.push(view.observations[k] + ' ' + STATUS[k]);
    });
    return parts.length ? 'Observations: ' + parts.join(', ') + '.' : '';
  }

  function parkedTable(witness) {
    if (!witness.parked.length) return null;
    var box = el('details', 'fx-section');
    box.appendChild(el('summary', null, 'Relaxed nodes outside this explanation: ' + witness.parked.length));
    var table = el('table');
    var head = el('tr');
    ['symbol', 'key', 'log-FC', 'expression', 'why it is listed'].forEach(function (h, i) {
      head.appendChild(el('th', i === 2 ? 'fx-num' : null, h));
    });
    table.appendChild(head);
    witness.parked.forEach(function (p) {
      var tr = el('tr');
      tr.appendChild(el('td', null, p.label));
      tr.appendChild(el('td', 'fx-key', p.key));
      tr.appendChild(el('td', 'fx-num', lfcText(p.lfc)));
      tr.appendChild(el('td', null, OBSERVED[p.observed] || p.observed));
      tr.appendChild(el('td', null, p.reason));
      table.appendChild(tr);
    });
    box.appendChild(table);
    return box;
  }

  function edgeTable(view, labels, witness, kept) {
    // S6.08: one row per drawn edge listing all its edges, as the edge table file has it.
    var box = el('details', 'fx-section');
    var rows = [];
    witness.edges.forEach(function (e, i) { if (kept[i]) rows.push(e); });
    box.appendChild(el('summary', null, 'Edge table: ' + rows.length + ' drawn edges'));
    var table = el('table');
    var head = el('tr');
    ['source', 'target', 'effect', 'draw', 'in witness', 'edges', 'pathways'].forEach(function (h) {
      head.appendChild(el('th', null, h));
    });
    table.appendChild(head);
    rows.forEach(function (e) {
      var tr = el('tr');
      var pathways = [];
      e.members.forEach(function (m) {
        m.pathways.forEach(function (p) { if (pathways.indexOf(p) < 0) pathways.push(p); });
      });
      tr.appendChild(el('td', null, labels[e.source]));
      tr.appendChild(el('td', null, labels[e.target]));
      tr.appendChild(el('td', null, e.effect));
      tr.appendChild(el('td', null, e.draw));
      tr.appendChild(el('td', null, e.in_witness ? 'yes' : 'no'));
      tr.appendChild(el('td', 'fx-key', e.members.map(function (m) { return m.key; }).join('; ')));
      tr.appendChild(el('td', null, pathways.sort().map(function (p) { return view.pathways[p] || p; }).join('; ')));
      table.appendChild(tr);
    });
    box.appendChild(table);
    return box;
  }

  // ------------------------------------------------------------------ the page

  function render() {
    var view;
    try {
      view = JSON.parse(document.getElementById('fusion-view').textContent);
    } catch (err) {
      document.body.appendChild(el('p', 'fx-missing', 'This page holds no readable explanation.'));
      return;
    }
    var state = { witness: Math.min(Math.max((view.witness || 1) - 1, 0), Math.max(view.witnesses.length - 1, 0)),
                  mode: parseMode(view.paths) };
    var network = null;

    var header = el('div', 'fx-header');
    header.appendChild(el('h1', 'fx-title', view.title));
    if (view.subtitle) header.appendChild(el('p', 'fx-subtitle', view.subtitle));
    if (view.certification) {
      header.appendChild(el('p', 'fx-certification' + (view.certified ? '' : ' fx-uncertified'), view.certification));
    }
    document.body.appendChild(header);

    if (!view.witnesses.length) {
      // S6.02, S6.21: a point without a witness, or a requested point that is not sat, says why.
      document.body.appendChild(el('p', 'fx-missing', view.missing || 'No explanation was recorded at this point.'));
      var summary0 = observationSummary(view);
      if (summary0) document.body.appendChild(el('div', 'fx-note', summary0));
      return;
    }

    var controls = el('div', 'fx-controls');
    var counts = el('span', 'fx-counts');
    if (view.witnesses.length > 1) {
      var pick = el('label', null, 'Explanation ');
      var select = el('select');
      view.witnesses.forEach(function (w, i) {
        var option = el('option', null, (i + 1) + ' of ' + view.witnesses.length);
        option.value = String(i);
        select.appendChild(option);
      });
      select.value = String(state.witness);
      select.addEventListener('change', function () { state.witness = parseInt(select.value, 10); draw(); });
      pick.appendChild(select);
      controls.appendChild(pick);
    }
    var modeLabel = el('label', null, 'Paths ');
    var modeSelect = el('select');
    [['subgraph', 'whole explanation'], ['all', 'all source-to-target paths'],
     ['shortest', 'shortest paths'], ['alt', 'near-shortest paths']].forEach(function (m) {
      var option = el('option', null, m[1]);
      option.value = m[0];
      modeSelect.appendChild(option);
    });
    modeSelect.value = state.mode.name;
    var windowLabel = el('label', null, 'window ');
    var windowInput = el('input');
    windowInput.type = 'number';
    windowInput.min = '0';
    windowInput.step = '1';
    windowInput.value = String(state.mode.window);
    windowLabel.appendChild(windowInput);
    function modeChanged() {
      var w = parseInt(windowInput.value, 10);
      state.mode = { name: modeSelect.value, window: isNaN(w) || w < 0 ? 0 : w };
      draw();
    }
    modeSelect.addEventListener('change', modeChanged);
    windowInput.addEventListener('change', modeChanged);
    modeLabel.appendChild(modeSelect);
    controls.appendChild(modeLabel);
    controls.appendChild(windowLabel);
    controls.appendChild(counts);
    document.body.appendChild(controls);
    document.body.appendChild(legend());

    var graph = el('div', 'fx-graph');
    var canvas = el('div', 'fx-graph-canvas');
    graph.appendChild(canvas);
    document.body.appendChild(graph);
    var route = el('div', 'fx-note');
    document.body.appendChild(route);
    var tables = el('div');
    document.body.appendChild(tables);
    var summary = observationSummary(view);
    if (summary) document.body.appendChild(el('div', 'fx-note', summary));

    function draw() {
      var witness = view.witnesses[state.witness];
      var labels = {};
      witness.nodes.forEach(function (n) { labels[n.key] = n.label; });
      var kept = keptBy(witness, state.mode);
      var keys = drawnKeys(view, witness, state.mode, kept);
      var levels = {};
      var nodes = [];
      witness.nodes.forEach(function (n) {
        if (!keys[n.key]) return;
        levels[n.key] = n.level;
        nodes.push(nodeStyle(n));
      });
      var edges = [];
      var flip = true;
      var seen = {};
      witness.edges.forEach(function (e, i) {
        if (!kept[i]) return;
        var pair = e.source + '\u0000' + e.target;
        var smooth = null;
        if (seen[pair]) {
          smooth = { enabled: true, type: 'curvedCW', roundness: STYLE.parallelRoundness };
        } else if (levels[e.source] === levels[e.target]) {
          // Same-level edges bow aside, alternating, so they do not cut through the column
          // (plot_explanation.py:307-314).
          smooth = { enabled: true, type: flip ? 'curvedCW' : 'curvedCCW', roundness: STYLE.sameLevelRoundness };
          flip = !flip;
        }
        seen[pair] = true;
        var edge = edgeStyle(view, labels, e, smooth);
        edge.id = 'e' + i;
        edges.push(edge);
      });
      var parked = witness.parked.length;
      // S6.09, S6.20: the counts printed are the counts drawn.
      counts.textContent = nodes.length + ' nodes, ' + edges.length + ' edges drawn' +
        (parked ? '; ' + parked + ' relaxed node' + (parked === 1 ? '' : 's') + ' not drawn' : '') +
        (state.mode.name !== 'subgraph' && !edges.length ? '; no source-to-target path in this mode' : '');
      windowLabel.style.display = state.mode.name === 'alt' ? '' : 'none';
      route.textContent = witness.route || '';
      while (tables.firstChild) tables.removeChild(tables.firstChild);
      var parkedBox = parkedTable(witness);
      if (parkedBox) tables.appendChild(parkedBox);
      if (view.edge_table) tables.appendChild(edgeTable(view, labels, witness, kept));
      if (typeof vis === 'undefined' || !vis.Network) {
        canvas.textContent = 'The drawing library did not load (lib/vis-network.min.js).';
        return;
      }
      var data = { nodes: new vis.DataSet(nodes), edges: new vis.DataSet(edges) };
      if (network) network.destroy();
      network = new vis.Network(canvas, data, OPTIONS);
      network.once('afterDrawing', function () { network.fit(); });
    }

    draw();
    // Refit on load and on resize, as pex did (plot_explanation.py:480-483).
    window.addEventListener('resize', function () { if (network) network.fit(); });
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', render);
  else render();
}());
