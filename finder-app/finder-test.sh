#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Siddhant Jajoo

set -e
set -u

NUMFILES=10
WRITESTR=AELD_IS_FUN
WRITEDIR=/tmp/aeld-data

SCRIPT_DIR=$(dirname "$0")
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

username=$(cat "${REPO_ROOT}/conf/username.txt")

if [ $# -lt 3 ]
then
        echo "Using default value ${WRITESTR} for string to write"
        if [ $# -lt 1 ]
        then
                echo "Using default value ${NUMFILES} for number of files to write"
        else
                NUMFILES=$1
        fi
else
        NUMFILES=$1
        WRITESTR=$2
        WRITEDIR=/tmp/aeld-data/$3
fi

MATCHSTR="The number of files are ${NUMFILES} and the number of matching lines are ${NUMFILES}"

echo "Writing ${NUMFILES} files containing string ${WRITESTR} to ${WRITEDIR}"

rm -rf "${WRITEDIR}"
mkdir -p "${WRITEDIR}"

# Clean and build writer C application
make -C "${REPO_ROOT}/finder-app" clean
make -C "${REPO_ROOT}/finder-app"

i=1
while [ $i -le "$NUMFILES" ]
do
        "${REPO_ROOT}/finder-app/writer" "${WRITEDIR}/${username}${i}.txt" "${WRITESTR}"
        i=$((i + 1))
done

OUTPUTSTRING=$("${REPO_ROOT}/finder-app/finder.sh" "${WRITEDIR}" "${WRITESTR}")

echo "${OUTPUTSTRING}"

if [ "${OUTPUTSTRING}" = "${MATCHSTR}" ]
then
        echo "success"
        exit 0
else
        echo "error"
        echo "Expected: ${MATCHSTR}"
        echo "Actual: ${OUTPUTSTRING}"
        exit 1
fi
