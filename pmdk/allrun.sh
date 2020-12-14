#!/bin/bash
function run(){
./run.sh  $1 $2 $3 &>> executionTime.txt
echo "
====================================================================================================
" >> executionTime.txt
}
echo 0 > executionTime.txt

#run --small btree
#run --med btree
#run --large btree
for i in btree ctree rbtree hashmap_atomic hashmap_tx;
do
for j in 1024 10240 102400;
do
echo $j $i
#run original $j $i
run pmemcheck $j $i
run pmdebugger $j $i
#run Nulgrind $j $i
done
done

#for i in hashmap_tx
#do
#echo $i
#run small $i
#run med $i
#run --large $i
#done
