#set term postscript
#set output "testw.ps"
set ytics ("RTPS:sch_send_done" 1,		\
	   "RTPS:sch_loc_send" 2, 		\
	   "RTPS:tx_data" 3,			\
	   "RTPS:sfw_be_send" 4,		\
	   "RTPS:sch_w_prepare" 5,		\
	   "DDS:W_POLL" 6,			\
	   "DDS:W_PROXY" 7,			\
	   "DDS:WAKEUP" 8,			\
	   "DDS:W_RX" 9,			\
	   "DDS:W_EV" 10,			\
	   "DDS:W_CONT" 11,			\
	   "RTPS:sfw_be_new_change" 12,		\
	   "RTPS:writer_new_change" 13,		\
	   "DCPS:DataWriter_write" 14,		\
	   "USER:TxDelay" 15,			\
	   "USER:TxBurst" 16)
set xlabel "Time (seconds)" 
set ylabel "DDS events"
set yrange [0:17]
#set xrange [3.0033:3.004]
set grid
plot	"/tmp/txburst.dat" using 1:2 notitle,	\
	"/tmp/txdelay.dat" using 1:2 notitle,	\
	"/tmp/dww.dat" using 1:2 notitle,	\
	"/tmp/wnch.dat" using 1:2 notitle,	\
	"/tmp/bench.dat" using 1:2 notitle,	\
	"/tmp/wcont.dat" using 1:2 notitle,	\
	"/tmp/wev.dat" using 1:2 notitle,	\
	"/tmp/wrx.dat" using 1:2 notitle,	\
	"/tmp/wakeup.dat" using 1:2 notitle,	\
	"/tmp/wproxy.dat" using 1:2 notitle,	\
	"/tmp/wpoll.dat" using 1:2 notitle,	\
	"/tmp/wprep.dat" using 1:2 notitle,	\
	"/tmp/besend.dat" using 1:2 notitle,	\
	"/tmp/txdata.dat" using 1:2 notitle,	\
	"/tmp/lsend.dat" using 1:2 notitle,	\
	"/tmp/sendd.dat" using 1:2 notitle

pause -1

