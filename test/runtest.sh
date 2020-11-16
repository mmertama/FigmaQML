#!/usr/bin/env bash
if [ ! $(which realpath) ]; then
    echo OSX? realpath not found - try brew install coreutils
    exit -1000
fi

LOCAL_DIR="$(dirname "$(realpath "$0")")"
${LOCAL_DIR}/runtest_export.sh $1 $2 $3
exit_code=$?
if [ $exit_code -ne 0 ]; then
	exit $exit_code 
fi
${LOCAL_DIR}/runtest_image.sh $1 $4
exit_code=$?
if [ $exit_code -ne 0 ]; then
	exit $exit_code
fi
