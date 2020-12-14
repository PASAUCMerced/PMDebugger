git clone https://github.com/pmem/redis.git -o redis
cd redis/
git checkout 3.2-nvml
 echo "***************** deps  *********************"
cd deps
git clone https://github.com/pmem/pmdk.git
cd pmdk
git checkout tags/1.5.2
make -j


cd ../../
echo "***************** Make redis *********************"

make USE_PMDK=yes STD=-std=gnu99 -j
cp ../redis_run.sh  run.sh
cp ../redis.conf ./
