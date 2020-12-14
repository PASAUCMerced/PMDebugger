#!/bin/bash
cd ../memcached-pmem-master/memcached-pmem-master_bugs
CFLAGS=-Wno-error ./configure --enable-pslab
make -j

valgrind --tool=pmdebugger --print-debug-detail=yes ./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force&



sleep 10
pwd
cd ../../memslap
pwd
LD_LIBRARY_PATH=. ./memslap -s 127.0.0.1:11211 -c 1 -x 10000 -T 1 -X 16384 -F run.cnf -d 1
echo "shutdown"|telnet localhost 11211

