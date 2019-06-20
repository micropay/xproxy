src = $(wildcard *.c)
obj = $(patsubst %.c, %.o, $(src))

all: xproxy

xproxy: main.o wrap.o
	gcc main.o wrap.o -o xproxy -lpthread -g -Wall

%.o:%.c
	gcc -c $< -g -Wall

.PHONY: clean all
clean:
	-rm -rf xproxy $(obj)
