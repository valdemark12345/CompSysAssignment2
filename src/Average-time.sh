#!/bin/bash

# Usage: ./test-fauxgrep ./process [args]
# Optional: change NUM_RUNS below

NUM_RUNS=1000
TOTAL=0

for ((i=1; i<=NUM_RUNS; i++)); do
    echo "Run #$i..."

    # Capture time using time command.
    OUTPUT=$( { time "$@" >/dev/null; } 2>&1 )

    # Extract and parse the 'real' time (format like: real   0m0.007s)
    REAL=$(echo "$OUTPUT" | grep real | awk '{print $2}')
    MIN=$(echo "$REAL" | sed -E 's/m.*//')
    SEC=$(echo "$REAL" | sed -E 's/.*m(.*)s/\1/')
    RUNTIME=$(echo "$MIN * 60 + $SEC" | bc -l)

    echo "Time: ${RUNTIME}s"
    TOTAL=$(echo "$TOTAL + $RUNTIME" | bc -l)
done

AVG=$(echo "scale=3; $TOTAL / $NUM_RUNS" | bc -l)
echo "----------------------------"
echo "Average time over $NUM_RUNS runs: ${AVG}s"
