#!/bin/sh
#
# Use `test/fakesmsc' to test the bearerbox and the smsbox.

set -e

times=10
interval=0.001
loglevel=0

test/fakesmsc -i $interval -m $times '123 234 www www.kannel.org' \
    > check_fakesmsc.log 2>&1 &

sleep 2

gw/bearerbox -v $loglevel gw/smskannel.conf > check_fakesmsc_bb.log 2>&1 &
bbpid=$!

sleep 2

gw/smsbox -v $loglevel gw/smskannel.conf > check_fakesmsc_sms.log 2>&1 &

running=yes
while [ $running = yes ]
do
    sleep 1
    if grep "got message $times" check_fakesmsc.log >/dev/null
    then
    	running=no
    fi
done

kill -INT $bbpid
wait

if grep 'WARNING:|ERROR:|PANIC:' check_fakesmsc*.log >/dev/null
then
	echo check_fakesmsc.sh failed 1>&2
	echo See check_fakesmsc*.log for info 1>&2
	exit 1
fi

rm check_fakesmsc*.log
