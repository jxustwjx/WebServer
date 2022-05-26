server:taskqueue.o threadpool.o server.o pub.o
	g++ taskqueue.o threadpool.o server.o pub.o -lpthread -o server

taskqueue.o:taskqueue.cpp
	g++ -c taskqueue.cpp

threadpool.o:threadpool.cpp
	g++ -c threadpool.cpp

server.o:server.cpp
	g++ -c server.cpp

pub.o:pub.cpp
	g++ -c pub.cpp

clean:
	rm taskqueue.o threadpool.o server.o pub.o server
