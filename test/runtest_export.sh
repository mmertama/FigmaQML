#!/usr/bin/env bash


if [ -z "${FILE_NAME}" ];
	then FILE_NAME="fq_test";
fi

echo Test: Export 
echo Params: $2 $3 

echo Phase 1: Get frame from the server.

rm -f ${FILE_NAME}_frame.figmaqml
$1 --render-frame --store $2 $3 ${FILE_NAME}_frame.figmaqml

if [ $? -ne 0 ]; then 
	echo Error: code $? 
	exit -80
fi

if [ ! -f ${FILE_NAME}_frame.figmaqml ]; then
	echo Error: ${FILE_NAME}_frame.figmaqml not found. 
	exit -86
fi


echo Phase 2: Generate QML directory from the server.

rm -rf ${FILE_NAME}_qml
$1 $2 $3 ${FILE_NAME}_qml

pid=$?
if [ $pid -ne 0 ]; then
    echo "Error: code $pid ($1 $2 $3 ${FILE_NAME}_qml)"
    exit -81
fi

if [ ! -d ${FILE_NAME}_qml ]; then
	echo Error: ${FILE_NAME}_qml not found. 
	exit -87
fi

echo Phase 3: Store .figmaqml file from the server.

rm -f ${FILE_NAME}
$1 $2 $3 --store ${FILE_NAME}

if [ $? -ne 0 ]; then 
	echo Error: code $? 
	exit -82
fi

if [ ! -f ${FILE_NAME}.figmaqml ]; then
	echo Error: ${FILE_NAME}.figmaqml not found. 
	exit -88
fi

echo Phase 4: Restore .figmaqml file and generate a QML directory.

rm -rf ${FILE_NAME}_qml_2
$1 ${FILE_NAME}.figmaqml ${FILE_NAME}_qml_2

if [ $? -ne 0 ]; then 
	echo Error: code $? 
	exit -83
fi

echo Phase 5: Compare contents.

if [ ! -d ${FILE_NAME}_qml_2 ]; then
	echo Error: ${FILE_NAME}_qml_2 not found. 
	exit -89
fi

ls -lah  --time-style='+'  ${FILE_NAME}_qml > ${FILE_NAME}.txt
ls -lah  --time-style='+'  ${FILE_NAME}_qml_2 > ${FILE_NAME}.txt
TEST = $(diff ${FILE_NAME}.txt ${FILE_NAME}.txt)

if [[ $TEST ]]; then
	echo Error: "$TEST" 
	echo Result: fail
    return -180
else
	echo Result: ok 
fi

