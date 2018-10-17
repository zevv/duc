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
#
# v20180816 - simple java script sorting of columns added.

bytesToHuman() {
    b=${1:-0}; d=''; s=0; S=(Bytes {K,M,G,T,P,E,Z,Y}iB)
    while ((b > 1024)); do
        d="$(printf ".%02d" $((b % 1024 * 100 / 1024)))"
        b=$((b / 1024))
        let s++
    done
    echo -n "$b$d ${S[$s]}"
}

numToHuman() {
    b=${1:-0}; d=''; s=0; S=(Bytes {K,M,G,T,P,E,Z,Y})
    while ((b > 1024)); do
        d="$(printf ".%02d" $((b % 1024 * 100 / 1024)))"
        b=$((b / 1024))
        let s++
    done
    echo -n "$b$d ${S[$s]}"
}


mkcgi=0

# Make your changes here to reflect your site's needs
title="Disk Usage graphs"
dbdir="/path/to/data/duc"
wwwdir="/var/www/html"
duc="/usr/local/bin/duc"

# Sort index by time, or by db file name.  It's just the arg to 'ls'
sortby="-t"

# If these are not empty, use these files in $wwwdir for the .cgi file's --header 
# and/or --footer options of 'duc cgi ...'.
footer=""
header=""


#-------------------------------------------------------------------------
# No changes needed past here hopefully
#-------------------------------------------------------------------------

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
  echo "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\""
  echo "  \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">"
  echo "<HTML>"
  echo " <HEAD>"
  echo "  <STYLE>"
  echo "    table, th, td { border: 1px solid black; border-collapse: collapse; }"
  echo "    th, td { padding: 5px; text-align: left; }"
  echo "  </STYLE>"
  echo "  <SCRIPT type=\"text/javascript\" src=\"jquery-latest.js\"></SCRIPT>"
  echo "  <SCRIPT type=\"text/javascript\" src=\"jquery.tablesorter.js\"></SCRIPT>"
  echo "  <SCRIPT type=\"text/javascript\" src=\"numeral.js\"></SCRIPT>"
  echo "  <SCRIPT type=\"text/javascript\" src=\"mytable.js\"></SCRIPT>"
  echo ""
  echo "  <TITLE>$title</TITLE>"
  echo "  <LINK rel=\"stylesheet\" href=\"style.css\" type=\"text/css\" id=\"\" media=\"print, projection, screen\" />"
  echo " </HEAD>"
  echo " <BODY>"
  echo ""
  echo " <TABLE style=\"width:50%\" id=\"ducdata\" class=\"tablesorter\">"
  echo "  <THEAD>"
  echo "   <TR style=\"border:1px\">"
  echo "    <TH>Path</TH>"
  echo "    <TH>Size</TH>"
  echo "    <TH>Files</TH>"
  echo "    <TH>Dirs</TH>"
  echo "    <TH>Date</TH>"
  echo "    <TH>Time</TH>"
  echo "   </TR>"
  echo "  </THEAD>"
  echo "  <TBODY>"
  echo ""
fi

for db in `ls $sortby *.db`
do
    base=`echo $db | awk -F. '{print $1}'`
    count=(`$duc info -d ${db} | wc -l`)
    # Skip if the DB isn't valid for some reason.
    if [ "$count" = "1" ]; then
	continue
    fi

    # if there is no .cgi script, skip 
    #if [ ! -x "${base}.cgi" ]; then
    #	continue
    #fi

    array=(`$duc info -b -d ${db} | tail -1 `)
    size=$(bytesToHuman ${array[4]} )
    filecnt=$(numToHuman ${array[2]} )
    dircnt=$(numToHuman ${array[3]} )

    if [ "$mkcgi" = "0" ]; then
      echo "  <TR>"
      echo "    <TD> <A HREF=\"${base}.cgi?cmd=index&path=${array[5]}\">${array[5]}</A></TD>"
      echo "    <TD data-sort-value=\"${array[4]}\">$size</TD>"
      echo "    <TD data-sort-value=\"${array[2]}\">$filecnt</TD>"
      echo "    <TD data-sort-value=\"${array[3]}\">$dircnt</TD>"
      echo "    <TD>${array[0]}</TD>"
      echo "    <TD>${array[1]}</TD>"
      echo "  </TR>"
    fi


    if  [ "$mkcgi" = "1" ]; then 
      cgi="${wwwdir}/${base}.cgi"
      echo " $cgi"
      cat <<EOF > $cgi
#!/bin/sh

args="-d ${dbdir}/${db} --palette=rainbow --list --header=header.htm --footer=footer.htm"

${duc} cgi \$args

EOF

      chmod +x $cgi
    fi
done

if  [ "$mkcgi" = "0" ]; then
  echo " </TBODY>"
  echo "</TABLE>"
  echo "</BODY>"
  echo "</HTML>"
fi

