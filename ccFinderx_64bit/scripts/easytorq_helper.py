#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import getopt
import sys

import libeasytorq

if __name__ == '__main__':
	options, args = getopt.getopt(sys.argv[1:], "cv")
	
	for name, value in options:
		if name == '-c':
			c = libeasytorq.ICUConverter()
			for e in c.getavailableencodings():
				print e
			sys.exit(0)
		elif name == '-v':
			print ".".join([str(i) for i in libeasytorq.version()])
		else:
			assert False
	
	if len(args) > 0:
		print >> sys.stderr, "error: too many command-line arguments"
		sys.exit(1)
	
	sys.exit(0)

