# TcpSvr
Tcp Server based on thread pool + epoll to achieve high efficiency and reliability.

# build
	# cd TcpSvr/
	# ls
	include  lib  LICENSE  Makefile  README.md  tcpsvr.cpp  tcpsvr.h  test
	# make
	g++ -I./include/json -I./include -Wno-deprecated -DBNO=\"20200514-1355\" -std=c++11 tcpsvr.cpp -c
	g++ -o TcpServer tcpsvr.o -lpthread -L./lib/ -lZLToolKit
	strip TcpServer

	# cd test/
	# make
	g++ -I../include/ -Wno-deprecated -DBNO=\"20200514-1356\" -std=c++11 tcpclt.cpp -c
	g++ -o tcpclt tcpclt.o -lpthread -L../lib/ -lZLToolKit
	strip tcpclt
# note
suggest to use ubuntu or debian to build. It can not be compiled pass on centos7.x

