#!/bin/bash
cd ../ 
valgrind=$1
insertNum=$2

a=$(./run.sh $valgrind $insertNum  |& grep "Run time:" | awk '{sum+=$3;num++} END{print sum/num;}')
b=$(./run.sh original $insertNum  |& grep "Run time:" | awk '{sum+=$3;num++} END{print sum/num;}')
echo "=======================Slowdown of $valgrind (x)=============================="
echo "$valgrind: $a ; original: $b"
echo "scale=2;$a/$b"|bc
