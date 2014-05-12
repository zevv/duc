
# duc

Dude, where are my bytes!


### introduction

Duc is a small library and a collection of tools for inspecting and visualizing disk usage.


### install

Duc depends on the Tokyo Cabinet [1] database library, and on Cairo [2] for writing
graphs.

1. http://fallabs.com/tokyocabinet/
2. http://cairographics.org/

On Ubuntu or Debian, the following will install all dependencies:

$ sudo apt-get install libcairo2-dev libtokyocabinet-dev


### usage

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

