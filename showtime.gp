reset
set ylabel 'time(ns)'
set xlabel 'size'
set key left top
set title 'runtime'
set term png enhanced font 'Verdana,10'

set output 'runtime.png'

plot [:][:]'kerneltime.txt' using 1:2 with linespoints linewidth 2 title 'kernel', \
'usertime.txt' using 1:2 with linespoints linewidth 2 title 'user', \
