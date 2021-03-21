#!/bin/sh
echo "run $0"
pkill -9 asr_comms
sleep 3
while [ 1 ];
do
	echo "start asr_comms"
	date
	asr_comms
	echo "asr_comms exit $?, restart asr_comms"
	sleep 3
done
