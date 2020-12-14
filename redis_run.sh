
if [[ $1 == 'pmdebugger' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=pmdebugger ./src/redis-server redis.conf &
sleep 45
timeout 20s src/redis-cli --lru-test $2
echo "shutdown" | src/redis-cli

elif [[ $1 == 'pmemcheck' ]]
then
rm /mnt/dbpmemfs/redis.pm
valgrind --tool=pmemcheck ./src/redis-server redis.conf &
sleep 45
timeout 20s src/redis-cli --lru-test $2
echo "shutdown" | src/redis-cli
rm /mnt/dbpmemfs/redis.pm

else
echo "Unknow checker"
fi
