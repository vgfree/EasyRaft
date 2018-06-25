#!/usr/bin/env bash


mkdir data_0 data_1 data_2

T1=$(date +%s%N)

for i in $(seq 0 2)
do
	./analyse -i $i -c "0.0.0.0:6000,0.0.0.0:6001,0.0.0.0:6002" &
done

wait


T2=$(date +%s%N)

elapsed=$((T2 - T1))
bc -l <<< "$elapsed / 1000000000"
