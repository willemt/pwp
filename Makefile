CONTRIB_DIR = ..
HASHMAP_DIR = $(CONTRIB_DIR)/CHashMapViaLinkedList
BITFIELD_DIR = $(CONTRIB_DIR)/CBitfield
BITSTREAM_DIR = $(CONTRIB_DIR)/CSimpleBitstream
LLQUEUE_DIR = $(CONTRIB_DIR)/CLinkedListQueue
MEANQUEUE_DIR = $(CONTRIB_DIR)/CMeanQueue
SPARSECOUNTER_DIR = $(CONTRIB_DIR)/CSparseCounter

GCOV_OUTPUT = *.gcda *.gcno *.gcov 
GCOV_CCFLAGS = -fprofile-arcs -ftest-coverage
SHELL  = /bin/bash
CC     = gcc
CCFLAGS = -g -O2 -Wall -Werror -Werror=return-type -Werror=uninitialized \
	  -Wcast-align -fno-omit-frame-pointer -fno-common -fsigned-char \
	  $(GCOV_CCFLAGS) -I$(HASHMAP_DIR) -I$(BITFIELD_DIR) -I$(BITSTREAM_DIR) \
	  -I$(LLQUEUE_DIR) -I$(MEANQUEUE_DIR) -I$(SPARSECOUNTER_DIR) \
	  -std=c99

all: tests_connection tests_handler tests_handshaker

chashmap:
	mkdir -p $(HASHMAP_DIR)/.git
	git --git-dir=$(HASHMAP_DIR)/.git init 
	pushd $(HASHMAP_DIR); git pull http://github.com/willemt/CHashMapViaLinkedList; popd

cbitfield:
	mkdir -p $(BITFIELD_DIR)/.git
	git --git-dir=$(BITFIELD_DIR)/.git init 
	pushd $(BITFIELD_DIR); git pull http://github.com/willemt/CBitfield; popd

cbitstream:
	mkdir -p $(BITSTREAM_DIR)/.git
	git --git-dir=$(BITSTREAM_DIR)/.git init 
	pushd $(BITSTREAM_DIR); git pull http://github.com/willemt/CSimpleBitstream; popd

clinkedlistqueue:
	mkdir -p $(LLQUEUE_DIR)/.git
	git --git-dir=$(LLQUEUE_DIR)/.git init 
	pushd $(LLQUEUE_DIR); git pull http://github.com/willemt/CLinkedListQueue; popd

cmeanqueue:
	mkdir -p $(MEANQUEUE_DIR)/.git
	git --git-dir=$(MEANQUEUE_DIR)/.git init 
	pushd $(MEANQUEUE_DIR); git pull http://github.com/willemt/CMeanQueue; popd

csparsecounter:
	mkdir -p $(SPARSECOUNTER_DIR)/.git
	git --git-dir=$(SPARSECOUNTER_DIR)/.git init 
	pushd $(SPARSECOUNTER_DIR); git pull http://github.com/willemt/CSparseCounter; popd


#splint: pwp_connection.c
#	splint pwp_connection.c $@ -I$(HASHMAP_DIR) -I$(BITFIELD_DIR) -I$(BITSTREAM_DIR) -I$(LLQUEUE_DIR) -I$(MEANQUEUE_DIR) -I$(SPARSECOUNTER_DIR) +boolint -mustfreeonly -immediatetrans -temptrans -exportlocal -onlytrans -paramuse +charint

downloadcontrib: chashmap cbitfield cbitstream clinkedlistqueue cmeanqueue csparsecounter

main_connection.c:
	if test -d $(HASHMAP_DIR); then echo have; else make downloadcontrib; fi
	sh make-tests.sh "tests/test_connection*.c" > main_connection.c

main_msghandler.c:
	sh make-tests.sh "tests/test_msghandler.c" > main_msghandler.c

main_handshaker.c:
	sh make-tests.sh "tests/test_handshaker.c" > main_handshaker.c

tests_handler: main_msghandler.c pwp_msghandler.c tests/test_msghandler.c tests/CuTest.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c
	$(CC) $(CCFLAGS) -I. -o $@ $^
	./tests_handler
	gcov main_msghandler.c tests/test_msghandler.c pwp_msghandler.c

tests_handshaker: main_handshaker.c pwp_handshaker.c tests/test_handshaker.c tests/CuTest.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c 
	$(CC) $(CCFLAGS) -I. -o $@ $^
	./tests_handshaker
	gcov main_handshaker.c tests/test_handshaker.c pwp_handshaker.c

tests_connection: main_connection.c pwp_connection.o pwp_msghandler.c pwp_bitfield.c pwp_util.c tests/test_connection.c tests/test_connection_send.c tests/mock_caller.c tests/mock_piece.c tests/bt_diskmem.c tests/CuTest.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c $(LLQUEUE_DIR)/linked_list_queue.c $(MEANQUEUE_DIR)/meanqueue.c $(SPARSECOUNTER_DIR)/sparse_counter.c 
	$(CC) $(CCFLAGS) -I. -o $@ $^
	./tests_connection
	gcov main_connection.c tests/test_connection.c tests/test_connection_send.c pwp_connection.c
pwp_connection.o: pwp_connection.c 
	$(CC) $(CCFLAGS) -c -o $@ $^

clean:
	rm -f main_connection.c main_msghandler.c main_handshaker.c *.o $(GCOV_OUTPUT)
