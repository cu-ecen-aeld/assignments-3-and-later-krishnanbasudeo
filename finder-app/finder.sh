#!/bin/sh

filesdir=$1
searchstr=$2

if [ $# -ne 2 ]; then
    echo "Error: Two arguments required: filesdir and searchstr"
    exit 1
fi

if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a directory"
    exit 1
fi

numfiles=$(find "$filesdir" -type f | wc -l)
numlines=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $numfiles and the number of matching lines are $numlines"
