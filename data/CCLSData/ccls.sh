#!/bin/bash

g++ genRandomCCLSInstance.cpp -o CCLS.out -std=c++11

for m in $(seq 5 5 20)
do
   for n in $(seq 1000 1000 3000)
   do
      for k in $(seq 1 1 5)
      do
         ./CCLS.out $m $n $k 
      done
   done
done
