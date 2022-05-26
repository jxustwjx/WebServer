src=$(wildcard threadpool.cpp taskqueue.cpp server.cpp pub.cpp)
obj=$(patsubst %.cpp, %.o, $(src))

target=server

$(target):$(obj)
	g++ $(obj) -lpthread -o $(target)

%.o:%.c   # %: 匹配文件
	g++ -c $<

.PHONY:clean #声明为伪文件
clean:
	rm $(obj) $(target)
