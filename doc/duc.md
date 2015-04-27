
duc(1) -- index, query and graph disk usage
===========================================

## SYNOPSIS

`duc` <subcommand> [options]


## DESCRIPTION

Duc is a collection of tools for inspecting and visualizing disk usage. 

Duc maintains an indexed database of accumulated sizes of directories of your
file system, and allows you to query this database with some tools, or create
fancy sunburst graphs to show you where your bytes are.

Duc scales quite well, it has been tested on systems with more then 500 million
files and several petabytes of storage. 


## USAGE

Duc comes with a command line tool called `duc`, which is used to create,
maintain and query the disk usage database.  run `duc help` to get a list of
available commands. `duc help <subcommand>` describes the usage of a specific
subcommand. Run `duc help --all` for an extensive list of all commands and
their options.

Duc allows any option to be placed either on the command line or in a
configuration file. Options on the command line are preceded by a
double-leading-dash (`--option`), some options have a corresponding short
option which can be used as well with a single leading dash. (`-o`)

At startup duc tries to read its configuration from three locations in this
particular order: `/etc/ducrc`, `~/.ducrc` and `./.ducrc`.

A configuration file consists of sections and parameters. The section names
correspond to the duc subcommands for which the parameters in that section
apply. A section begins with the name of the section in square brackets and
continues until the next section begins.Sections contain parameters, one per
line, which consist of a single option name for boolean flags, or a option name
and a value for options which take a value. See the EXAMPLES section for an
example of the configuration file format.


## CREATING THE INDEX

Duc needs an index file of the file system before it is able to show any
information.  To create the index, run the `duc index` command. For example, to
create an index of your home directory run `duc index ~`

```
$ duc index /usr
Skipping lost+found: Permission denied
Indexed 333823 files and 48200 directories, (35.0GB total) in 1 seconds
```

The default location of the database is `$HOME/.duc.db`. To use a different
database location, use the DUC_DATABASE environment variable or specify the
database location with the --database argument.

You can run `duc index` at any time later to rebuild the index.


## QUERYING THE INDEX

Duc has various subcommands for querying or exploring the index:

* `duc info` shows a list of available directory trees in the database, and the time
  and date of the last scan.

* `duc ls` lists all files and directories under the given path on the console.

* `duc ui` runs a ncurses based console user interface for exploring the file
  system usage

* `duc gui` starts a graphical (X11) interface representing the file system in
  a sunburst graph. Click on a directory to redraw the graph from the
  perspective of the selected directory. Click in the center of the graph to go
  up one directory in the tree.


## OPTIONS

This section list all available subcommands and describes their usage and options.

### Global options

    These options apply to all Duc subcommands:
  * `--debug`:
    increase verbosity to debug level

  * `-h`, `--help`:
    show help

  * `-q`, `--quiet`:
    quiet mode, do not print any warning

  * `-v`, `--verbose`:
    increase verbosity

  * `--version`:
    output version information and exit

### duc help

Options for command `duc help [options]`:

  * `-a`, `--all`:
    show complete help for all commands

### duc index

The 'index' subcommand performs a recursive scan of the given paths on the
filesystem and calculates the inclusive size of all directories. The results
are written to the index, and can later be queried by one of the other duc tools.


Options for command `duc index [options] PATH ...`:

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `-d`, `--database=VAL`:
    use database file ARG

  * `-e`, `--exclude=VAL`:
    exclude files matching ARG

  * `-H`, `--check-hard-links`:
    count hard links only once. if two or more hard links point to the same file, only one of the hard links is displayed and counted.


  * `-f`, `--force`:
    force writing in case of corrupted db

  * `--hide-file-names`:
    hide file names in index (privacy). the names of directories will be preserved, but the names of the individual files will be hidden


  * `-m`, `--max-depth=VAL`:
    limit directory names to given depth. when this option is given duc will traverse the complete file system, but will only the first VAL levels of directories in the database to reduce the size of the index


  * `-x`, `--one-file-system`:
    don't cross filesystem boundaries

  * `-p`, `--progress`:
    show progress during indexing

  * `--uncompressed`:
    do not use compression for database. Duc enables compression if the underlying database supports this. This reduces index size at the cost of slightly longer indexing time


### duc info

Options for command `duc info [options]`:

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

### duc ls

The 'ls' subcommand queries the duc database and lists the inclusive size of
all files and directories on the given path. If no path is given the current
working directory is listed.


Options for command `duc ls [options] [PATH]`:

  * `-a`, `--apparent`:
    show apparent instead of actual file size

  * `--ascii`:
    use ASCII characters instead of UTF-8 to draw tree

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `-F`, `--classify`:
    append file type indicator (one of */) to entries

  * `-c`, `--color`:
    colorize output (only on ttys)

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `--dirs-only`:
    list only directories, skip individual files

  * `-g`, `--graph`:
    draw graph with relative size for each entry

  * `-l`, `--levels=VAL`:
    traverse up to ARG levels deep [4]

  * `-R`, `--recursive`:
    list subdirectories in a recursive tree view

### duc xml

Options for command `duc xml [options] [PATH]`:

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `-x`, `--exclude-files`:
    exclude file from xml output, only include directories

  * `-s`, `--min_size=VAL`:
    specify min size for files or directories

### duc cgi

Options for command `duc cgi [options] [PATH]`:

  * `-a`, `--apparent`:
    Show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `--css-url=VAL`:
    url of CSS style sheet to use instead of default CSS

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `--fuzz=VAL`:
    use radius fuzz factor when drawing graph [0.7]

  * `-l`, `--levels=VAL`:
    draw up to ARG levels deep [4]

  * `--list`:
    generate table with file list

  * `--palette=VAL`:
    select palette <size|rainbow|greyscale|monochrome>

  * `-s`, `--size=VAL`:
    image size [800]

### duc graph

The 'graph' subcommand queries the duc database and generates a sunburst graph
showing the disk usage of the given path. If no path is given a graph is created
for the current working directory.

By default the graph is written to the file 'duc.png', which can be overridden by
using the -o/--output option. The output can be sent to stdout by using the special
file name '-'.


Options for command `duc graph [options] [PATH]`:

  * `-a`, `--apparent`:
    Show apparent instead of actual file size

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `-f`, `--format=VAL`:
    select output format <png|svg|pdf> [png]

  * `--fuzz=VAL`:
    use radius fuzz factor when drawing graph [0.7]

  * `-l`, `--levels=VAL`:
    draw up to ARG levels deep [4]

  * `-o`, `--output=VAL`:
    output file name [duc.png]

  * `--palette=VAL`:
    select palette <size|rainbow|greyscale|monochrome>

  * `-s`, `--size=VAL`:
    image size [800]

### duc gui

The 'gui' subcommand queries the duc database and runs an interactive graphical
utility for exploring the disk usage of the given path. If no path is given the
current working directory is explored.

The following keys can be used to navigate and alter the graph:

    +           increase maximum graph depth
    -           decrease maximum graph depth
    0           Set default graph depth
    a           Toggle between apparent and actual disk usage
    b           Toggle between exact byte count and abbreviated sizes
    p           toggle palettes
    f           toggle graph fuzz
    backspace   go up one directory


Options for command `duc gui [options] [PATH]`:

  * `-a`, `--apparent`:
    show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `--fuzz=VAL`:
    use radius fuzz factor when drawing graph

  * `-l`, `--levels=VAL`:
    draw up to ARG levels deep [4]

  * `--palette=VAL`:
    select palette <size|rainbow|greyscale|monochrome>

### duc ui

The 'gui' subcommand queries the duc database and runs an interactive ncurses
utility for exploring the disk usage of the given path. If no path is given the
current working directory is explored.

The following keys can be used to navigate and alter the file system:

    k, up, pgup:     move cursor up
    j, down, pgdn:   move cursor down
    h, left:         go up to parent directory (..)
    l, right, enter: descent into selected directory
    a:               toggle between actual and apparent disk usage
    b:               toggle between exact and abbreviated sizes
    g:               toggle graph
    q, escape:       quit


Options for command `duc ui [options] [PATH]`:

  * `-a`, `--apparent`:
    show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]



## CGI INTERFACING

The `duc` binary has support for a rudimentary CGI interface, currently only
tested with apache.  The CGI interface generates a simple HTML page with a list
of indexed directories, and shows a clickable graph for navigating the file
system. If the option `--list` is given, a list of top sized files/dirs is also
written.

Configuration is done by creating a simple shell script as .cgi in a directory
which is configured for CGI execution by your web server (usually
`/usr/lib/cgi-bin`). The shell script should simply start duc, and pass the
location of the database to offer.

An example duc.cgi script would be

```
#!/bin/sh
/usr/local/bin/duc cgi -d /home/jenny/.duc.db
```

* Make sure the database file is readable by the user (usually www-data)
* Debugging is best done by inspecting the web server's error log
* Make sure the .cgi script has execute permissions (`chmod +x duc.cgi`)

The HTML page is generated with a simple embedded CSS style sheet. If the style
is not to your liking you can provide an external CSS url with the --css-url
option which will then be used instead of the embedded style definition.

The current CGI configuration is not very flexible, nor secure. Use at your own
risk!


## A NOTE ON FILE SIZE AND DISK USAGE

The concepts of 'file size' and 'disk usage' can be a bit confusing. Files on
disk have an apparent size, which indicates how much bytes are in the file from
the users point of view; this is the size reported by tools like `ls -l`. The
apparent size can be any number, from 0 bytes up to several TB.  The actual
number of bytes which are used on the filesystem to store the file can differ
from this apparent size for a number of reasons: disks store data in blocks,
which cause files to always take up a multiple of the block size, files can
have holes ('sparse' files), and other technical reasons. This number is always
a multiple of 512, which means that the actual size used for a file is almost
always a bit more then its apparent size.

Duc has two modes for counting file sizes:

- `apparent size`: this is the size as reported by `ls`. This number indicates
  the file length, which is usually smaller then the actual disk usage. In this
  mode, directories themselves do not have a size.

- `actual size`: this is the size as reported by `du` and `df`. The actual file
  size tells you how much disk is actually used by a file, and is alwasys a
  multiple of 512 bytes. In this mode, the blocks used to store the directory
  information are counted as well.

The default mode used by duc is to use the 'actual size'. Most duc commands to
report disk usage (`duc ls`, `duc graph`, `duc gui`, etc) have an option to
change between these two modes (usually the `-a`), in the gui tool use the 'a'
key to toggle.


## EXAMPLES


Index the /usr directory, writing to the default database location ~/.duc.db:

   $ duc index /usr

List all files and directories under /usr/local, showing relative file sizes
in a graph:

    $ duc ls -Fg /usr/local
      4.7G lib/                 [+++++++++++++++++++++++++++++++++++++++++++]
      3.1G share/               [++++++++++++++++++++++++++++               ]
      2.7G src/                 [++++++++++++++++++++++++                   ]
    814.9M bin/                 [+++++++                                    ]
    196.6M include/             [+                                          ]
     66.6M x86_64-w64-mingw32/  [                                           ]
     59.9M local/               [                                           ]
     38.8M i686-w64-mingw32/    [                                           ]
     20.3M sbin/                [                                           ]
     13.6M lib32/               [                                           ]
     13.3M libx32/              [                                           ]

or use the -R  options for the tree view:

    $ duc ls -RF /etc/logcheck
     24.0K `+- ignore.d.server/
      4.0K  |  `+- hddtemp 
      4.0K  |   |- ntpdate 
      4.0K  |   |- lirc 
      4.0K  |   |- rsyslog 
      4.0K  |   `- libsasl2-modules 
      8.0K  |- ignore.d.workstation/
      4.0K  |   `- lirc 
      8.0K  `- ignore.d.paranoid/
      4.0K      `- lirc 

Start the graphical interface to explore the file system using sunburst graphs:

    $ duc gui /usr

Generate a graph of /usr/local in .png format:

    $ duc graph -o /tmp/usr.png /usr


The following sample configuration file defines default parameters for the `duc
ls` and `duc gui` commands and defines a global option to configure the database
path which is used by all subcommands
 
    [global]
    database /var/cache/duc.db
 
    [ls]
    recursive
    classify
    color
    
    [gui]
    fuzz 0.7
    palette rainbow
    levels 4


## FILES

At startup duc tries to read its configuration from three locations in this
particular order: `/etc/ducrc`, `~/.ducrc` and `./.ducrc`.

Duc mainains an index of scanned directories, which defaults to ~/.duc.db. All tools
take the -d/--database option to override the database path.


## AUTHORS

* Ico Doornekamp <duc@zevv.nl>
* John Stoffel <john@stoffel.org>

Other contributers can be found in the Git log at GitHub.


## LICENSE

Duc is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
version 2 dated June, 1991. Duc is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
License for more details.

