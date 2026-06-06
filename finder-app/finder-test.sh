#!/bin/sh

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

SCRIPT_DIR=$(dirname "$0")
SCRIPT_DIR=$(cd "$SCRIPT_DIR" && pwd)

if [ -f "$SCRIPT_DIR/conf/username.txt" ]; then
    username=$(cat "$SCRIPT_DIR/conf/username.txt")
elif [ -f "$SCRIPT_DIR/../conf/username.txt" ]; then
    username=$(cat "$SCRIPT_DIR/../conf/username.txt")
else
    echo "Error: username.txt not found"
    exit 1
fi

if [ $# -lt 3 ]; then
    echo "Using default value $WRITESTR for string to write"
    if [ $# -lt 1 ]; then
        echo "Using default value $NUMFILES for number of files to write"
    else
        NUMFILES=$1
    fi
else
    NUMFILES=$1
    WRITESTR=$2
    WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are $NUMFILES and the number of matching lines are $NUMFILES"

echo "Writing $NUMFILES files containing string $WRITESTR to $WRITEDIR"

rm -rf "$WRITEDIR"
mkdir -p "$WRITEDIR"

i=1
while [ "$i" -le "$NUMFILES" ]
do
    "$SCRIPT_DIR/writer" "$WRITEDIR/${username}${i}.txt" "$WRITESTR"
    i=$((i + 1))
done

OUTPUTSTRING=$("$SCRIPT_DIR/finder.sh" "$WRITEDIR" "$WRITESTR")

echo "$OUTPUTSTRING"

if [ "$OUTPUTSTRING" = "$MATCHSTR" ]; then
    echo "success"
    exit 0
else
    echo "error"
    echo "Expected: $MATCHSTR"
    echo "Actual: $OUTPUTSTRING"
    exit 1
fi
