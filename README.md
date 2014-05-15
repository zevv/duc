
![Duc](/img/duc.png) 

### Introduction

Duc is a small library and a collection of tools for inspecting and visualizing
disk usage. 

Duc maintains a database of accumulated sizes of directories of your file
system, and allows you to query this database with some tools, or create fancy
graphs showing you where your bytes are.


### Install

Duc depends on the Tokyo Cabinet [1] database library, and on Cairo [2] and
Pango [3] for writing graphs.

1. http://fallabs.com/tokyocabinet/
2. http://cairographics.org/
3. http://www.pango.org/


On Ubuntu or Debian, the following will install all dependencies:

  $ sudo apt-get install libcairo2-dev libpango1.0-dev libtokyocabinet-dev

On RHEL or CentOS systems, you need to do:

  $ sudo yum install pango-devel cairo-devel tokyocabinet-devel


### Compiling

Duc use the GNU Autoconf system for compiling if you do not have a
pre-built package.  So you can normally just do:

  $ ./configure
  $ make
  $ make install

to install duc into /usr/local/bin

If you need to edit the source, you will need to have the GNU autoconf
tools installed first, which you can do with:

  Ubuntu/Debian:  

    $ sudo apt-get install autoconf

  RHEL/CentOS:

    $ sudo yum install autoconf

Then you will need to do:

  $ autoreconf --install

to generate the correct files.  Once that is done, you can do the
above configure, make and make install steps.


### Usage

Duc comes with a command line tool called `duc`, which is used to create,
maintain and query the disk usage database.  run `duc help` to get a list of
available commands. `duc help <subcommand>` describes the usage of a specific
subcommand.


#### Creating the index

Duc needs an index file of the file system before it is able to show any
information.  To create the index, run the `duc index` command. For example, to
create an index of your home directory run `duc index ~`

```
$ duc index /usr
Skipping lost+found: Permission denied
Indexed 333823 files and 48200 directories, (35.0GB total) in 1 seconds
```

The default location of the database is `$HOME/.duc.db`. To use a different database
location, use the DUC_DATABASE environment variable or specify the database
location with the --database argument.

You can run `duc index` at any time later to rebuild the index.


#### Querying the index

`duc info` shows a list of available directory trees in the database, and the time
and date of the last scan.

```
Available indices:
 2014-05-14 19:06:27   27.7G /var
 2014-05-14 19:06:30   35.0G /usr
 2014-05-14 19:06:49    6.3G /
```

Use the `duc ls` command to see the disk usage of a directory. A specific path
can be specified as command line argument, the current directory is used if omitted.

`dus ls` has some options similar to the normal `ls` program. For example, try
`duc ls -F` for a more friendly output.

```
$ duc ls -n10 -F /usr
lib/                       4.7G [==================================================]
src/                       4.4G [=============================================     ]
share/                     3.1G [========================                          ]
bin/                     814.2M [============                                      ]
include/                 196.1M [===                                               ]
x86_64-w64-mingw32/       66.6M [=                                                 ]
local/                    59.9M [                                                  ]
i686-w64-mingw32/         38.8M [                                                  ]
sbin/                     20.3M [                                                  ]

Omitted files             29.7M
```

For a graphical representation of the disk usage, use the command `duc draw`

![Example](/img/example.png) 

#### TL;DR

```
$ duc help
$ duc help index
$ duc info
$ duc index /usr
$ duc ls /usr/bin
$ duc draw /usr
```


### History

Duc is the replacement for Philesight[1], which I wrote a few years ago but has
some shortcomings (slow indexing, large database) which I felt were not simple
to fix.

Instead of Ruby, Duc is written in plain C, which is probably as fast as it
will get. Duc is about ten times faster then Philesight when indexing, with a
database size which is about eight times smaller.

1. http://zevv.nl/play/code/philesight/


### Author

Ico Doornekamp <philesight@zevv.nl>


### License

This package is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 dated June, 1991. This package is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

