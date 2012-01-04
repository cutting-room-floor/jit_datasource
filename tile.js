#!/usr/bin/env node

var mapnik = require('mapnik'),
    fs = require('fs'),
    express = require('express'),
    sm = new (require('sphericalmercator'))(),
    app = express.createServer(),
    path = require('path'),
    port = 20020;

var stylesheet = path.join(__dirname, 'test.xml');

try {
  // create map
  var map = new mapnik.Map(256, 256); // , mercator.proj4);
  map.load(stylesheet, {
      strict:true
  }, function(err, map) {
      if (err) {
          console.log(err);
          res.send();
      }
      app.get('/:z/:x/:y.png', function(req, res, next){
          // calculate the bounding box for each tile
          var bbox = sm.bbox(parseInt(req.params.x, 10),
              parseInt(req.params.y, 10),
              parseInt(req.params.z, 10), false);
          // render map
          var im = new mapnik.Image(map.width, map.height);
          map.extent = bbox;
          map.render(im, function(err, im) {
            if (err) {
              throw err;
            } else {
              res.statusCode = 200;
              res.setHeader('Content-Type', 'image/png');
              res.end(im.encodeSync('png'));
            }
          });
      });
  });
} catch (err) {
  res.statusCode = 500;
  res.setHeader('Content-Type', 'text/plain');
  res.end(err.message);
}

app.listen(port);
console.log("Server running on port " + port);
