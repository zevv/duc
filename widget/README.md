DUC Widget
==========

This is a simple widget based on Sunburst sequences graph (http://bl.ocks.org/kerryrodden/7090426) build with d3.js (http://d3js.org/).

-	You can easily see your disk usage with nice mouseover effect.
-	You click in each portion of to magnify and get more detailed info.

### Configuration

Make sure you have **duc** compiled and installed.

Run `cd widget` to select widget directory.

Inside `duc` script edit:

```sh
INDEX_DIR='/'
DB_DIR='.'
JSON_DIR='.'
```

and set the correct directory to index (`INDEX_DIR`), the path to save the `.duc.db` (`DB_DIR`\) and finally where the parsed json with disk info will be saved (`JSON_DIR`\. If JSON_DIR is different than the `.` change also the path in `js/sequence.js` to load correctly.

### Use

Simply run:

```sh
sudo ./duc #indexing, db conversion and json parsing
```

Now you have a file called `duc.json` inside you directory that will be loaded by *widget.html* throught JavaScript and **d3.js** lib.

Open `widget.html` and you see the graph.
