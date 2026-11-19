# FuSION 2.0 Pareto plot (section 07; S6.22 to S6.24). In this directory,
# "gnuplot pareto.gp" draws pareto.png again; the data are inline.
set encoding utf8
set terminal pngcairo enhanced size 980,690 font "Liberation Sans,13" background rgb "#fcfcfb"
set output 'pareto.png'
set lmargin at screen 0.085; set rmargin at screen 0.975; set bmargin at screen 0.105; set tmargin at screen 0.865
set title "{/:Bold EGFR → CXCL8     ·     Pareto frontier}\n{/*0.72 FSC JUN (hsa3725) · bound 10 · coexpression > 700, frozen > 800 · n ≤ 8, e ≤ 8}" tc rgb "#0b0b0b" font "Liberation Sans,16" offset 0,-0.4
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
set style line 12 lc rgb "#e34948" lw 2.4 pt 5 ps 1.4
set style line 22 lc rgb "#e34948" lw 2.4 pt 4 ps 1.4
$c0 << EOD
2	6.1
3	4.1
4	3.1
5	2.1
6	1.1
7	0.1
EOD
$c0_certified << EOD
2	6.1
3	4.1
4	3.1
5	2.1
6	1.1
7	0.1
EOD
$c1 << EOD
2	5.9
3	3.9
4	2.9
5	1.9
6	0.9
7	-0.1
EOD
$c1_certified << EOD
2	5.9
3	3.9
4	2.9
5	1.9
6	0.9
7	-0.1
EOD
plot $c0 u 1:2 w points pt 7 ps 2.3 lc rgb "#fcfcfb" notitle, \
     $c1 u 1:2 w points pt 5 ps 2.2 lc rgb "#fcfcfb" notitle, \
     $c0 u 1:2 w lines ls 11 notitle, \
     $c1 u 1:2 w lines ls 12 notitle, \
     $c0_certified u 1:2 w points ls 11 notitle, \
     $c1_certified u 1:2 w points ls 12 notitle, \
     keyentry w linespoints ls 11 title "with FSC (JUN)", \
     keyentry w linespoints ls 12 title "without FSC (JUN)"
