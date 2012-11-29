#!/bin/sh
pid=`cat /tmp/bluelog.pid`
if ! kill -0 $pid 
then
	sudo /etc/init.d/bluelog start
fi
