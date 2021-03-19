pool:threadpool.c wrap.c pub.c
	gcc threadpool.c wrap.c pub.c -o pool -pthread