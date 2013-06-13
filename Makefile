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

all: tests tests_pwphandler

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

main.c:
	if test -d $(HASHMAP_DIR); \
	then echo have contribs; \
	else make download-contrib; \
	fi
	sh make-tests.sh test_pwp_connection*.c > main.c

main_pwphandler.c:
	sh make-tests.sh test_pwphandler.*c > main_pwphandler.c

tests_pwphandler: main_pwphandler.c pwp_connection.o test_pwphandler.c CuTest.c main.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c
	$(CC) $(CCFLAGS) -o $@ $^
	./tests
	gcov main_pwphandler.c test_pwphandler.c pwp_msghandler.c

tests: main.c pwp_connection.o test_pwp_connection.c test_pwp_connection_handshake.c test_pwp_connection_handshake.c test_pwp_connection_send.c test_pwp_connection_mock_functions.c mock_piece.c bt_diskmem.c CuTest.c main.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c
	$(CC) $(CCFLAGS) -o $@ $^
	./tests
	gcov main.c test_pwp_connection.c test_pwp_connection_send.c pwp_connection.c

pwp_connection.o: pwp_connection.c 
	$(CC) $(CCFLAGS) -c -o $@ $^

clean:
	rm -f main.c *.o tests $(GCOV_OUTPUT)
