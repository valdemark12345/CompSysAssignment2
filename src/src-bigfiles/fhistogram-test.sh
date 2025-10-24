#!/bin/bash
#To test correctness of multithreaded version against original, enter process name w/o -mt at the end.
# Usage: ./test ./process [args]

ARGS=("../src" "small_directory" "empty_directory" "small_directory2")
make all > /dev/null
for dir in "${ARGS[@]}"; do

    echo "Testing  on $dir"

    if  diff <(./fhistogram "$dir" | tail -n 9 | tr -d '\r') \
             <(./fhistogram-mt "$dir" | tail -n 9 | tr -d '\r')  
             then
        echo "Test passed: same histogram"
    else
        echo "Test failed: histograms not equal"
    fi

   # Measure average execution times
runs=30
total1=0
total2=0

for ((i=1; i<=runs; i++)); do
    start1=$(date +%s%N)
    ./fauxgrep here "$dir" > /dev/null
    end1=$(date +%s%N)
    total1=$(( total1 + (end1 - start1) ))

    start2=$(date +%s%N)
    ./fauxgrep-mt here "$dir" > /dev/null
    end2=$(date +%s%N)
    total2=$(( total2 + (end2 - start2) ))
done

avg1=$(( total1 / runs / 1000000 ))  # convert ns → ms
avg2=$(( total2 / runs / 1000000 ))

echo "Single-threaded average time:   ${avg1} ms over ${runs} runs"
echo "Multithreaded average time:     ${avg2} ms over ${runs} runs"

if (( avg2 < avg1 )); then
    improvement=$(( avg1 - avg2 ))
    echo "Multithreaded version is faster by ${improvement} ms on average"
else
    slowdown=$(( avg2 - avg1 ))
    echo "Multithreaded version is slower by ${slowdown} ms on average"
fi
echo

done

make clean
