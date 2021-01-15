#!/bin/bash
export LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH
if [[ $1 == 'pmdebugger' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=pmdebugger ./src/redis-server redis.conf &> /dev/null &
sleep 50
echo "throughput (Get/Sec)"
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2 |& grep "Gets/sec" | awk '{sum+=$1;num++} END{print sum/num;}'
echo "shutdown" | src/redis-cli  &> /dev/null

elif [[ $1 == 'pmemcheck' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=pmemcheck ./src/redis-server redis.conf  &> /dev/null &
#./src/redis-server redis.conf &
sleep 50
echo "throughput (Get/Sec)"
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2 |& grep "Gets/sec" | awk '{sum+=$1;num++} END{print sum/num;}'
echo "shutdown" | src/redis-cli  &> /dev/null
rm /mnt/dbpmemfs/redis.pm

elif [[ $1 == 'Nulgrind' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=none ./src/redis-server redis.conf   &> /dev/null &
#./src/redis-server redis.conf &
sleep 50
echo "throughput (Get/Sec)"
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2 |& grep "Gets/sec" | awk '{sum+=$1;num++} END{print sum/num;}'
echo "shutdown" | src/redis-cli  &> /dev/null
rm /mnt/dbpmemfs/redis.pm

elif [[ $1 == 'original' ]]
then
rm /mnt/dbpmemfs/redis.pm
./src/redis-server redis.conf  &> /dev/null &
#./src/redis-server redis.conf &
sleep 50
echo "throughput (Get/Sec)"
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2 |& grep "Gets/sec" | awk '{sum+=$1;num++} END{print sum/num;}'
echo "shutdown" | src/redis-cli  &> /dev/null
rm /mnt/dbpmemfs/redis.pm

else
echo "Unknow checker"
fi
