#set term postscript
#set output "plott64.ps"
set title "uDDS Throughput Results (inter-thread 64-bit)"
set xlabel "Sample size (bytes)"
set xrange [0:14]
set xtics ("1" 1, "16" 2, "64" 3, "128" 4, "256" 5, "512" 6, "1024" 7, \
	   "1500" 8, "2048" 9, "4096" 10, "8192" 11, "16384" 12, "32768" 13)
set ylabel "Speed (Mbps)"
set y2label "Samples/second"
set log y
set yrange [1:400000]
set nolog y2
set y2range [0:400000]
set y2tics
set grid
plot "/tmp/plot.dat" using 1:4 title "speed" with linespoints axis x1y1,\
     "/tmp/plot.dat" using 1:3 title "samples" with linespoints axis x1y2
pause -1

