# Replaced by [tilelive-vector](https://github.com/mapbox/tilelive-vector) et al

## JIT Datasource

A just-in-time datasource for Mapnik. It takes bboxes,
deciphers tiles, downloads and parses GeoJSON.

### Building

if you've got a similar install to me, you can run

    make do

and rebuild and install in one swipe.

### Testing

Right now there's a mocking server in `test/mock/server.py`. It's based
on `BaseHTTPServer`, so shouldn't require any additional python modules.

Run it and you should get a view at `http://localhost:9000/view/osm.road.json`.

### Components

Includes [yajl](http://lloyd.github.com/yajl/) for
JSON parsing.

## Authors

* (tmcw) Tom MacWright
