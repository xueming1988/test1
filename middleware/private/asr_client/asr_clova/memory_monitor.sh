#!/bin/bash

PID=
RSS=
LIMIT=10000

while true
do
    sleep 60
    echo "================Iptvagent monitor start===================="
    PID=$(ps | grep iptvagent | grep -v grep | awk '{printf $1}')
    echo "PID=$PID"
    if [ "$PID" != "" ]
    then
        RSS=$(cat /proc/$PID/status | grep RSS | awk '{printf $2}')
        echo "Memory usage=${RSS}KB"
        if [ $RSS -gt $LIMIT ]
        then
            echo "Memory usage is abnormal, need to restart iptvagent!!!"
            killall -9 iptvagent
        else
            echo "Memory usage is normal."
        fi
    fi
    echo "================Iptvagent monitor end======================"
done
