#!/usr/bin/env node

var mapnik = require('mapnik'),
    fs = require('fs'),
    sm = new (require('./sphericalmercator.js'))();

    console.log(sm.convert(sm.bbox(4959, 6060, 14), '900913'));
var mercproj4 = '+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over';

var t = sm.bbox(100, 100, 10);
var map = new mapnik.Map(256, 256, mercproj4);
var image = new mapnik.Image(256, 256);

var jit = new mapnik.Datasource({
    type: 'jit',
    url: 'http://ssd.local/view/osm.road.json'
});

map.bufferSize = 0;
map.fromString(fs.readFileSync('test_tm.xml', 'utf-8'),
    function(err, m) {
    m.extent = sm.convert(sm.bbox(4959, 6060, 14), '900913');
    console.log(sm.convert(sm.bbox(4959, 6060, 14), '900913'));
    m.renderFileSync('/tmp/0_0_0.png');
});
