#syntax=docker/dockerfile:1
FROM ubuntu:20.04

# copy sourcefile to image
WORKDIR /root
COPY . .

RUN chmod +x ./dockerbuild.sh && ./dockerbuild.sh
