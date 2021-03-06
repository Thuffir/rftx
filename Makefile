TARGET = rftx
CC = gcc
CFLAGS = -O3 -Wall -fomit-frame-pointer
LIBS = -lpigpio -lpthread -lrt
LFLAGS = -s

INSTALLDIR = /opt/fhem
INSTALL = sudo install -m 4755 -o root -g root

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(LFLAGS) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)

install: $(TARGET)
	$(INSTALL) -s $(TARGET) $(INSTALLDIR)
