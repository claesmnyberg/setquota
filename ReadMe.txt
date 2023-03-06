Commandline interface to disk quota on Solaris.
Author: cmn@signedness.org, 2004

Usage: setquota user path [option(s)]
Options:
  -b soft:hard  Set soft and hard block quota (kilo bytes)
  -f soft:hard  Set soft and hard file quota (kilo bytes)

The current disk quota for the partition holding path
will be printed if no options are given.
The mount point is resolved by opening the /etc/mnttab file
and performing a stat(2) on every mount point until a device
with the same ID and an rdev of value 0 is found.
This simplify writing of scripts a lot since knowledge about 
the physical location of for example a home directory is not 
required. User can be a username or an uid.
Tested on Solaris 8 and 9.
