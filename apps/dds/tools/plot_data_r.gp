#set term postscript
#set output "testr.ps"
set ytics ("DCPS:DataReader_return_loan" 1, 	\
	   "DCPS:DataReader_take" 2,		\
	   "DDS:W_POLL" 3,			\
	   "DDS:W_NOTIF" 4,			\
	   "DDS:W_RX" 5,			\
	   "DDS:W_EV" 6,			\
	   "DDS:W_CONT" 7,			\
	   "DDS:NOTIFY" 8,			\
	   "DCPS:notify_data_avail" 9,		\
	   "RTPS:sfr_be_data" 10,		\
	   "RTPS:rx_data" 11,			\
	   "RTPS:rx_info_ts" 12,		\
	   "RTPS:receive" 13)
set xlabel "Time (seconds)" 
set ylabel "DDS events"
set grid
set yrange [0:14]
#set xrange [2.13087:2.13114]
plot	"/tmp/rrx.dat" using 1:2 notitle,	\
	"/tmp/infts.dat" using 1:2 notitle,	\
	"/tmp/data.dat" using 1:2 notitle,	\
	"/tmp/sfbdata.dat" using 1:2 notitle,	\
	"/tmp/ndavail.dat" using 1:2 notitle,	\
	"/tmp/notif.dat" using 1:2 notitle,	\
	"/tmp/wcont.dat" using 1:2 notitle,	\
	"/tmp/wev.dat" using 1:2 notitle,	\
	"/tmp/wrx.dat" using 1:2 notitle,	\
	"/tmp/wnotif.dat" using 1:2 notitle,	\
	"/tmp/wpoll.dat" using 1:2 notitle,	\
	"/tmp/take.dat" using 1:2 notitle,	\
	"/tmp/rloan.dat" using 1:2 notitle

pause -1

