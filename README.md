
# duc

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

Duc comes with a command line tool called 'duc', which is used to create,
maintain and query the disk usage database.  run `duc help` to get a list of
available commands. 'duc help <subcommand>' describes the usage of a specific
subcommand.

Duc needs an index file of the file system before it is able to show any
information.  To create the index, run the 'duc index' command. For example, to
create an index of your home directory run:

```
$ duc index ~
```

The default location of the database is ~/.duc.db. To use a different database
location, use the DUC_DATABASE environment variable or specify the database
location with the --database argument.


TL;DR

```
$ duc help
$ duc help index
$ duc index /usr
$ duc ls /usr/bin
$ duc graph /usr
```


### history

Duc is the replacement for Philesight[1], which I wrote a few years ago but has
some shortcomings (slow indexing, large database) which I felt were not simple
to fix.

Instead of Ruby, Duc is written in plain C, which is probably as fast as it will get.

1. http://zevv.nl/play/code/philesight/

