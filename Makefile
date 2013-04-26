CONTRIB_DIR = ..
HASHMAP_DIR = $(CONTRIB_DIR)/CHashMapViaLinkedList
BITFIELD_DIR = $(CONTRIB_DIR)/CBitfield
BITSTREAM_DIR = $(CONTRIB_DIR)/CBitstream

GCOV_OUTPUT = *.gcda *.gcno *.gcov 
GCOV_CCFLAGS = -fprofile-arcs -ftest-coverage
SHELL  = /bin/bash
CC     = gcc
CCFLAGS = -g -O2 -Wall -Werror -W -fno-omit-frame-pointer -fno-common -fsigned-char $(GCOV_CCFLAGS) -I$(HASHMAP_DIR) -I$(BITFIELD_DIR) -I$(BITSTREAM_DIR)

all: tests

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


download-contrib: chashmap cbitfield cbitstream

main.c:
	if test -d $(HASHMAP_DIR); \
	then echo have contribs; \
	else make download-contrib; \
	fi
	sh make-tests.sh > main.c

tests: main.c pwp_connection.o test_pwp_connection.c mock_piece.c bt_diskmem.c CuTest.c main.c $(HASHMAP_DIR)/linked_list_hashmap.c $(BITFIELD_DIR)/bitfield.c $(BITSTREAM_DIR)/bitstream.c 
	$(CC) $(CCFLAGS) -o $@ $^
	./tests
	gcov main.c test_pwp_connection.c pwp_connection.c

pwp_connection.o: pwp_connection.c 
	$(CC) $(CCFLAGS) -c -o $@ $^

clean:
	rm -f main.c *.o tests $(GCOV_OUTPUT)