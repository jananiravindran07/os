# Makefile - User Authentication and Access Control System
#
#   make          build the project (uac.exe on Windows, ./uac on Linux)
#   make run      build and start the program
#   make clean    remove build artefacts
#   make rebuild  clean + build

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O2
LDFLAGS =
TARGET  = uac

# Windows specific libraries (console password input)
ifeq ($(OS),Windows_NT)
    TARGET = uac.exe
    RUN     = $(TARGET)
else
    RUN     = ./$(TARGET)
endif

SOURCES = main.c auth.c user.c password.c session.c \
          access_control.c admin.c logger.c database.c utils.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = auth.h user.h password.h session.h access_control.h \
          admin.h logger.h database.h utils.h

.PHONY: all run clean rebuild

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	$(RUN)

clean:
	$(RM) $(OBJECTS) $(TARGET) $(TARGET).exe

rebuild: clean all