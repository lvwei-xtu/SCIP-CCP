#!/bin/bash

g++ genRandomCCMPPInstance.cpp -o CCMPP.out -std=c++11

# for m in $(seq 10 10 30)
for m in $(seq 50 50 150)
do
   for n in $(seq 1000 1000 3000)
   do
      for k in $(seq 1 1 5)
      do
         ./CCMPP.out $m $n $k 
      done
   done
done
