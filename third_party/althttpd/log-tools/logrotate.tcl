#!/usr/bin/tclsh
#
# Run this TCL script like this:
#
#     tclsh logrotate.tcl
#
# To rotate the althttpd log file located a LOGFILENAME.
# Algorithm:
#
#    If the size of $LOGFILENAME is more than 1GB, then
#    move it to a new file in the same directory named
#    YYYYMMDD.log where YYYYMMDD is the current date.
#    Then compress the YYYYMMDD.log file using xz.
#
set LOGFILENAME /home/www/logs/http.log
if {[file size $LOGFILENAME]<1000000000} {exit 0}
set ymd [clock format [clock seconds] -format %Y%m%d]
set dir [file dirname $LOGFILENAME]
set newname $dir/$ymd.log
file rename -force $LOGFILENAME $newname
exec xz --threads=1 $newname >&stdout 2>&stderr
