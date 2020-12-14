cd memcached-pmem-master
make clean
git reset --hard c1bbcba0101b7afc79ae9b4a4062393e692fde6a

CFLAGS=-Wno-error ./configure --enable-pslab
make -j
