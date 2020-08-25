CC = gcc
CFLAGS = -g -W

INCLUDE = ./include
CFLAGS += -I$(INCLUDE)

LIB_DIR = ./lib
LIBS = ${LIB_DIR}/libzone.o ${LIB_DIR}/bitmap.o

DLL = zbc


default: build-libs test

test: 
	$(CC) $(CFLAGS) -l$(DLL) $(LIBS) main.c -o test

build-libs:
	$(CC) $(CFLAGS) -c ${LIB_DIR}/libzone.c -o ${LIB_DIR}/libzone.o
	$(CC) $(CFLAGS) -c ${LIB_DIR}/bitmap.c -o ${LIB_DIR}/bitmap.o


libzone.o: 
	$(CC) $(CFLAGS) -c ${LIB_DIR}/libzone.c -o ${LIB_DIR}/libzone.o




clean: 
	rm -f ./*.o
	rm -f ${LIB_DIR}/*.o
	rm -f ./test
