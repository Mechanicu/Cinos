#!/bin/bash
go env -w GOPROXY=https://goproxy.cn
go build *.go
kill -9  $(pidof sdcs)
if [ "$1" == "test" ];then
    NODE_COUNT=$2
    echo ${NODE_COUNT}
    for ((i=0;i<${NODE_COUNT};i++));  
    do   
        ./sdcs $i & 
    done   
fi
