#!/usr/bin/env bash
if [ -z "${PYTHON_3}" ]; then
    PYTHON_3="python3"
fi

if [ -z "${FILE_NAME}" ]; then
    FILE_NAME="fq_test";
fi

if [ $2 ];
	then show_param="--show $2"
else
	show_param=""
fi

echo Test: Image 
echo Params: $2 
	
rm -f ${FILE_NAME}.png
$1 ${FILE_NAME}.figmaqml --snap ${FILE_NAME}.png ${show_param}

if [ $? -ne 0 ]; then 
	echo Error: code $? 
	exit -90
fi

if [ ! -f ${FILE_NAME}.png ]; then
    echo Error: ${FILE_NAME}.png not found. 
	exit -97
fi

rm -f ${FILE_NAME}_frame.png
$1 ${FILE_NAME}_frame.figmaqml --render-frame --snap  ${FILE_NAME}_frame.png ${show_param}

if [ $? -ne 0 ]; then 
	echo Error: code $? 
	exit -91
fi

if [ ! -f  ${FILE_NAME}_frame.png ]; then
    echo Error: ${FILE_NAME}_frame.png not found. 
	exit -98
fi 

compare_ratio=$(${IMAGE_COMPARE} ${FILE_NAME}.png ${FILE_NAME}_frame.png)
ratio_ok=$(${PYTHON_3} -c "print('True') if float($compare_ratio) > float(${IMAGE_TRESHOLD}) else print('False')")

echo Ratio ok: $ratio_ok 
echo Ratio value: $compare_ratio 
if [ "${ratio_ok}" != "True" ]; then
	if [ "${ratio_ok}" != "False" ]; then
		py_error=$(${PYTHON_3} -c "print('True') if float($compare_ratio) > float(${IMAGE_TRESHOLD}) else print('False')" 2>&1)
		echo Problem with script: "${py_error}", in Windows, please try 'export PYTHON_3="python"' before running the script.
	fi
	echo Result: fail 
	exit -99
fi
echo Result: ok 

