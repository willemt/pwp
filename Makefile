CONTRIB_DIR = ..
HASHMAP_DIR = $(CONTRIB_DIR)/CHashMapViaLinkedList
BITFIELD_DIR = $(CONTRIB_DIR)/CBitfield
BITSTREAM_DIR = $(CONTRIB_DIR)/CBitstream
LLQUEUE_DIR = $(CONTRIB_DIR)/CLinkedListQueue

GCOV_OUTPUT = *.gcda *.gcno *.gcov 
GCOV_CCFLAGS = -fprofile-arcs -ftest-coverage
SHELL  = /bin/bash
CC     = gcc
CCFLAGS = -g -O2 -Wall -Werror -Werror=return-type -Werror=uninitialized -Wcast-align -fno-omit-frame-pointer -fno-common -fsigned-char $(GCOV_CCFLAGS) -I$(HASHMAP_DIR) -I$(BITFIELD_DIR) -I$(BITSTREAM_DIR) -I$(LLQUEUE_DIR)

all: tests_connection tests_handler tests_handshaker

chashmap:
	mkdir -p $(HASHMAP_DIR)/.git
	git --git-dir=$(HASHMAP_DIR)/.git init 
	pushd $(HASHMAP_DIR); git pull git@github.com:willemt/CHashMapViaLinkedList.git; popd

cbitfield:
	mkdir -p $(BITFIELD_DIR)/.git
	git --git-dir=$(BITFIELD_DIR)/.git init 
	pushd $(BITFIELD_DIR); git pull git@github.com:willemt/CBitfield.git; popd

cbitstream:
	mkdir -p $(BITSTREAM_DIR)/.git
	git --git-dir=$(BITSTREAM_DIR)/.git init 
	pushd $(BITSTREAM_DIR); git pull git@github.com:willemt/CBitstream.git; popd

clinkedlistqueue:
	mkdir -p $(LLQUEUE_DIR)/.git
	git --git-dir=$(LLQUEUE_DIR)/.git init 
	pushd $(LLQUEUE_DIR); git pull git@github.com:willemt/CLinkedListQueue.git; popd

splint: pwp_connection.c
	splint pwp_connection.c $@ -I$(HASHMAP_DIR) -I$(BITFIELD_DIR) -I$(BITSTREAM_DIR) -I$(LLQUEUE_DIR) +boolint -mustfreeonly -immediatetrans -temptrans -exportlocal -onlytrans -paramuse +charint

download-contrib: chashmap cbitfield cbitstream clinkedlistqueue

main_connection.c:
	if test -d $(HASHMAP_DIR); \
	then echo have contribs; \
	else make download-contrib; \
	fi
	sh make-tests.sh "test_connection*.c" > main_connection.c

main_msghandler.c:
	sh make-tests.sh "test_msghandler.c" > main_msghandler.c

main_handshaker.c:
	sh make-tests.sh "test_handshaker.c" > main_handshaker.c

tests_handler: main_msghandler.c pwp_msghandler.c test_msghandler.c CuTest.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c
	$(CC) $(CCFLAGS) -o $@ $^
	./tests_handler
	gcov main_msghandler.c test_msghandler.c pwp_msghandler.c

tests_handshaker: main_handshaker.c pwp_handshaker.c test_handshaker.c CuTest.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c 
	$(CC) $(CCFLAGS) -o $@ $^
	./tests_handshaker
	gcov main_handshaker.c test_handshaker.c pwp_handshaker.c


tests_connection: main_connection.c pwp_connection.o pwp_msghandler.c test_connection.c test_connection_send.c test_connection_mock_functions.c mock_piece.c bt_diskmem.c CuTest.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c
	$(CC) $(CCFLAGS) -o $@ $^
	./tests_connection
	gcov main_connection.c test_connection.c test_connection_send.c pwp_connection.c

pwp_connection.o: pwp_connection.c 
	$(CC) $(CCFLAGS) -c -o $@ $^

clean:
	rm -f main_connection.c main_msghandler.c main_handshaker.c *.o tests $(GCOV_OUTPUT)
