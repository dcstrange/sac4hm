CC = gcc
CFLAGS = -std=c99 -g -W 

# OBJS
INCLUDE_DIR = ./include

LIB_DIR = ./lib
OBJ_LIBS = ${LIB_DIR}/libzone.o 

UTIL_DIR = ./util
OBJ_UTILS =	${UTIL_DIR}/bitmap.o \
			${UTIL_DIR}/hashtable.o \
			${UTIL_DIR}/xstrtoumax.o

SRC_DIR = ./src
OBJ_ALGORITHM = ${SRC_DIR}/zbd-cache.o

STRATEGY_DIR = ./src/strategy
OBJ_STRATEGIES = ${STRATEGY_DIR}/cars.o \
				 ${STRATEGY_DIR}/most.o

TOOLS_DIR = ./tools
OBJ_TOOLS = ${TOOLS_DIR}/zbd_set_full

# DLL
DLL = zbc


CFLAGS += -I$(INCLUDE_DIR) -I$(LIB_DIR) -I$(UTIL_DIR) -I$(SRC_DIR) -I$(STRATEGY_DIR)


default: build-libs build-utils build-algorithm build-tools test

test: 
	$(CC) $(CFLAGS) -l$(DLL) $(OBJ_LIBS) $(OBJ_UTILS) $(OBJ_ALGORITHM) $(OBJ_STRATEGIES) main.c -o test

build-libs:
	$(CC) $(CFLAGS) -c ${LIB_DIR}/libzone.c -o ${LIB_DIR}/libzone.o

build-utils:
	$(CC) $(CFLAGS) -c ${UTIL_DIR}/bitmap.c -o ${UTIL_DIR}/bitmap.o
	$(CC) $(CFLAGS) -c ${UTIL_DIR}/hashtable.c -o ${UTIL_DIR}/hashtable.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -c ${UTIL_DIR}/xstrtoumax.c -o ${UTIL_DIR}/xstrtoumax.o

build-algorithm:
	$(CC) $(CFLAGS) -c ${SRC_DIR}/zbd-cache.c -o ${SRC_DIR}/zbd-cache.o
	$(CC) $(CFLAGS) -c ${STRATEGY_DIR}/cars.c -o ${STRATEGY_DIR}/cars.o
	$(CC) $(CFLAGS) -c ${STRATEGY_DIR}/most.c -o ${STRATEGY_DIR}/most.o

build-tools:
	$(CC) $(CFLAGS) -l$(DLL) ${TOOLS_DIR}/zbd_set_full.c -o ${TOOLS_DIR}/zbd_set_full

clean: 
	rm -f ./*.o

	rm -f ${LIB_DIR}/*.o
	rm -f ${UTIL_DIR}/*.o
	rm -f ${ALGORITHM}/*.o
	rm -f ${SRC_DIR}/*.o
	rm -f ${STRATEGY_DIR}/*.o
	rm -f ${OBJ_TOOLS}

	rm -f ./test
