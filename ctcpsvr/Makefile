#CROSS=
AR=$(CROSS)ar
CC=$(CROSS)gcc
CXX=$(CROSS)g++
STRIP=$(CROSS)strip

DATE=$(shell date "+%Y%m%d-%H%M")

CFLAGS += -Wall -Wno-write-strings -Wno-unused-but-set-variable -Wno-deprecated -Wno-unused-variable -DBNO=\"$(DATE)\" -c -O2 -Wno-unused-function
CFLAGS += -std=c++11
LDFLAGS=

INC+=-I./include/hiredis -I./include/json
LIBS= -lpthread -lm -lc -lrt  ./lib/libhiredis.a

EXEC = TcpServer
OBJS = tcpsvr.o vos.o netsock.o cfgfile.o logfile.o 

all:$(EXEC)

$(EXEC): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)
	$(STRIP) $(EXEC)
#       cp $@ ../$(HOST)

clean:
	rm -rf $(OBJS) $(EXEC) *.a

%.o: %.cpp
	$(CXX) -c $< ${CFLAGS} $(INC)

%.o: %.c
	$(CC) -c $< ${CFLAGS} $(INC)

%: %.cpp
	$(CXX) -o $@ $< $(INC) $(LIBS)

%: %.c
	$(CC) -o $@ $< $(INC) $(LIBS)
