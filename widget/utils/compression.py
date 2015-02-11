#!/usr/bin/env python
'''
    File name: compression.py
    Author: Edoardo Spadoni
    Mail: edoardo.spadoni@nethesis.it
    Date created: 02/11/2015
    Date last modified: 02/11/2015
    Version: 1.0
'''

import json

with open('pretty.json', 'r') as handle:
    parsed = json.load(handle)

minJSON = json.dumps(parsed, indent = 0, sort_keys = True)

wf = open("pretty.json","w")
for line in minJSON:
    newline = line.rstrip('\r\n')
    wf.write(newline)
