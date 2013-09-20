#!/bin/sh
DIR=$1
for filename in $DIR/*
do
  echo $filename
  ./conv_annotate.py $filename
done;
