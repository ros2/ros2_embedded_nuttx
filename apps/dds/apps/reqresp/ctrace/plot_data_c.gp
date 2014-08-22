#set term postscript
#set output "testw.ps"
set ytics ("RTPS:rx_acknack" 1,			\
	   "RTPS:tx_heartbeat" 2,		\
	   "RTPS:tx_acknack" 3,			\
	   "RTPS:rx_heartbeat" 4,		\
	   "DCPS:DataReader_return_loan" 5, 	\
	   "RTPS:sfw_rel_remove_change" 6, 	\
	   "RTPS:writer_delete_change" 7,	\
	   "DCPS:DataWriter_unregister" 8,	\
	   "DCPS:DataWriter_lookup_inst" 9,	\
	   "USER:RxResponse" 10,			\
	   "DCPS:DataReader_take" 11,		\
	   "DCPS:notify_data_avail" 12,		\
	   "RTPS:sfr_rel_data" 13,		\
	   "RTPS:sfr_rx_data" 14,		\
	   "RTPS:sfr_rx_info_ts" 15,		\
	   "RTPS:receive" 16,			\
	   "RTPS:sch_send_done" 17,		\
	   "RTPS:sch_loc_send" 18,		\
	   "RTPS:tx_data" 19,			\
	   "RTPS:sfw_rel_send" 20,		\
	   "RTPS:sch_w_prepare" 21,		\
	   "DDS:W_IO" 22,			\
	   "DDS:W_POLL" 23,			\
	   "DDS:W_PROXY" 24,			\
	   "DDS:WAKEUP" 25,			\
	   "DDS:W_RX" 26,			\
	   "DDS:W_EV" 27,			\
	   "DDS:W_CONT" 28,			\
	   "USER:TxWait" 29,			\
	   "RTPS:sfw_rel_new_change" 30,	\
	   "DCPS:DataWriter_write" 31,		\
	   "USER:TxRequest" 32)
set xlabel "Time (seconds)" 
set ylabel "DDS events"
set yrange [0:33]
#set xrange [3.0033:3.004]
set grid
plot	"/tmp/txreq.dat" using 1:2 notitle,	\
	"/tmp/dww.dat" using 1:2 notitle,	\
	"/tmp/relnch.dat" using 1:2 notitle,	\
	"/tmp/txwait.dat" using 1:2 notitle,	\
	"/tmp/wcont.dat" using 1:2 notitle,	\
	"/tmp/wev.dat" using 1:2 notitle,	\
	"/tmp/wrx.dat" using 1:2 notitle,	\
	"/tmp/wakeup.dat" using 1:2 notitle,	\
	"/tmp/wproxy.dat" using 1:2 notitle,	\
	"/tmp/wpoll.dat" using 1:2 notitle,	\
	"/tmp/wio.dat" using 1:2 notitle,	\
	"/tmp/wprep.dat" using 1:2 notitle,	\
	"/tmp/rsend.dat" using 1:2 notitle,	\
	"/tmp/txdata.dat" using 1:2 notitle,	\
	"/tmp/lsend.dat" using 1:2 notitle,	\
	"/tmp/sendd.dat" using 1:2 notitle,	\
	"/tmp/receive.dat" using 1:2 notitle,	\
	"/tmp/sfrrxits.dat" using 1:2 notitle,	\
	"/tmp/sfrrxd.dat" using 1:2 notitle,	\
	"/tmp/sfrreld.dat" using 1:2 notitle,	\
	"/tmp/notifyd.dat" using 1:2 notitle,	\
	"/tmp/drtake.dat" using 1:2 notitle,	\
	"/tmp/rxresp.dat" using 1:2 notitle,	\
	"/tmp/dwlookup.dat" using 1:2 notitle,	\
	"/tmp/dwunreg.dat" using 1:2 notitle,	\
	"/tmp/wdelch.dat" using 1:2 notitle,	\
	"/tmp/wrremch.dat" using 1:2 notitle,	\
	"/tmp/drrloan.dat" using 1:2 notitle,	\
	"/tmp/rhbeat.dat" using 1:2 notitle,	\
	"/tmp/tack.dat" using 1:2 notitle,	\
	"/tmp/thbeat.dat" using 1:2 notitle,	\
	"/tmp/rack.dat" using 1:2 notitle

pause -1

