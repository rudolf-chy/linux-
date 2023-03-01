all: http_conn.o webserver

http_conn.o: http_conn.cpp http_conn.h
	g++ -c http_conn.cpp -o http_conn.o
webserver: http_conn.o locker.h threadpool.h
	g++ main.cpp http_conn.o -o webserver -lpthread 

clean:
	rm *.o webserver
