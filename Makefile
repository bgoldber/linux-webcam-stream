CC=gcc
CFLAGS=-Wall
DEBUG=-g
LDLIBS=-lv4l2
SOURCES=cameraCapture.c
EXE_NAME=capture

# Build all camera capture components
all:
	$(CC) $(SOURCES) $(CFLAGS) $(LDLIBS) -o $(EXE_NAME)

# Delete all intermediate files and executables
clean:
	rm -rf *o $(EXE_NAME)
