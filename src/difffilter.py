#!/usr/bin/python

import sys
if len(sys.argv) != 2:
    sys.stderr.write('FAIL FAIL FAIL need a filter type')
    exit(-1)

lineending="\n"
filefilter='.'+sys.argv[1] + lineending

oneago=''
valid=False
sentinel='===================================================================' + lineending

for currline in sys.stdin:
    if currline == sentinel and oneago.startswith('Index: '):
        # we're at the start of a new diff section
        if oneago.endswith(filefilter):
            # oneago is the index line
            valid=True
        else:
            valid=False

    if valid:
        sys.stdout.write(oneago)

    oneago=currline
