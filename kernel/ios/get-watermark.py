#!/usr/bin/python

import sys

data = open(sys.argv[1], 'rb').read()
offset = data.find("W$PP")
if offset != -1:
    print data[offset + 4:offset + 4 + 20].encode('hex')
