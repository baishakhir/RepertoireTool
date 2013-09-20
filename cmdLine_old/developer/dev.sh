#!/bin/sh
DIR=$1
for filename in $DIR/*
do
  echo $filename
  ./dev.py $filename
done;
