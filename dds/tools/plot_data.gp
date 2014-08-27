set ytics ("DCPS:DataReader_return_loan" 1, 	\
	   "DCPS:DataReader_take" 2,		\
	   "DDS:W_POLL" 3,			\
	   "DDS:W_NOTIF" 4,			\
	   "DDS:WAKEUP" 5,			\
	   "DDS:W_RX" 6,			\
	   "DDS:W_EV" 7,			\
	   "DDS:W_CONT" 8,			\
	   "DCPS:DataWriter_write" 9)
set xlabel "Time (seconds)" 
set ylabel "DDS events"
set yrange [0:10]
plot	"/tmp/dww.dat" using 1:2 notitle,	\
	"/tmp/wcont.dat" using 1:2 notitle,	\
	"/tmp/wev.dat" using 1:2 notitle,	\
	"/tmp/wrx.dat" using 1:2 notitle,	\
	"/tmp/wakeup.dat" using 1:2 notitle,	\
	"/tmp/notif.dat" using 1:2 notitle,	\
	"/tmp/wpoll.dat" using 1:2 notitle,	\
	"/tmp/take.dat" using 1:2 notitle,	\
	"/tmp/rloan.dat" using 1:2 notitle

pause -1

