## Fast, Flexible and Comprehensive Bug Detection for Persistent Memory Programs

Bang Di, Jiawen Liu, Hao Chen, Dong Li <br/>
The International Conference on Architectural Support for Programming Languages and Operating Systems (ASPLOS), 2021

## Table of Contents
* [Introduction to PMDebugger](#introduction-to-pmdebugger)
* [System configurations](#system-configurations)
  * [Hardware Dependencies](#hardware-dependencies)
  * [Software Dependencies](#software-dependencies)
* [Installation](#installation)
  * [Build PMDebugger](#build-pmdebugger)
  * [Build PMDK](#build-pmdk)
  * [Build Redis](#build-redis)
  * [Build Memcached](#build-memcached)
* [Evaluation](#evaluation)
  * [Performance](#performance)
  * [Bug Detection Capability](#bug-detection-capability)
  * [New Bugs Found By PMDebugger](#new-bugs-found-by-pmdebugger)
* [Experiment Customization](#experiment-customization)

## Introduction to PMDebugger
Debugging PM programs faces a fundamental tradeoff between performance overhead and bug coverage (comprehensiveness). Large performance overhead or limited bug coverage makes debugging infeasible or ineffective for PM programs. In this paper, we propose PMDebugger, a debugger to detect crash consistency bugs. Unlike prior work, PMDebugger is fast, flexible and comprehensive for bug detection. The design of PMDebugger is driven by the characterization of how three fundamental operations in PM programs (store, cache writeback and fence) typically happen in PM programs. PMDebugger uses a hierarchical design composed of PM debugging-specific data structures, operations and bug-detection algorithms (rules). We generalize nine rules to detect crash-consistency bugs for various PM persistency models. Compared with a state-of-the-art detector ([XFDetector](https://github.com/sihangliu/xfdetector)) and an industry-quality detector ( [Pmemcheck](https://github.com/pmem/valgrind)), PMDebugger leads to 49.3x and 3.4x speedup on average. Compared with another state-of-the-art detector ([PMTest](https://github.com/sihangliu/pmtest)) optimized for high performance, PMDebugger achieves comparable performance, without heavily relying on the programmer’s annotation but detect 38 more bugs than PMTest on ten applications. PMDebugger also identifies more bugs than XFDetector, Pmemcheck and PMTest. PMDebugger detects 19 new bugs in a real application (memcached) and two new bugs from Intel PMDK.

## System configurations

### Hardware Dependencies
PMdebugger debugs workloads that directly manage the persistent data on PM through a DAX file system. To debug these workloads, the hardware requires to provide a persistent memory device, either by using real PM (Intel Optane DC Persistent Memory) or emulating PM with DRAM. 

#### Real PM system 
* CPU: Intel 2nd Generation Xeon Scalable Processor (Gold or Platinum).
* Persistent Memory: Intel Optane DC Persistent Memory (at least 1x 128GB DIMM) and DDR4 RDIMM.

Please refer to Intel's [guide](https://software.intel.com/en-us/articles/quick-start-guide-configure-intel-optane-dc-persistent-memory-on-linux) to initialize DC persistent memory in App Direct mode. In the rest of this documentation, we assume the PM device is mounted on `/mnt/dbpmemfs`.

#### Emulated PM system
* CPU: Intel 1st/2nd Generation Xeon Scalable Processor.
* Memory: at least DDR4.

You can simply use `/dev/shm` as a persistent memory file system to debug workloads.

### Software Dependencies
The following is a list of software dependencies for PMDebugger and workloads  (the listed versions have been tested, other versions might work but not guaranteed):   
* OS: Ubuntu 18.04, Linux kernel 5.0.   
* Compiler: g++/gcc-9.2.
* Tool: Valgrind-3.15.
* Dependent libraries: libevent, libseccomp, autoconf, pkg-config, libndctl-devel (v63 or later), libdaxctl-devel (v63 or later).

## Installation
This repository is organized as the following structure:
* `valgrind-pmdebugger/`: The source code of our tool.
* `pmdk/`: Intel's [PMDK](https://pmem.io/) library, including its example PM programs.
* `memcached-pmem-master`: A Memcached implementation (from [Lenovo](https://github.com/lenovo/memcached-pmem)). 
* `memslap/`: A tool to run Memcached.

You can also simply run scripts to build them: `build_pmdebugger.sh`, `build_pmdk.sh`, `build_redis.sh` and `build_memcached.sh`. **Note that if you are not sure that any dependencies are ready, please build it step by step**. The followings are the detailed instructions to build PMDebugger and workloads separately. 

### Build PMDebugger
```
$ cd valgrind-pmdebugger
$ ./autogen.sh
$ ./configure
$ make
$ make check
$ sudo make install 
```

### Build PMDK 
```
$ cd pmdk/
$ make
$ sudo make install (if you require setting environment variables to build, run sudo -E make install)
```

### Build Redis
Redis depends on an old version PMDK, so you firstly need to build PMDK for Redis.
```
$ git clone https://github.com/pmem/redis.git -o redis
$ cd redis/
$ git checkout 3.2-nvml

$ cd deps
$ git clone https://github.com/pmem/pmdk.git
$ cd pmdk
$ git checkout tags/1.5.2
$ make
```
Then, build Redis.
```
$ cd ../../ (back to redis fold)
$ make USE_PMDK=yes STD=-std=gnu99
$ cp ../redis_run.sh  run.sh
$ cp ../redis.conf ./
```

### Build Memcached
```
$ cd memcached-pmem-master
$ CFLAGS=-Wno-error ./configure --enable-pslab
$ make 
```

## Evaluation
After building the PMDebugger suite, we can start testing performance and reproducing bugs.

### Performance
Tests for the following programs are available in PMDebugger:
* PMDK program examples:
	* btree
	* ctree
	* rbtree
	* hashmap_tx
	* hashmap_atomic
	* synth_strand (synthetic benchmark for the strand persistency model)
* Redis
* Memcached

We choose `Pmemcheck` for comparison, because it is an industry-quality detector based on Valgrind for instrumentation, while PMDebugger uses Valgrind too. We provide scripts for running programs.  The detailed steps are as follows.

#### PMDK Examples
Use script `pmdk/run.sh` to run all those benchmarks. The usage is shown as follows. 
```
Usage: ./run.sh <CHECKER> <INPUTSIZE> <WORKLOAD>
       CHECKER:   Debugger tool name (pmdebugger, pmemcheck, Nulgrind, and original). "original" represents original program with detector disabled.
       INPUTSIZE: The number of data insertions.
       WORKLOAD:  The workload to be tested.
```
For example,  we insert `1024` elements in `btree` to evaluate the performance of `PMDebugger`,  so we can run the following command:
```
$ ./run.sh pmdebugger 1024 btree
```
**Output:** PMDebugger reports all detected bugs (if any) after a test is complete. PMDK benchmarks report the execution time (i.e., `total-avg`).

#### Redis
Use script `redis/run.sh` to run the Redis example. The usage is shown as follows.
```
Usage: ./run.sh <CHECKER> <INPUTSIZE>
       CHECKER:    Debugger tool name (pmdebugger, pmemcheck, Nulgrind, and original). "original" represents original program with detector disabled.
       INPUTSIZE: The number of LRU tests.
```
For example, we use `100000` LRU tests in Redis to evaluate the performance of `PMDebugger`,  so we can run the following command:
```
$ ./run.sh pmdebugger 100000
```
**Output:** PMDebugger reports all detected bugs (if any) after a test is complete. Redis benchmarks report the throughput (i.e., `Gets/sec`).

#### Memcached
We use memslap in [WHISPER](https://github.com/swapnilh/whisper) to run Memcached. You can use the script `memslap/run.sh` to run Memcached examples. The usage is shown as follows.
```
Usage: ./run.sh <CHECKER> <INPUTSIZE>
       CHECKER:    Debugger tool name (pmdebugger, pmemcheck, Nulgrind, and original). "original" represents original program with detector disabled.
       INPUTSIZE: The number of operations to execute.
```
For example, we execute `10000` operations in Memcached to evaluate the performance of `PMDebugger`,  so we can run the following command:
```
$ ./run.sh pmdebugger 10000
```
**Output:** PMDebugger reports all detected bugs (if any) after a test is complete. Memcached benchmarks report the execution time (i.e., `Run time:`).

### Bug Detection Capability
We implement `9` rules in PMDebugger to detect bugs. To verify its capability of bug detection,  we integrate those bug cases into Valgrind. Those bug cases are in `valgrind-pmdebugger/pmdebugger/tests` folder and classified by their bug type.

They are organized as the following structure:
* `address_specific/` and `logging_related/`: Function test.
* `no_durability_guarantee/`: A persistent memory location, since the last write to it, is not persisted.
* `multiple_overwrite/`: The program writes to the same persistent memory location multiple times, before the durability of the memory location is guaranteed. 
* `no_order_guarantee/`: The program cannot guarantee the order in which writes become persistent. 
* `redundant_flush/`:  A store to a memory location is flushed multiple times before the nearest fence. 
* `flush_nothing/`: A CLF instruction does not persist any prior store.
* `epoch_redundant_fence/`: More than one fence can exist in an epoch section.
* `lack_ordering_in_strand/`: Persisting memory locations across strands can violate the order guarantee.
* `epoch_durability/`: At the end of an epoch, the durability of all memory locations updated by store instructions in the epoch cannot be guaranteed.
* `redundant_logging/`: In a logging-based transaction in PMDK, a data object is updated once but logged multiple times.

We can simply run the following command to verify bug detection capability:
```
$ cd <PMDebugger Root>/valgrind-pmdebugger
$ perl tests/vg_regtest pmdebugger
```

### New Bugs Found By PMDebugger
PMDebugger finds `19` new bugs in Memcached and `two` new bugs (confirmed by PMDK[[1]](https://github.com/pmem/pmdk/pull/4939/commits/e394307ef2baea1de31fa054a1e2c3dff3581a59)[[2]](https://github.com/pmem/pmdk/issues/4927)) in PMDK. These bugs were not reported before. We provide scripts to reproduce these bugs.

* `Bug 1, No durability guarantee in Memcached`: Use the script `memslap/new_bug1.sh`  to reproduce bugs.
* `Bug 2, Redundant epoch fence in PMDK`: Use the script `pmdk/new_bug2.sh` to reproduce the bug.
* `Bug 3, Lack durability in epoch`: Use the script `pmdk/new_bug3.sh` to reproduce the bug.

## Experiment Customization
For workloads based on PMDK, you can directly debug their binary file with PMDebugger. For example:
```
$ valgrind --tool=pmdebugger ./WORKLOAD
```
For more details, please run `valgrind --tool=pmdebugger -h`.

For other workloads, you firstly require to insert annotations, such as `epoch_begin (VALGRIND_PMC_EPOCH_BEGIN)` and `epoch_end (VALGRIND_PMC_EPOCH_END)`. Then, after recompiling workloads, you can debug them in the above way. Note that these annotations can be inserted into low_level API and achieve automatic annotation. For example, we insert `epoch_begin` and `epoch_end` in PMDK's `TX_BEGIN` and `TX_END` to achieve automatic annotation.

## Note
If you have any question when building our artifact, please contact Bang Di (<dibang@hnu.edu.cn>).  if you want a real PM system to evaluate our artifact, please contact Jiawen Liu (Jiawen Liu, <jliu265@ucmerced.edu>). We have prepared a system with Intel Optane DC persistent memory module for evaluation.

