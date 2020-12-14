export PMEM_NO_MOVNT=1
export PMEM_MMAP_HINT=0x0000100000000000
export MALLOC_MMAP_THRESHOLD_=0
cd src/examples/libpmemobj/map
rm /dev/shm/testfile 
valgrind --tool=pmdebugger --print-debug-detail=yes --epoch-durabiliy-fence=yes ./data_store hashmap_atomic /dev/shm/testfile 100
