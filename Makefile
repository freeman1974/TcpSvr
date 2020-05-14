CC=$(CROSS)g++
STRIP=$(CROSS)strip

DATE=$(shell date "+%Y%m%d-%H%M")

LDFLAG=
CFLAG= -Wno-deprecated -DBNO=\"$(DATE)\" -std=c++11

INC= -I./include/json -I./include
LIBS=  -lpthread -L./lib/ -lZLToolKit

EXEC = TcpServer
OBJS = tcpsvr.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)
	$(STRIP) $@
	
$(OBJS):%.o: %.cpp
	$(CC) $(INC) $(CFLAG) $< -c

clean:
	rm -rf $(OBJS) $(EXEC)