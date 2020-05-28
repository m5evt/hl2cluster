CC = g++
LINK = g++

OPTIONS = -g -Wall -ggdb

LIBS = -lpthread -pthread

COMPILE = $(CC) $(OPTIONS) $(INCLUDES)

PROGRAM = hl2cluster

SOURCES = \
hl2cluster.cpp \
HL2discover.cpp \
HL2server.cpp \
HL2ipaddr.cpp \
HL2radio.cpp \
HL2udp.cpp 

HEADERS = \
hl2cluster.hh \
HL2discover.hh \
HL2server.hh \
HL2ipaddr.hh \
HL2radio.hh \
HL2udp.hh 

OBJS = \
hl2cluster.o \
HL2discover.o \
HL2server.o \
HL2ipaddr.o \
HL2radio.o \
HL2udp.o 

$(PROGRAM): $(OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(LIBS) 

all: $(PROGRAM) $(HEADERS) $(SOURCES) 

clean:
	-rm -f *.o
	-rm -f $(PROGRAM)
