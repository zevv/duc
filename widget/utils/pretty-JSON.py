#!/usr/bin/env python
'''
    File name: pretty-JSON.py
    Author: Edoardo Spadoni
    Mail: edoardo.spadoni@nethesis.it
    Date created: 02/11/2015
    Date last modified: 02/11/2015
    Version: 1.0
'''
import json

with open('data.json', 'r') as handle:
    parsed = json.load(handle)

print json.dumps(parsed, indent = 4, sort_keys = True)
