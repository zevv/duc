
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

Some commands might not be available on your system, depending on the exact
configuration chosen when building Duc. (For example, the `duc gui` command is
not available in the `duc-nox` package on Debian and Ubuntu)

Duc allows any option to be placed either on the command line or in a
configuration file. Options on the command line are preceded by a
double-leading-dash (`--option`), some options have a corresponding short
option which can be used as well with a single leading dash. (`-o`)

At startup duc tries to read its configuration from three locations in this
particular order: `/etc/ducrc`, `~/.config/duc/ducrc`, `~/.ducrc` and
`./.ducrc`.

A configuration file consists of sections and parameters. The section names
correspond to the duc subcommands for which the parameters in that section
apply. A section begins with the name of the section in square brackets and
continues until the next section begins. Sections contain parameters, one per
line, which consist of a single option name for boolean flags, or an option name
and a value for options which take a value. See the EXAMPLES section for an
example of the configuration file format.


## CREATING THE INDEX

Duc needs an index file of the file system before it is able to show any
information.  To create the index, run the `duc index` command. For example, to
create an index of your home directory run `duc index ~`

    $ duc index /usr
    Skipping lost+found: Permission denied
    Indexed 333823 files and 48200 directories, (35.0GB total) in 1 seconds

The default location of the database is `$HOME/.duc.db`. To use a different
database location, use the DUC_DATABASE environment variable or specify the
database location with the --database argument.

You can run `duc index` at any time later to rebuild the index.

By default Duc indexes all directories it encounters during file system
traversal, including special file systems like /proc and /sys, and
network file systems like NFS or Samba mounts. There are a few options to
select what parts of your filesystem you want to include or exclude from the
scan, check the documentation below for the options --one-file-system, 
--exclude, --fs-exclude and --fs-include for more details.


## QUERYING THE INDEX

Duc has various subcommands for querying or exploring the index: (Note that
depending on your configuration, some of these commands might not be available)

* `duc info` shows a list of available directory trees in the database, and the time
  and date of the last scan.

* `duc ls` lists all files and directories under the given path on the console.

* `duc ui` runs a ncurses based console user interface for exploring the file
  system usage.

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
    use database file VAL

  * `-e`, `--exclude=VAL`:
    exclude files matching VAL

  * `-H`, `--check-hard-links`:
    count hard links only once. if two or more hard links point to the same file, only one of the hard links is displayed and counted


  * `-f`, `--force`:
    force writing in case of corrupted db

  * `--fs-exclude=VAL`:
    exclude file system type VAL during indexing. VAL is a comma separated list of file system types as found in your systems fstab, for example ext3,ext4,dosfs


  * `--fs-include=VAL`:
    include file system type VAL during indexing. VAL is a comma separated list of file system types as found in your systems fstab, for example ext3,ext4,dosfs


  * `--hide-file-names`:
    hide file names in index (privacy). the names of directories will be preserved, but the names of the individual files will be hidden


  * `-m`, `--max-depth=VAL`:
    limit directory names to given depth. when this option is given duc will traverse the complete file system, but will only the first VAL levels of directories in the database to reduce the size of the index


  * `-x`, `--one-file-system`:
    skip directories on different file systems

  * `-p`, `--progress`:
    show progress during indexing

  * `--dry-run`:
    do not update database, just crawl

  * `--uncompressed`:
    do not use compression for database. Duc enables compression if the underlying database supports this. This reduces index size at the cost of slightly longer indexing time


### duc info

Options for command `duc info [options]`:

  * `-a`, `--apparent`:
    show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

### duc ls

The 'ls' subcommand queries the duc database and lists the inclusive size of
all files and directories on the given path. If no path is given the current
working directory is listed.


Options for command `duc ls [options] [PATH]...`:

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

  * `--count`:
    show number of files instead of file size

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `-D`, `--directory`:
    list directory itself, not its contents

  * `--dirs-only`:
    list only directories, skip individual files

  * `--full-path`:
    show full path instead of tree in recursive view

  * `-g`, `--graph`:
    draw graph with relative size for each entry

  * `-l`, `--levels=VAL`:
    traverse up to ARG levels deep [4]

  * `-n`, `--name-sort`:
    sort output by name instead of by size

  * `-R`, `--recursive`:
    recursively list subdirectories

### duc xml

Options for command `duc xml [options] [PATH]`:

  * `-a`, `--apparent`:
    interpret min_size/-s value as apparent size

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `-x`, `--exclude-files`:
    exclude file from xml output, only include directories

  * `-s`, `--min_size=VAL`:
    specify min size for files or directories

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

  * `--count`:
    show number of files instead of file size

  * `--dpi=VAL`:
    set destination resolution in DPI [96.0]

  * `-f`, `--format=VAL`:
    select output format <png|svg|pdf|html> [png]

  * `--fuzz=VAL`:
    use radius fuzz factor when drawing graph [0.7]

  * `--gradient`:
    draw graph with color gradient

  * `-l`, `--levels=VAL`:
    draw up to ARG levels deep [4]

  * `-o`, `--output=VAL`:
    output file name [duc.png]

  * `--palette=VAL`:
    select palette. available palettes are: size, rainbow, greyscale, monochrome, classic


  * `--ring-gap=VAL`:
    leave a gap of VAL pixels between rings

  * `-s`, `--size=VAL`:
    image size [800]

### duc cgi

Options for command `duc cgi [options] [PATH]`:

  * `-a`, `--apparent`:
    Show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `--count`:
    show number of files instead of file size

  * `--css-url=VAL`:
    url of CSS style sheet to use instead of default CSS

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `--dpi=VAL`:
    set destination resolution in DPI [96.0]

  * `--footer=VAL`:
    select HTML file to include as footer

  * `--fuzz=VAL`:
    use radius fuzz factor when drawing graph [0.7]

  * `--gradient`:
    draw graph with color gradient

  * `--header=VAL`:
    select HTML file to include as header

  * `-l`, `--levels=VAL`:
    draw up to ARG levels deep [4]

  * `--list`:
    generate table with file list

  * `--palette=VAL`:
    select palette. available palettes are: size, rainbow, greyscale, monochrome, classic


  * `--ring-gap=VAL`:
    leave a gap of VAL pixels between rings

  * `-s`, `--size=VAL`:
    image size [800]

  * `--tooltip`:
    enable tooltip when hovering over the graph. enabling the tooltip will cause an asynchronous HTTP request every time the mouse is moved and can greatly increase the HTTP traffic to the web server


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
    c           Toggle between file size and file count
    f           toggle graph fuzz
    g           toggle graph gradient
    p           toggle palettes
    backspace   go up one directory


Options for command `duc gui [options] [PATH]`:

  * `-a`, `--apparent`:
    show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `--count`:
    show number of files instead of file size

  * `--dark`:
    use dark background color

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `--fuzz=VAL`:
    use radius fuzz factor when drawing graph

  * `--gradient`:
    draw graph with color gradient

  * `-l`, `--levels=VAL`:
    draw up to VAL levels deep [4]

  * `--palette=VAL`:
    select palette. available palettes are: size, rainbow, greyscale, monochrome, classic


  * `--ring-gap=VAL`:
    leave a gap of VAL pixels between rings

### duc ui

The 'ui' subcommand queries the duc database and runs an interactive ncurses
utility for exploring the disk usage of the given path. If no path is given the
current working directory is explored.

The following keys can be used to navigate and alter the file system:

    up, pgup, j:     move cursor up
    down, pgdn, k:   move cursor down
    home, 0:         move cursor to top
    end, $:          move cursor to bottom
    left, backspace: go up to parent directory (..)
    right, enter:    descent into selected directory
    a:               toggle between actual and apparent disk usage
    b:               toggle between exact and abbreviated sizes
    c:               Toggle between file size and file count
    h:               show help. press 'q' to return to the main screen
    n:               toggle sort order between 'size' and 'name'
    o:               try to open the file using xdg-open
    q, escape:       quit


Options for command `duc ui [options] [PATH]`:

  * `-a`, `--apparent`:
    show apparent instead of actual file size

  * `-b`, `--bytes`:
    show file size in exact number of bytes

  * `--count`:
    show number of files instead of file size

  * `-d`, `--database=VAL`:
    select database file to use [~/.duc.db]

  * `-n`, `--name-sort`:
    sort output by name instead of by size

  * `--no-color`:
    do not use colors on terminal output



## CGI INTERFACING

The `duc` binary has support for a rudimentary CGI interface, currently only
tested with apache.  The CGI interface generates a simple HTML page with a list
of indexed directories, and shows a clickable graph for navigating the file
system. If the option `--list` is given, a list of top sized files/dirs is also
written.

Configuration is done by creating a simple shell script as .cgi in a directory
which is configured for CGI execution by your web server (usually
`/usr/lib/cgi-bin`). The shell script should simply start duc, and pass the
location of the database to navigate.

An example duc.cgi script would be

    #!/bin/sh
    /usr/local/bin/duc cgi -d /home/jenny/.duc.db

* Make sure the database file is readable by the user (usually www-data)
* Debugging is best done by inspecting the web server's error log
* Make sure the .cgi script has execute permissions (`chmod +x duc.cgi`)

Some notes:

* The HTML page is generated with a simple embedded CSS style sheet. If the
  style is not to your liking you can provide an external CSS url with the
  --css-url option which will then be used instead of the embedded style
  definition.

* Add the option --list to generate a table of top sized files and directories
  in the HTML page.

* The options --header and --footer allow you to insert your own HTML code
  before and after the main.

The current CGI configuration is not very flexible, nor secure. It is not
advised to run the CGI from public reachable web servers, use at your own risk.


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
  the file length, which is usually smaller then the actual disk usage. 

- `actual size`: this is the size as reported by `du` and `df`. The actual file
  size tells you how much disk is actually used by a file, and is always a
  multiple of 512 bytes. 

The default mode used by duc is to use the 'actual size'. Most duc commands to
report disk usage (`duc ls`, `duc graph`, `duc ui`, etc) have an option to
change between these two modes (usually the `-a`), or use the 'a' key to
toggle.

## BUILDING from git

If you use git clone to pull down the latest release, you will have to
do the following:

  git clone https://github.com/zevv/duc  
  cd duc  
  autoreconf -i

Then you can run the regular 

  ./configure [ options ]  
  make  

to the regular build of the software.

A note for Redhat and derivates users. The package providing the development file
for lmdb (lmdb-devel) does not include a lmdb.pc pkgconfig file. This could lead to
errors during the configure phase:

  checking for LMDB... no  
  configure: error: Package requirements (lmdb) were not met:  
                                                                                     
To avoid the need to call pkg-config, you may set the environment variables          
LMDB_CFLAGS and LMDB_LIBS:

  LMDB_CFLAGS=" " LMDB_LIBS=-llmdb ./configure --with-db-backend=lmdb [ options ]  

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
ls` and `duc ui` commands and defines a global option to configure the database
path which is used by all subcommands
 
    [global]
    database /var/cache/duc.db
 
    [ls]
    recursive
    classify
    color
    
    [ui]
    no-color
    apparent


## FREQUENTLY ASKED QUESTIONS

* What does the error 'Database version mismatch mean?'
 
  The layout of the index database sometimes changes when new features are
  implemented. When you get this error you have probably upgraded to a newer
  version. Just remove the old database file and rebuild the index.

* Duc crashes with a segmentation fault, is it that buggy?

  By default Duc uses the Tokyocabinet database backend. Tokyocabinet is pretty
  fast, stores the database in a single file and has nice compression support
  to keep the database small. Unfortunately, it is not always robust and
  sometimes chokes on corrupt database files. Try to remove the database
  and rebuild the index. If the error persists contact the authors.

* Some of the Duc subcommands like `duc gui` are not available on my system?

  Depending on the configuration that was chosen when building Duc, some
  options might or might not be available in the `duc` utility. For example, on
  Debian or Ubuntu Duc comes in two flavours: there is a full featured package
  called `duc`, or a package without dependencies on X-windows called
  `duc-nox`, for which the latter lacks the `duc gui` command.

## FILES

At startup duc tries to read its configuration from three locations in this
particular order: `/etc/ducrc`, `~/.config/duc/ducrc`, `~/.ducrc` and
`./.ducrc`.

Duc mainains an index of scanned directories, which defaults to ~/.duc.db. All tools
take the -d/--database option to override the database path.


## AUTHORS

* Ico Doornekamp <duc@zevv.nl>
* John Stoffel <john@stoffel.org>

Other contributors can be found in the Git log at GitHub.


## LICENSE

Duc is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
version 2 dated June, 1991. Duc is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
License for more details.

