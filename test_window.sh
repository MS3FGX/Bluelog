#!/bin/bash

secs=420

for i in 3 4 5 6 7 8
do
	echo "Window: $i"
	./bluelog -w $i -to dump_valleri_$i.txt &
	PID=$!
	sleep $secs
	kill  $PID
done
