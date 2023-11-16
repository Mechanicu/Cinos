#!/bin/bash

# change docker apt sources
cat <<EOF > /etc/apt/sources.list
deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse 
deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse 
deb-src http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse
EOF

# set env for go compiler
GOPATH="/root/go"
GOINSTALL_PATH="/usr/local"
GO_VERSION="1.21.4"
GO_SRC_FILES=" sdcs.go"
GO_SRC_FILES+=" sdcs_grpc_service.go"

# update aptlist and install necessary tools
BUILD_TOOLS=" protobuf-compiler"
BUILD_TOOLS+=" wget"
apt update && apt install -y ${BUILD_TOOLS} 
echo BUILD_TOOLS=$BUILD_TOOLS

# download and install go tools and source code
wget https://go.dev/dl/go${GO_VERSION}.linux-amd64.tar.gz
rm -rf ${GOINSTALL_PATH}/go && tar -C ${GOINSTALL_PATH} -xzf go${GO_VERSION}.linux-amd64.tar.gz
export PATH=${GOINSTALL_PATH}/go/bin:${GOPATH}/bin:$PATH
go env -w GOPROXY=https://goproxy.cn
go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.28
go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.2
echo GOINSTALL_PATH=$GOINSTALL_PATH GO_VERSION=$GO_VERSION GO_PATH=$GOPATH

# build go executable files
PROTO_FILE_PATH="sdcs_grpc/sdcs.proto"
protoc --go_out=. --go_opt=paths=source_relative --go-grpc_out=. --go-grpc_opt=paths=source_relative ${PROTO_FILE_PATH}
go build ${GO_SRC_FILES}
