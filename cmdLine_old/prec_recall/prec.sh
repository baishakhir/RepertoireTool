#!/bin/sh
 
DIR=$1
for filename in $DIR/*
do
	echo $filename
	./prec_recall.py gt.csv $filename
done;
