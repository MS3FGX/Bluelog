#!/bin/bash

secs=300

for i in 3 4 5 6 7 8:
do
	echo "Window: $i"
	./bluelog -w $i -o dump_$USER_$i.txt &
	PID=$!
	sleep $secs
	kill  $PID
done
