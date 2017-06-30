#!/bin/sh
#
# Simple script to both generate simple CGI scripts for a bunch of duc databases, as well
# as acting as the main index page to browse those DBs.
#
# When called from the CLI with the -g option, it will generate <db>.cgi scripts in the $CWD. 
#
# This is totally not secure or hardened against malicious actors!  Only use internally in 
# a secured network you trust completely.  You have been warned!
# 
# Only tested with Apache, and you will need to make sure you have the +ExecCGI option setup, 
# and this file chmod +x as well.
#
# Author: John Stoffel (john@stoffel.org)
# License: GPLv2

mkcgi=0

title="Disk Usage graphs"
dbdir="/path/to/duc/dbs"
wwwdir="/var/www/html"
duc="/usr/local/bin/duc"

# Sort index by time, or by db file name.  It's just the arg to 'ls'
sortby="-t"

# If these are not empty, use these files in $wwwdir for the .cgi file's --header 
# and/or --footer options of 'duc cgi ...'.
footer=""
header=""


# Option processing
while getopts "g" opt; do
  case $opt in
    g)
      echo "Generating new cgi scripts in $CWD from DBs in $dbdir/."
      mkcgi=1
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      ;;
  esac
done


# Now do the work, 
cd $dbdir

if [ "$mkcgi" = "0" ]; then
  echo "Content-type: text/html"
  echo ""
  echo "<HTML><HEAD>"
  echo "<STYLE>"
  echo "table, th, td { border: 1px solid black; border-collapse: collapse; }"
  echo "th, td { padding: 5px; text-align: left; }"
  echo "</STYLE>"
  echo ""
  echo "<TITLE>$title</TITLE></HEAD><BODY>"
  echo ""
  echo "<TABLE style=\"width:50%\">"
  echo " <TR style=\"border:1px\">"
  echo "  <TH>Path</TH>"
  echo "  <TH>Size</TH>"
  echo "  <TH>Files</TH>"
  echo "  <TH>Dirs</TH>"
  echo "  <TH>Date</TH>"
  echo "  <TH>Time</TH>"
  echo " </TR>"
  echo ""
fi

for db in `ls $sortby *.db`
do
    base=`echo $db | awk -F. '{print $1}'`
    array=(`$duc info -d ${db} | tail -1 `)

    if [ "$mkcgi" = "0" ]; then
      echo "  <TR>"
      echo "    <TD> <A HREF=\"${base}.cgi?cmd=index&path=${array[5]}\">${array[5]}</A></TD>"
      echo "    <TD>${array[4]}</TD>"
      echo "    <TD>${array[2]}</TD>"
      echo "    <TD>${array[3]}</TD>"
      echo "    <TD>${array[0]}</TD>"
      echo "    <TD>${array[1]}</TD>"
      echo "  </TR>"
    fi


    if  [ "$mkcgi" = "1" ]; then 
      cgi="${wwwdir}/${base}.cgi"
      echo " $cgi"
      cat <<EOF > $cgi
#!/bin/sh

args="-d /data/sjscratch/is/duc/${db} --palette=rainbow --list --header=header.htm --footer=footer.htm"

/usr/local/bin/duc cgi \$args

EOF

      chmod +x $cgi
    fi
done

if  [ "$mkcgi" = "0" ]; then
  echo "</TABLE>"
  echo "</BODY>"
  echo "</HTML>"
fi

