#!/bin/bash
source /opt/intel/inteloneapi/setvars.sh
cd /home/u172990/prjs/IntelOneApiPOC
if [ $# -ne 1 ]
then
    rm -rf build
    build="$PWD/build"
    [ ! -d "$build" ] && mkdir -p "$build"
    cd build &&
	cmake .. &&
	cmake --build . &&
	make run
else
    cd build && make run
fi


