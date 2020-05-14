CC=$(CROSS)g++
STRIP=$(CROSS)strip

DATE=$(shell date "+%Y%m%d-%H%M")

LDFLAG=
CFLAG= -Wno-deprecated -DBNO=\"$(DATE)\" -std=c++11

INC= -I../include/
LIBS=  -lpthread -L../lib/ -lZLToolKit

EXEC = tcpclt
OBJS = tcpclt.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)
	$(STRIP) $@
	
$(OBJS):%.o: %.cpp
	$(CC) $(INC) $(CFLAG) $< -c

clean:
	rm -rf $(OBJS) $(EXEC)
