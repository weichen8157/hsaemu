#!/bin/bash

times=$1
if [ -z $times ]; then
	times=100
fi

for (( i=0; i<$times; i=i+1 ))
do
	echo "*****************************************************************"
	echo $i
	./a.out
	for (( j=0; j<1000; j=j+1 ))
	do
		echo ""
	done
done
