#!/bin/bash
instrument_flag=$1

if [[ $instrument_flag == 'with_instr' ]]
then
a1=$(./figure_8.sh pmdebugger 10000 |tail -1)
a2=$(./figure_8.sh pmdebugger 40000 |tail -1)
a3=$(./figure_8.sh pmdebugger 70000 |tail -1)
a4=$(./figure_8.sh pmdebugger 100000 |tail -1)
#echo "execution time of PMdebugger with 10000, 40000, 70000, 100000 input: $a1, $a2, $a3, $a4"
b1=$(./figure_8.sh pmemcheck 10000 |tail -1)
b2=$(./figure_8.sh pmemcheck 40000 |tail -1)
b3=$(./figure_8.sh pmemcheck 70000 |tail -1)
b4=$(./figure_8.sh pmemcheck 100000 |tail -1)
#echo "execution time of Pmemcheck with 10000, 40000, 70000, 100000 input: $b1, $b2, $b3, $b4"
echo "Average performance improvement of PMDebugger over Pmemcheck with instrument time (x)"
echo "scale=2;(($b1/$a1)+($b2/$a2)+($b3/$a3)+($b4/$a4))/4"|bc

elif [[ $instrument_flag == 'without_instr' ]]
then
a1=$(./figure_8.sh pmdebugger 10000 |tail -1)
a2=$(./figure_8.sh pmdebugger 40000 |tail -1)
a3=$(./figure_8.sh pmdebugger 70000 |tail -1)
a4=$(./figure_8.sh pmdebugger 100000 |tail -1)
#echo "execution time of PMdebugger with 10000, 40000, 70000, 100000 input: $a1, $a2, $a3, $a4"
b1=$(./figure_8.sh pmemcheck 10000 |tail -1)
b2=$(./figure_8.sh pmemcheck 40000 |tail -1)
b3=$(./figure_8.sh pmemcheck 70000 |tail -1)
b4=$(./figure_8.sh pmemcheck 100000 |tail -1)
#echo "execution time of Pmemcheck with 10000, 40000, 70000, 100000 input: $b1, $b2, $b3, $b4"

c1=$(./figure_8.sh Nulgrind 10000 |tail -1)
c2=$(./figure_8.sh Nulgrind 40000 |tail -1)
c3=$(./figure_8.sh Nulgrind 70000 |tail -1)
c4=$(./figure_8.sh Nulgrind 100000 |tail -1)
#echo "execution time of Nulgrind with 10000, 40000, 70000, 100000 input: $c1, $c2, $c3, $c4"


echo "Average performance improvement of PMDebugger over Pmemcheck without instrument time (x)"
echo "scale=2;(($b1-$c1)/($a1-$c1)+($b2-$c2)/($a2-$c2)+($b3-$c3)/($a3-$c3)+($b4-$c4)/($a4-$c4))/4"|bc

else
echo "error variable"
fi


