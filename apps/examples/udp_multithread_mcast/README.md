UDP Multithread-Multicast
=========================

This code provides a reproducible example that shows a corner case within NuttX when dealing with sockets:

It seems like: *the same socket can't be used for writing and reading simultaneously*. Although not common, this scenario works in POSIX. 

The example codes an application that launches 5 reading (UDP) threads: 4 threads reading from a unicast socket and 1 thread reading from a multicast thread. After 10 seconds the application writes using the multicast socket. A script `test.sh` is included that writes packets in the different sockets created. After 10 seconds the user can appreciate that the multicast socket (the one used to write) doesn't receive anymore.