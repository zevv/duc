#!/bin/sh
#
# Example (and insecure!) CGI script you can use.

/usr/local/bin/duc cgi -d /var/www/duc/duc.db --header \
/var/www/duc/duc.header --footer /var/www/duc/duc.footer --css-url \
http://localhost/duc/duc.css --list --palette=size

