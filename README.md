Implementation of Selective Repete over berkeley UDP socket
used to implement a FTP like server
final project for my becheleor degree :) 30L
core struct supporting the implementation of the protocol is a scoreboard housing a lockless ring buffer
posix thread used to implement producer and consumers that read/write pcks from the ring buffer 
also there is a version using the gcc atomics to implement the spin locks with a propagation of changed data among threads running on different core at hardware level (actually at the time I've little knowing of that so I know that part can be done better )

on the sender read and write operation on the socket are done concurrently so it can be sended a pck during a reception of ack
	(that will cause the motion of the pck window)
more detailed information in DOCs 

also intresting scripts to tune the protocol and app paramters quickly
	a script interact with the Makefile allowing an automatized reconfiguration and rebuild and time mesuring	

python GUI binded with the C client with an amazing trick:
	basically a forked process that will run the simple python GUI will have stdin and stdout redirect to pipes,
	that pipes are used in C trivially with R/W
	that trick used also for whole test of the application
Hopefully Usefull :)
Andrea Di Iorio
