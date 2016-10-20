#!/bin/bash
#
# Build an index of duc DBs and thier Paths, generating a basic web page to be used as index.htm 
#

url="http://localhost:8000"
ducdir=/var/tmp/duc-cgi-test
duc=${ducdir}/duc
dbdir=${ducdir}

#---------------------------------------------------------------------
function puthead()
{        
    echo "<head>"
    echo "  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=ISO-8859-1\" />"

    echo "  <title>${title}</title>"
    echo "</head>"

    echo "These graphs are produced using the tool <a href=\"http://github.com/zevv/duc/\">duc</a>, #ocally patched and updated.<p>"
}

#---------------------------------------------------------------------
function footer()
{
  echo ""
  echo "<hr>"
  echo "Return the <A HREF=\"http://somewhere/\">General Homepage</A>"
  echo "<P>"
  echo "Last updated: 10/20/2016"
  
  echo "</body>"
  echo "</html>"
  echo ""
}

#---------------------------------------------------------------------
# Gets a quantity of seconds and formats the output into Days, Hours, Minutes and seconds
#
# @parm Seconds
# @return string containing days, hours Minutes and seconds example: 0D 0H 0M 1S
#
# dependencies:none
#
#===============================================================================
function SecondsToDaysHoursMinutesSeconds()
{
    local seconds=$1
    local days=$(($seconds/86400))
    seconds=$(($seconds-($days*86400) ))
    local hours=$(($seconds/3600))
    seconds=$((seconds-($hours*3600) ))
    local minutes=$(($seconds/60))
    seconds=$(( $seconds-($minutes*60) ))
    echo -n "${days} days ${hours} hours"
}

#===============================================================================
# Gets the age of a file in seconds
#
# @parm a file name (example: /etc/hosts
# @return Age in seconds
#
# dependencies:none
#
#===============================================================================
function FileAge()
{
    echo $((`date +%s` - `stat -c %Z $1`))
}

echo $(SecondsToDaysHoursMinutesSeconds $(FileAge /etc/hosts) )

puthead

echo "<h1>Multiple Disk Usage Graphs</h1>"
echo ""
echo "<table border=0 cellpadding=2 cellspacing=2 width=\"50%\">"
echo " <tr>"
echo "  <th align=\"left\">Directory Path</th>"
echo "  <th align=\"left\">Date Last updated</th>"
echo "  <th align=\"left\">Difference</th>"
echo " </tr>"

now=`date +%s`
for db in ${dbdir}/*.db; do
    justdb=`echo $db | awk -F/ '{print $NF}' |sed -e 's/.db//'`
    path=`${duc} info -d ${db} | tail -1 | awk '{print $NF}'`
    age=`stat -c %z ${db} | awk '{print $1,$2 }' | sed -E 's/\.[0-9]+$//'`
    secs=`stat -c %Z ${db}`
    diff=$(( $now - $secs))
    diff=$( SecondsToDaysHoursMinutesSeconds $diff )
    echo " <tr>"
    echo "  <td><a href=\"/cgi-bin/duc-test.cgi&db=${justdb}\">${path}</a></td>"
    echo "  <td>${age}</td>"
    echo "  <td>${diff}</td>"
    echo " </tr>"
done

footer
