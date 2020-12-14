#!/bin/bash
for ((i=0;i<1;i++))
do
    echo "$i"
cd ../memcached-pmem-master

if [ $1 == "pmemcheck" ]
then
valgrind --tool=pmemcheck ./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force&

elif [ $1 == "pmdebugger" ]
then
valgrind --tool=pmdebugger ./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force&

else
echo "Unknow checker"
fi

sleep 10
pwd
cd ../memslap
pwd
LD_LIBRARY_PATH=. ./memslap -s 127.0.0.1:11211 -c 1 -x $2 -T 1 -X 16384 -F run.cnf -d 1 
echo "shutdown"|telnet localhost 11211

done
grep "Run time:" time_$2.txt
