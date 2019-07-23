#####	RELIABLE UDP MAKEFILE	########################
CC=gcc
#libs
CFLAGS=-pthread -lm -pthread -lpthread -lrt #-Wall
#add header in path of compilation
CFLAGS+= -I headers -I .
###GDB MAX COMPATIBILITY
#CFLAGS+= -ggdb
##GCC OPTIMIZATION
#CFLAGS+= -O2
#dep for fcntl and seek64
CFLAGS+=  -D _LARGEFILE64_SOURCE -D _GNU_SOURCE

#TODO APP BASIC CONFIGS #######################################
###FIXED TIMEOUT TODO COMMENT FOR ADAPTIVE TIMEOUT
CFLAGS+= -D TIMEOUTFIX
###USE DEFAULT (MACRO DEFINED) FOR TX CONFIGURATION TODO DECOMMENT
#CFLAGS+= -D TX_CONFIG_FIXED
###########################################################

################		TESTS EXTRA CONFIG	#############
TESTCONFIG_MACRO =
TESTCONFIG_MACRO+= -D TEST_QUIET_PRINT		#DISABLE SOME PRINT DURING TEST
### PCKPAYLOAD WILL BE READED FROM A FILE NAMED pcksize,
### RELATED MACRO WILL BE SETTED IF THE FILE IS FOUNDED
_PCKPAYLOAD=$(shell cat pcksize)
ifneq ($(_PCKPAYLOAD),)						#TRUE IF NOT EMPTY=> FOUNDED DATA IN FILE
TESTCONFIG_MACRO+= -D PCKPAYLOADSIZE=$(_PCKPAYLOAD)	#set pcksize from file
endif
###TODO NEXT MACRO TRIGGER GCC ATOMIC VERSION OF SENDER/RECEIVER
#TESTCONFIG_MACRO+= -D GCCATOMICS		#decomment for gcc atomics trigger on test
#############################################################

##basic source files vars
CLIENT=client/clientStart.c
SERVER=server/serverBis.c
CFILES=$(shell find . -iname "*.c" )
CFILES+=$(shell find . -iname "*.lc" )
HEADERS= headers/*

all: client.o server.o
print:
	@echo ${TESTCONFIG_MACRO} $(_EXIT_SUCCESS)

server.o: $(HEADERS) $(CFILES) $(HEADERS)
	$(CC) -o $@ $(SERVER) $(CFLAGS)

client.o: $(HEADERS) $(CFILES) $(HEADERS)
	$(CC) -o $@ $(CLIENT) $(CFLAGS)
test: all
	$(CC) -o tests/test.o tests/trasmissionTest.c $(CFLAGS) $(TESTCONFIG_MACRO)
	$(CC) -o tests/test_concurrent.o tests/multiTX_test.c $(CFLAGS)

generate_files:
	$(shell cd ../files_sendable && ./generateNullFile.sh)
clean:
	rm server.o client.o
clean_tests:
	rm tests/test.o tests/test_concurrent.o


