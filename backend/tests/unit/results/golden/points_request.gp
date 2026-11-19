# FuSION 2.0 Pareto plot (section 07; S6.22 to S6.24). In this directory,
# "gnuplot pareto.gp" draws pareto.png again; the data are inline.
set encoding utf8
set terminal pngcairo enhanced size 980,690 font "Liberation Sans,13" background rgb "#fcfcfb"
set output 'pareto.png'
set lmargin at screen 0.085; set rmargin at screen 0.975; set bmargin at screen 0.105; set tmargin at screen 0.865
set title "{/:Bold EGFR → CXCL8     ·     Pareto frontier}\n{/*0.72 bound 10 · coexpression > 700, frozen > 800 · n ≤ 8, e ≤ 8}" tc rgb "#0b0b0b" font "Liberation Sans,16" offset 0,-0.4
set xlabel "Node relaxations" tc rgb "#52514e" font "Liberation Sans,12.5" offset 0,0.4
set ylabel "Edge relaxations" tc rgb "#52514e" font "Liberation Sans,12.5" offset 1.2,0
set xrange [-0.5:8.5]
set yrange [-0.5:8.5]
set xtics 1 nomirror out scale 0.4 tc rgb "#898781" font "Liberation Sans,11"
set ytics 1 nomirror out scale 0.4 tc rgb "#898781" font "Liberation Sans,11"
set border 3 lc rgb "#c3c2b7" lw 1.4
set grid xtics ytics back lc rgb "#e1e0d9" lw 1.0 lt 1
set tics front
set key top right reverse Left samplen 1.4 spacing 1.35 font "Liberation Sans,12" tc rgb "#52514e" box lc rgb "#e1e0d9" lw 1.0 opaque
set style line 11 lc rgb "#2a78d6" lw 2.4 pt 7 ps 1.5
set style line 21 lc rgb "#2a78d6" lw 2.4 pt 6 ps 1.5
$c0 << EOD
1	0
2	2
EOD
$c0_certified << EOD
1	0
2	2
EOD
plot $c0 u 1:2 w points pt 7 ps 2.3 lc rgb "#fcfcfb" notitle, \
     $c0_certified u 1:2 w points ls 11 notitle, \
     keyentry w points ls 11 title "unconstrained (requested points)"
