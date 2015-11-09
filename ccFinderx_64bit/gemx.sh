#!/bin/bash
export LD_LIBRARY_PATH=./:./.libs:$LD_LIBRARY_PATH
java -classpath com.ibm.icu.jar:swt.jar:trove.jar:jars/icu4j-charsets-4_0_1.jar:jars/icu4j-localespi-4_0_1.jar -jar gemxlib.jar

