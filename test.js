#!/usr/bin/env node

var mapnik = require('mapnik'),
    fs = require('fs'),
    sm = new (require('sphericalmercator'))();

var t = sm.bbox(100, 100, 10);
var map = new mapnik.Map(256, 256);
var image = new mapnik.Image(256, 256);

console.log('creating datasource');

var jit = new mapnik.Datasource({
    type: 'jit',
    url: 'http://ssd.local/view/osm.road.json'
});

console.log(jit);
console.log(jit.describe());

map.bufferSize = 0;
map.fromString(fs.readFileSync('test_tm.xml', 'utf-8'),
    function(err, m) {
    // m.extent = sm.bbox(4959, 6060, 14);
    m.extent = sm.bbox(6060, 4959, 14);
    m.renderFileSync('/tmp/0_0_0.png');
});
