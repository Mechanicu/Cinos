#!/bin/bash

# gcc -o test *.c -I../include 
rm -rf *.o dsdbs dsdbc test
gcc -o dsdbs dsdbs.c ./clib/* -I../include 
gcc -o dsdbc dsdbc.c ./clib/* -I../include