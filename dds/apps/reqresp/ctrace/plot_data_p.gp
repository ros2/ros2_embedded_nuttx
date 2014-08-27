#set term postscript
#set output "testw.ps"
set ytics ("RTPS:rx_acknack" 1,			\
	   "RTPS:tx_heartbeat" 2,		\
	   "RTPS:tx_acknack" 3,			\
	   "RTPS:rx_heartbeat" 4,		\
	   "RTPS:sch_send_done" 5,		\
	   "RTPS:sch_loc_send" 6, 		\
	   "RTPS:tx_data" 7,			\
	   "RTPS:sfw_rel_send" 8,		\
	   "RTPS:sch_w_prepare" 9,		\
	   "DDS:W_IO" 10,			\
	   "DDS:W_POLL" 11,			\
	   "DDS:W_PROXY" 12,			\
	   "DDS:WAKEUP" 13,			\
	   "DDS:W_RX" 14,			\
	   "DDS:W_EV" 15,			\
	   "DDS:W_CONT" 16,			\
	   "DDS:NOTIFY" 17,			\
	   "DCPS:DataReader_return_loan" 18,	\
	   "RTPS:sfw_rel_new_change" 19,	\
	   "RTPS:writer_new_change" 20,		\
	   "DCPS:DataWriter_write" 21,		\
	   "USER:TxResponse" 22,		\
	   "USER:RxRequest" 23,			\
	   "RTPS:sfw_rel_remove_change" 24,	\
	   "RTPS:writer_delete_change" 25,	\
	   "DCPS:DataWriter_unregister" 26,	\
	   "DCPS:DataReader_get_key_value" 27,	\
	   "DCPS:DataReader_take" 28,		\
	   "DCPS:notify_data_avail" 29,		\
	   "RTPS:sfr_rel_data" 30,		\
	   "RTPS:sfr_rx_data" 31,		\
	   "RTPS:sfr_rx_info_ts" 32,		\
	   "RTPS:receive" 33)
set xlabel "Time (seconds)" 
set ylabel "DDS events"
set yrange [0:34]
#set xrange [3.0033:3.004]
set grid
plot	"/tmp/receive.dat" using 1:2 notitle,	\
	"/tmp/sfrrxits.dat" using 1:2 notitle,	\
	"/tmp/sfrrxd.dat" using 1:2 notitle,	\
	"/tmp/sfrreld.dat" using 1:2 notitle,	\
	"/tmp/notifyd.dat" using 1:2 notitle,	\
	"/tmp/drtake.dat" using 1:2 notitle,	\
	"/tmp/gkeyval.dat" using 1:2 notitle,	\
	"/tmp/dwunreg.dat" using 1:2 notitle,	\
	"/tmp/wdelch.dat" using 1:2 notitle,	\
	"/tmp/wrremch.dat" using 1:2 notitle,	\
	"/tmp/rxreq.dat" using 1:2 notitle,	\
	"/tmp/txresp.dat" using 1:2 notitle,	\
	"/tmp/dww.dat" using 1:2 notitle,	\
	"/tmp/wnch.dat" using 1:2 notitle,	\
	"/tmp/relnch.dat" using 1:2 notitle,	\
	"/tmp/drrloan.dat" using 1:2 notitle,	\
	"/tmp/notify.dat" using 1:2 notitle,	\
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
	"/tmp/rhbeat.dat" using 1:2 notitle,	\
	"/tmp/tack.dat" using 1:2 notitle,	\
	"/tmp/thbeat.dat" using 1:2 notitle,	\
	"/tmp/rack.dat" using 1:2 notitle

pause -1

