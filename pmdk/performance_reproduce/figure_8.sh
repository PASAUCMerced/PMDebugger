#!/bin/bash

cd ../
valgrind=$1
insertNum=$2
runtype=$3

a=$(./run.sh $valgrind $insertNum $runtype |& grep "false" | awk -F ';' '{print $1}')
b=$(./run.sh original $insertNum $runtype  | grep "false" | awk -F ';' '{print $1}')
echo "=======================Slowdown of $valgrind (x)=============================="
echo "$valgrind: $a ; original: $b"
echo "scale=2;$a/$b"|bc
