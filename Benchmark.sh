#!/bin/sh

number=100
#man date
#%s seconds since 1970-01-01 00:00:00 UTC
#%N nanoseconds (000000000..999999999)
#echo "Time for creating 100 files:"
#START_TIME=$SECONDS
#START_TIME=$(date +%s)
for i in $(seq 1 $number); 
do
	touch "File$(printf "%d" "$i").txt"
done

#END_TIME=$SECONDS
#END_TIME=$(date +%s)

#echo $START_TIME
#echo $END_TIME

#ELAPSED_TIME=$(( END_TIME - START_TIME))
#echo $ELAPSED_TIME
#echo "$(($ELAPSED_TIME/60)) min $(($ELAPSED_TIME%60)) sec"

#echo "Time for writing to 100 files:"
#START_TIME=$(date +%s)
for j in $(seq 1 $number); 
do
	echo hello > "File$(printf "%d" "$j").txt" 
done
#END_TIME=$(date +%s)

#echo $START_TIME
#echo $END_TIME

#ELAPSED_TIME=$(( END_TIME - START_TIME))
#echo $ELAPSED_TIME
for l in $(seq 1 $number); 
do
	value= cat "File$(printf "%d" "$l").txt"

	for k in $value; do
		echo $value
	done
done




