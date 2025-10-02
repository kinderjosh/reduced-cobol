CC = gcc
SRCS = $(wildcard src/*.c)
EXEC = cobc

DEBUG ?= 0
CFLAGS = -Wall -Wextra -Wpedantic -Wno-missing-braces -std=c11 -march=native

ifeq ($(DEBUG),1)
CFLAGS += -g -Wl,-z,now -Wl,-z,relro \
	  -fsanitize=undefined,address \
	  -fstack-protector-strong \
	  -ftrampolines \
	  -ftrivial-auto-var-init=pattern
else
CFLAGS += -s -O3 -DNDEBUG
endif

.PHONY: all clean install uninstall

all: $(EXEC)

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@

clean:
ifeq ($(OS),Windows_NT)
	del /q .\$(EXEC).exe
else
	rm -f ./$(EXEC)
endif

install:
	make
ifneq ($(OS),Windows_NT)
	cp ./$(EXEC) /usr/local/bin/
endif

uninstall:
ifneq ($(OS),Windows_NT)
	rm /usr/local/bin/$(EXEC)
endif