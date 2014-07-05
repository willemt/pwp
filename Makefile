
GCOV_OUTPUT = *.gcda *.gcno *.gcov 
GCOV_CCFLAGS = -fprofile-arcs -ftest-coverage
SHELL  = /bin/bash
CC     = gcc
INCLUDES = $(shell ls deps | sed 's/^/-Ideps\//')
DEPS_SRC = $(shell find deps | grep \.c$$)
CCFLAGS = -I. -Itests -g -O2 -Wall -Werror -Werror=return-type -Werror=uninitialized \
	  -Wcast-align -fno-omit-frame-pointer -fno-common -fsigned-char \
	  $(GCOV_CCFLAGS) \
	  $(INCLUDES) \
	  -Ideps/fe
#	  -std=c99

all: tests_connection tests_handler tests_handshaker

#splint: pwp_connection.c
#	splint pwp_connection.c $@ -I$(HASHMAP_DIR) -I$(BITFIELD_DIR) -I$(BITSTREAM_DIR) -I$(LLQUEUE_DIR) -I$(MEANQUEUE_DIR) -I$(SPARSECOUNTER_DIR) +boolint -mustfreeonly -immediatetrans -temptrans -exportlocal -onlytrans -paramuse +charint

downloadcontrib: chashmap cbitfield cbitstream clinkedlistqueue cmeanqueue csparsecounter

main_connection.c:
	sh make-tests.sh "tests/test_connection*.c" > main_connection.c

main_msghandler.c:
	sh make-tests.sh "tests/test_msghandler.c" > main_msghandler.c

main_handshaker.c:
	sh make-tests.sh "tests/test_handshaker.c" > main_handshaker.c

tests_handler: main_msghandler.c pwp_msghandler.c tests/test_msghandler.c tests/CuTest.c $(DEPS_SRC) 
	$(CC) $(CCFLAGS) -I. -o $@ $^
	./tests_handler
	gcov main_msghandler.c tests/test_msghandler.c pwp_msghandler.c

tests_handshaker: main_handshaker.c pwp_handshaker.c tests/test_handshaker.c tests/CuTest.c $(DEPS_SRC) 

	$(CC) $(CCFLAGS) -I. -o $@ $^
	./tests_handshaker
	gcov main_handshaker.c tests/test_handshaker.c pwp_handshaker.c

tests_connection: main_connection.c pwp_connection.o pwp_msghandler.c pwp_bitfield.c deps/fe/fe.c tests/test_connection.c tests/test_connection_send.c tests/mock_caller.c tests/mock_piece.c tests/bt_diskmem.c tests/CuTest.c  $(DEPS_SRC) 
	$(CC) $(CCFLAGS) -I. -o $@ $^
	./tests_connection
	gcov main_connection.c tests/test_connection.c tests/test_connection_send.c pwp_connection.c

pwp_connection.o: pwp_connection.c 
	$(CC) $(CCFLAGS) -c -o $@ $^

clean:
	rm -f main_connection.c main_msghandler.c main_handshaker.c *.o $(GCOV_OUTPUT)
