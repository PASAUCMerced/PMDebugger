#!/bin/bash
export LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH
echo 0 >tmp_memcached.txt
for ((i=0;i<2;i++))
do
    #echo "$i"
cd ../memcached-pmem-master

if [ $1 == "pmemcheck" ]
then
valgrind --tool=pmemcheck ./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force &> /dev/null&

elif [ $1 == "pmdebugger" ]
then
valgrind --tool=pmdebugger ./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force &> /dev/null&

elif [ $1 == "Nulgrind" ]
then
valgrind --tool=none ./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force  &> /dev/null &

elif [ $1 == "original" ]
then
./memcached -A -m 0 -o pslab_file=/mnt/dbpmemfs/pool,pslab_force &> /dev/null & 

else

echo "Unknow checker"
fi

sleep 10
#pwd
cd ../memslap
#pwd
LD_LIBRARY_PATH=. ./memslap -s 127.0.0.1:11211 -c 1 -x $2 -T 1 -X 16384 -F run.cnf -d 1 >>tmp_memcached.txt
echo "shutdown"|telnet localhost 11211 &> /dev/null

done
echo "execution time"
grep "Run time:" tmp_memcached.txt | awk '{sum+=$3;num++} END{print sum/num;}'
