export PMEM_NO_MOVNT=1
export PMEM_MMAP_HINT=0x0000100000000000
export MALLOC_MMAP_THRESHOLD_=0
cd src/examples/libpmemobj/array
rm /dev/shm/testfile 
valgrind --tool=pmdebugger --print-debug-detail=yes --epoch-durabiliy-fence=yes ./array /dev/shm/testfile alloc test2 100 int