#!/bin/sh

VOL=/data
ID=$(od -An -N8 -tx1 /dev/urandom | tr -d ' ')
COUNT=0

exec 9>$VOL/.lock

while :; do
    COUNT=$((COUNT + 1))

    flock 9
    NUM=1
    while [ -f $VOL/$(printf '%03d' $NUM) ]; do
        NUM=$((NUM + 1))
    done
    NAME=$VOL/$(printf '%03d' $NUM)
    echo "$ID $COUNT" > $NAME
    flock -u 9

    sleep 1
    rm $NAME
    sleep 1
done
