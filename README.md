

![Duc](/img/duc.png) 

Dude, where are my bytes!

### introduction

Duc is a small library and a collection of tools for inspecting and visualizing
disk usage. 

Duc maintains a database of accumulated sizes of directories of your file
system, and allows you to query this database with some tools, or create fancy
graphs showing you where your bytes are.


### install

Duc depends on the Tokyo Cabinet [1] database library, and on Cairo [2] for writing
graphs.

1. http://fallabs.com/tokyocabinet/
2. http://cairographics.org/

On Ubuntu or Debian, the following will install all dependencies:

$ sudo apt-get install libcairo2-dev libtokyocabinet-dev


### usage

Duc comes with a command line tool called `duc`, which is used to create,
maintain and query the disk usage database.  run `duc help` to get a list of
available commands. `duc help <subcommand>` describes the usage of a specific
subcommand.


#### Creating the index

Duc needs an index file of the file system before it is able to show any
information.  To create the index, run the `duc index` command. For example, to
create an index of your home directory run `duc index ~`

The default location of the database is `$HOME/.duc.db`. To use a different database
location, use the DUC_DATABASE environment variable or specify the database
location with the --database argument.

You can run `duc index` at any time later to rebuild the index.


#### Querying the index

Use the `duc ls` command to see the disk usage of a directory. A specific path
can be specified as command line argument, the current directory is used if omitted.

`dus ls` has some options similar to the normal `ls` program. For example, try
`duc ls -hF` for a more friendly output.

```
$ duc ls -n10 -hF /lib
modules/                  24.9M ##################################################################
x86_64-linux-gnu/         14.9M #######################################
i386-linux-gnu/           11.5M ###############################
udev/                     10.7M #############################
firmware/                 10.4M ###########################
discover/                  4.1M ###########
xtables/                   1.1M ###
x86_64-linux-musl/       523.1K #
systemd/                 209.9K 

Omitted files            403.2K
```

For a graphical representation of the disk usage, use the command `duc draw`

![Example](/img/example.png) 

#### TL;DR

```
$ duc help
$ duc help index
$ duc index /usr
$ duc ls /usr/bin
$ duc draw /usr
```


### history

Duc is the replacement for Philesight[1], which I wrote a few years ago but has
some shortcomings (slow indexing, large database) which I felt were not simple
to fix.

Instead of Ruby, Duc is written in plain C, which is probably as fast as it
will get. Duc is about ten times faster then Philesight when indexing, with a
database size which is about eight times smaller.

1. http://zevv.nl/play/code/philesight/


### License

This package is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 dated June, 1991. This package is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details.

