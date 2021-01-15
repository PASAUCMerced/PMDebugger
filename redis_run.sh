#!/bin/bash
export LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH
if [[ $1 == 'pmdebugger' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=pmdebugger ./src/redis-server redis.conf &
sleep 50
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2
echo "shutdown" | src/redis-cli

elif [[ $1 == 'pmemcheck' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=pmemcheck ./src/redis-server redis.conf &
#./src/redis-server redis.conf &
sleep 50
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2
echo "shutdown" | src/redis-cli
rm /mnt/dbpmemfs/redis.pm

elif [[ $1 == 'Nulgrind' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=none ./src/redis-server redis.conf &
#./src/redis-server redis.conf &
sleep 50
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2
echo "shutdown" | src/redis-cli
rm /mnt/dbpmemfs/redis.pm

elif [[ $1 == 'original' ]]
then
rm /mnt/dbpmemfs/redis.pm
./src/redis-server redis.conf &
#./src/redis-server redis.conf &
sleep 50
timeout 20s stdbuf -o0 src/redis-cli --lru-test $2
echo "shutdown" | src/redis-cli
rm /mnt/dbpmemfs/redis.pm

else
echo "Unknow checker"
fi
