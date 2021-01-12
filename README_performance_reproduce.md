## How to get results in Figure 8

### PMDK (Figure 8(a) -- Figure 8(f))
```
$ cd pmdk/performance_reproduce
$ ./figure_8.sh <CHECKER> <INPUTSIZE> <WORKLOAD>
       CHECKER:   Debugger tool name (pmdebugger, pmemcheck, Nulgrind).
       INPUTSIZE: The number of data insertions.
       WORKLOAD:  The workload to be tested.
```
For example, we want to get the result of PMDebugger with 1K insertion in Figure 8(a). So we can run the following command:
```
$ ./figure_8.sh pmdebugger 1000 btree
```

### memcached (Figure 8(g))
```
$ cd memcached/performance_reproduce
$ ./figure_8.sh <CHECKER> <INPUTSIZE>
       CHECKER:   Debugger tool name (pmdebugger, pmemcheck, Nulgrind).
       INPUTSIZE: The number of data insertions.
```
For example, we want to get the result of PMDebugger with 10K get and set operations in Figure 8(g). So we can run the following command:
```
$ ./figure_8.sh pmdebugger 10000 
```

### redis (Figure 8(h))
```
$ cd redis/performance_reproduce
$ ./figure_8.sh <CHECKER> <INPUTSIZE>
       CHECKER:   Debugger tool name (pmdebugger, pmemcheck, Nulgrind).
       INPUTSIZE: The number of data insertions.
```
For example, we want to get the result of PMDebugger with 100K keys in Figure 8(h). So we can run the following command:
```
$ ./figure_8.sh pmdebugger 100000 
```

## How to get results in Table 5
### PMDK (btree -- synth_strand)
```
$ cd pmdk/performance_reproduce
$ ./table_5.sh  <WORKLOAD> <WITH/WITHOUT INSTRUMENTATION>
       WORKLOAD:  The workload to be tested.
       <WITH/WITHOUT INSTRUMENTATION>: Including instrumentation time or not (with_instr or without_instr).
```
For example, we want to get the result of btree with instrumentation time in Table 5. So we can run the following command:
```
$ ./table_5.sh btree with_instr
```
### memcached
```
$ cd memcached/performance_reproduce
$ ./table_5.sh <WITH/WITHOUT INSTRUMENTATION>
       <WITH/WITHOUT INSTRUMENTATION>: Including instrumentation time or not (with_instr or without_instr).
```
For example, we want to get the result of memcached with instrumentation time in Table 5. So we can run the following command:
```
$ ./table_5.sh with_instr
```

### redis
```
$ cd redis/performance_reproduce
$ ./table_5.sh <WITH/WITHOUT INSTRUMENTATION>
       <WITH/WITHOUT INSTRUMENTATION>: Including instrumentation time or not (with_instr or without_instr).
```
For example, we want to get the result of redis with instrumentation time in Table 5. So we can run the following command:
```
$ ./table_5.sh with_instr
```
