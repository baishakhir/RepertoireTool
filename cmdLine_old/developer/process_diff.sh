#!/bin/sh
DIR=$1
for filename in $DIR/*
do
  echo $filename
  ./process_diff_dev.py $filename
done;
