app:taskqueue.o threadpool.o server.o main.o
	g++ taskqueue.o threadpool.o server.o main.o -lpthread -o app

taskqueue.o:taskqueue.cpp
	g++ -c taskqueue.cpp

threadpool.o:threadpool.cpp
	g++ -c threadpool.cpp

server.o:server.cpp
	g++ -c server.cpp

main.o:main.cpp
	g++ -c main.cpp

clean:
	rm taskqueue.o threadpool.o server.o main.o app
