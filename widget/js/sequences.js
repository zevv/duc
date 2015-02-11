// Dimensions of sunburst.
var width = window.innerWidth;
var height = window.innerHeight;
var radius = Math.min(width, height)/2;

var total = 0;
var breadCumbText = window.innerWidth*10/1920;

// Breadcrumb dimensions: width, height, spacing, width of tip/tail.
var b = {
  w: window.innerWidth*150/1920, h: 30, s: 3, t: 10
};

// make `colors` an ordinal scale
var colors = d3.scale.category10();

// Total size of all segments; we set this later, after loading the data.
var totalSize = 0;

var vis = d3.select("#chart").append("svg:svg")
    .attr("width", width)
    .attr("height", height)
    .append("svg:g")
    .attr("id", "container")
    .attr("transform", "translate(" + width / 2 + "," + height /2 + ")");

var partition = d3.layout.partition()
    .size([2 * Math.PI, radius * radius])
    .value(function(d) { return d.size; });

var arc = d3.svg.arc()
    .startAngle(function(d) { return d.x; })
    .endAngle(function(d) { return d.x + d.dx; })
    .innerRadius(function(d) { return Math.sqrt(d.y); })
    .outerRadius(function(d) { return Math.sqrt(d.y + d.dy); });

// get json
d3.json("duc.json", function(error, json) {
  if (error) return console.warn(error);
  data = json;
  createVisualization(data);
});

function reset() {
    location.reload();
}

// Main function to draw and set up the visualization, once we have the data.
function createVisualization(json) {

  // Basic setup of page elements.
  initializeBreadcrumbTrail();

  // For efficiency, filter nodes to keep only those large enough to see.
  var nodes = partition.nodes(json)
      .filter(function(d) {
      return (d.dx > 0.0001); // 0.005 radians = 0.29 degrees
      });

    var uniqueNames = (function(a) {
        var output = [];
        a.forEach(function(d) {
            if (output.indexOf(d.name) === -1) {
                output.push(d.name);
            }
        });
        return output;
    })(nodes);

  // set domain of colors scale based on data
  colors.domain(uniqueNames);

   path = vis.data([json]).selectAll("path")
      .data(nodes)
      .enter().append("svg:path")
      .attr("display", function(d) { return d.depth ? null : "none"; })
      .attr("d", arc)
      .attr("fill-rule", "evenodd")
      .style("fill", function(d) { return colors(d.name); })
      .style("opacity", 1)
      .on("mouseover", mouseover)
      .on("click", magnify)
      .each(stash);

  // Get total size of the tree = value of root node from partition.
  totalSize = path.node().__data__.value;
 };

 // Distort the specified node to 80% of its parent.
function magnify(node) {
  if (parent = node.parent) {
    var parent,
        x = parent.x,
        k = .8;
    parent.children.forEach(function(sibling) {
      x += reposition(sibling, x, sibling === node
          ? parent.dx * k / node.value
          : parent.dx * (1 - k) / (parent.value - node.value));
    });
  } else {
    reposition(node, 0, node.dx / node.value);
  }

  path.transition()
      .duration(250)
      .attrTween("d", arcTween);
}

// Recursively reposition the node at position x with scale k.
function reposition(node, x, k) {
  node.x = x;
  if (node.children && (n = node.children.length)) {
    var i = -1, n;
    while (++i < n) x += reposition(node.children[i], x, k);
  }
  return node.dx = node.value * k;
}

// Stash the old values for transition.
function stash(d) {
  d.x0 = d.x;
  d.dx0 = d.dx;
}

// Interpolate the arcs in data space.
function arcTween(a) {
  var i = d3.interpolate({x: a.x0, dx: a.dx0}, a);
  return function(t) {
    var b = i(t);
    a.x0 = b.x;
    a.dx0 = b.dx;
    return arc(b);
  };
}

// Fade all but the current sequence, and show it in the breadcrumb trail.
function mouseover(d) {

  d3.select("#nameFolder")
      .text(truncate(d.name, 18));

  d3.select("#sizeFolder")
      .text(filesize(d.size));

  d3.select("#explanation")
      .style("visibility", "");

  var sequenceArray = getAncestors(d);
  updateBreadcrumbs(sequenceArray, d.size);

  // Fade all the segments.
  d3.selectAll("path")
      .style("opacity", 0.3);

  // Then highlight only those that are an ancestor of the current segment.
  vis.selectAll("path")
      .filter(function(node) {
                return (sequenceArray.indexOf(node) >= 0);
              })
      .style("opacity", 1);

}

// Given a node in a partition layout, return an array of all of its ancestor
// nodes, highest first, but excluding the root.
function getAncestors(node) {
  var path = [];
  var current = node;
  while (current.parent) {
    path.unshift(current);
    current = current.parent;
  }
  return path;
}

function initializeBreadcrumbTrail() {
  // Add the svg area.
  var trail = d3.select("#sequence").append("svg:svg")
      .attr("width", width)
      .attr("height", 50)
      .attr("id", "trail");
  // Add the label at the end, for the percentage.
  trail.append("svg:text")
    .attr("id", "endlabel")
    .style("fill", "#000");
}

// Generate a string that describes the points of a breadcrumb polygon.
function breadcrumbPoints(d, i) {
  var points = [];
  points.push("0,0");
  points.push(b.w + ",0");
  points.push(b.w + b.t + "," + (b.h / 2));
  points.push(b.w + "," + b.h);
  points.push("0," + b.h);
  if (i > 0) { // Leftmost breadcrumb; don't include 6th vertex.
    points.push(b.t + "," + (b.h / 2));
  }
  return points.join(" ");
}

// Update the breadcrumb trail to show the current sequence and percentage.
function updateBreadcrumbs(nodeArray, percentageString) {

  // Data join; key function combines name and depth (= position in sequence).
  var g = d3.select("#trail")
      .selectAll("g")
      .data(nodeArray, function(d) { return d.name + d.depth; });

  // Add breadcrumb and label for entering nodes.
  var entering = g.enter().append("svg:g");

  entering.append("svg:polygon")
      .attr("points", breadcrumbPoints)
      .style("fill", function(d) { return colors(d.name); });

  entering.append("svg:text")
      .attr("x", (b.w + b.t) / 2)
      .attr("y", b.h / 2)
      .attr("dy", "0.35em")
      .attr("text-anchor", "middle")
      .text(function(d) { return truncate(d.name, breadCumbText); });

  // Set position for entering and updating nodes.
  g.attr("transform", function(d, i) {
    return "translate(" + i * (b.w + b.s) + ", 0)";
  });

  // Remove exiting nodes.
  g.exit().remove();

  // Now move and update the percentage at the end.
  d3.select("#trail").select("#endlabel")
      .attr("x", (nodeArray.length + 0.5) * (b.w + b.s))
      .attr("y", b.h / 2)
      .attr("dy", "0.35em")
      .attr("text-anchor", "middle")

  // Make the breadcrumb trail visible, if it's hidden.
  d3.select("#trail")
      .style("visibility", "");

}

function truncate(n, len) {
      var ext = n.substring(n.length, n.length).toLowerCase();
      var filename = n.replace(ext,'');
      if(filename.length < len) {
          return n;
      }
      filename = filename.substr(0, len) + (n.length > len ? '...' : '');
      return filename + ext;
};

function getAllChildren(obj) {
    var array = [];

    if (typeof(obj.children) !== 'undefined')
        for(var i = 0; i < obj.children.length; i++) {

        }
}

// Take a 2-column CSV and transform it into a hierarchical structure suitable
// for a partition layout. The first column is a sequence of step names, from
// root to leaf, separated by hyphens. The second column is a count of how
// often that sequence occurred.
function buildHierarchy(csv) {
  var root = {"name": "root", "children": []};
  for (var i = 0; i < csv.length; i++) {
    var sequence = csv[i][0];
    var size = +csv[i][1];
    if (isNaN(size)) { // e.g. if this is a header row
      continue;
    }
    var parts = sequence.split("-");
    var currentNode = root;
    for (var j = 0; j < parts.length; j++) {
      var children = currentNode["children"];
      var nodeName = parts[j];
      var childNode;
      if (j + 1 < parts.length) {
   // Not yet at the end of the sequence; move down the tree.
     var foundChild = false;
     for (var k = 0; k < children.length; k++) {
       if (children[k]["name"] == nodeName) {
         childNode = children[k];
         foundChild = true;
         break;
       }
     }
  // If we don't already have a child node for this branch, create it.
     if (!foundChild) {
       childNode = {"name": nodeName, "children": []};
       children.push(childNode);
     }
     currentNode = childNode;
      } else {
     // Reached the end of the sequence; create a leaf node.
     childNode = {"name": nodeName, "size": size};
     children.push(childNode);
      }
    }
  }
  return root;
};
