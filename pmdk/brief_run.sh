#/bin/bash
valgrind=$1
insertNum=$2
runtype=$3

#trace=$4
#rm /mnt/dbpmemfs/testfile
rm /dev/shm/testfile
if [[ $runtype == 'synth_strand' ]]
then
        runtype='strand'
fi

export LD_LIBRARY_PATH=/usr/local/lib64/:$LD_LIBRARY_PATH
#export PMEM_TRACE_ENABLE=$trace     # y or n
export PMEM_NO_MOVNT=1
export PMEM_MMAP_HINT=0x0000100000000000
export MALLOC_MMAP_THRESHOLD_=0
#export PMEM_IS_PMEM_FORCE=1
LD_LIBRARY_PATH=./src/nondebug:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=./src/nondebug:$LD_LIBRARY_PATH
#dir=/mnt/dbpmemfs/testfile
dir=/dev/shm/testfile
if [[ $valgrind == 'pmemcheck' ]]
then
	bin="valgrind --tool=pmemcheck ./src/benchmarks/pmembench map_insert"
elif [[ $valgrind == 'pmdebugger' ]]
then
    if [[ $runtype == 'hashmap_tx' ]]
    then
    bin="valgrind --tool=pmdebugger --tree-reorganization=yes  ./src/benchmarks/pmembench map_insert"
    else
    bin="valgrind --tool=pmdebugger  ./src/benchmarks/pmembench map_insert"
    #	bin="valgrind --tool=pmdebugger --epoch-durabiliy-fence=yes ./src/benchmarks/pmembench map_insert"
    fi
elif [[ $valgrind == 'Nulgrind' ]]
then
	bin="valgrind --tool=none ./src/benchmarks/pmembench map_insert"
elif [[ $valgrind == 'original' ]]
then
	bin="./src/benchmarks/pmembench map_insert"
else
	echo "error valgrind variable"
fi
 echo $bin
	$bin -f $dir -d 128 -s 2385006593 -n $insertNum -t 1 -r 1 -T $runtype |& grep "false" | awk -F ';' '{print "execution time:"}{print $1}'
#gdb --args ./src/benchmarks/pmembench map_insert -f $dir -d 128 -n 102400 -t 4 -r 1 -T ctree   
#gdb --args ./src/benchmarks/pmembench map_insert -f $dir -d 128 -n 1024 -t 1 -r 1 -T ctree  
