#!/usr/bin/env python
'''
    File name: xml-remove.py
    Author: Edoardo Spadoni
    Mail: edoardo.spadoni@nethesis.it
    Date created: 02/11/2015
    Date last modified: 02/11/2015
    Version: 1.0
'''
import xml.etree.ElementTree as ET
tree = ET.parse('tree.xml')
root = tree.getroot()

for child in root:
    if(child.attrib['name'] == 'dev' or child.attrib['name'] == 'selinux'):
        root.remove(child)

for child in root:
    if(child.attrib['name'] == 'sys' or child.attrib['name'] == 'proc'):
        root.remove(child)

tree.write('tree.xml')
