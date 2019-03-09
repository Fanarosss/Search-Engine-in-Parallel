#!/bin/bash

echo "Total Keywords received by all workers"

cd log

for filename in *; do
	cat $filename | grep "search"
	break
done | wc -l
