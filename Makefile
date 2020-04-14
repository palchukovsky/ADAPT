CC = g++
CFLAGS  = -g -Wall -Wfatal-errors -std=c++17
SRC = src/Main.cpp src/Environment.cpp
OBJ = Main.o Environment.o
TARGET = adapt-test

$(TARGET):
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
