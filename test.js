#!/usr/bin/env node

var mapnik = require('mapnik'),
    fs = require('fs'),
    sm = new (require('sphericalmercator'))();

var t = sm.bbox(100, 100, 10);
var map = new mapnik.Map(256, 256);
var image = new mapnik.Image(256, 256);
map.bufferSize = 0;
map.fromString(fs.readFileSync('test.xml', 'utf-8'),
    function(err, m) {
        var i = 14;
        var xyz = sm.bbox(1, 1, i);
        var ll = xyz;
        m.extent = sm.bbox(1, 1, i);
        console.log(ll);
        console.log('mid from here: ' + ((ll[0] + ll[2]) / 2));
        console.log('mid from here: ' + ((ll[1] + ll[3]) / 2));
        var px = sm.px(
            [(ll[0] + ll[2]) / 2,
             (ll[1] + ll[3]) / 2], i);
        m.renderFileSync('/tmp/100_100_10.png');
});
