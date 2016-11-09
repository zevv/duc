
This is a list of feature requests and enhancements that have come up over the
years.  Most of them are not easy to implement in the current architecture, but
I'm still keeping the requests here for future reference or for if I ever get
bored and have a lot of time on my hands. Anybody is free to pick and implement
any of these tasks, of course!


### Show increase since last index or time period 

  https://github.com/zevv/duc/issues/153

  The main reason I started using this tool is actually trying to find users
  that increase their home directory rapidly. It was easy in the beginning but
  starting to get much harder now that we have more users and I keep forgetting
  the initial size. :) I think it would be nice, and certainly possible?, for
  DUC to show the increase in % since last index or a configured time period
  (say 7 days ago).

### Incremental Indexing
 
  https://github.com/zevv/duc/issues/115

  Just a question, is the indexing process incremental or does it restart the
  indexing from scratch each time?

### Per user filtering

  https://github.com/zevv/duc/issues/30

  At work I have to monitor files that differ on a per-user basis, so it would
  be nice to produce graphs that are limited to a single user and / or use the
  data to produce a graph that showed the percentage of the disk space used by
  a specific user. This way I could send users graphs of the disk usage limited
  to their user.

### Directory mtime optimization

  https://github.com/zevv/duc/issues/101

  When running index against an existing database does duc compare the current
  mtime of directories in the filesystem versus the database to see whether
  readdir() is needed to update the database, or as a performance optimization
  whether an update of that directory can be skipped?

### overall graph of multiple datasets in a database 

  https://github.com/zevv/duc/issues/56


