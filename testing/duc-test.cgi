#!/bin/sh
#
# Testing of CGI without needing apache, nginx, etc.  Just python3 and builtin http lib.
#

testdir=/var/tmp/duc-cgi-test

${testdir}/duc cgi -d ${testdir}/duc.db --list 

