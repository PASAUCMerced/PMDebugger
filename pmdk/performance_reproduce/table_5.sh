#!/bin/bash

runtype=$1
instrument_flag=$2
if [[ $instrument_flag == 'with_instr' ]]
then
a1=$(./figure_8.sh pmdebugger 1024 $runtype|tail -1)
a2=$(./figure_8.sh pmdebugger 10240 $runtype|tail -1)
a3=$(./figure_8.sh pmdebugger 102400 $runtype|tail -1)

b1=$(./figure_8.sh pmemcheck 1024 $runtype|tail -1)
b2=$(./figure_8.sh pmemcheck 10240 $runtype|tail -1)
b3=$(./figure_8.sh pmemcheck 102400 $runtype|tail -1)
echo "Average performance improvement of PMDebugger over Pmemcheck with instrument time (x)"
echo "scale=2;(($b1/$a1)+($b2/$a2)+($b3/$a3))/3"|bc

elif [[ $instrument_flag == 'without_instr' ]]
then
a1=$(./figure_8.sh pmdebugger 1024 $runtype|tail -1)
a2=$(./figure_8.sh pmdebugger 10240 $runtype|tail -1)
a3=$(./figure_8.sh pmdebugger 102400 $runtype|tail -1)

b1=$(./figure_8.sh pmemcheck 1024 $runtype|tail -1)
b2=$(./figure_8.sh pmemcheck 10240 $runtype|tail -1)
b3=$(./figure_8.sh pmemcheck 102400 $runtype|tail -1)

c1=$(./figure_8.sh Nulgrind 1024 $runtype|tail -1)
c2=$(./figure_8.sh Nulgrind 10240 $runtype|tail -1)
c3=$(./figure_8.sh Nulgrind 102400 $runtype|tail -1)


echo "Average performance improvement of PMDebugger over Pmemcheck without instrument time (x)"
echo "scale=2;(($b1-$c1)/($a1-$c1)+($b2-$c2)/($a2-$c2)+($b3-$c3)/($a3-$c3))/3"|bc

else
echo "error variable"
fi
