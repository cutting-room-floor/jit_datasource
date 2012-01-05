## JIT Datasource

A just-in-time datasource for Mapnik. It takes bboxes,
deciphers tiles, downloads and parses GeoJSON.

The GeoJSON datasource is [incubated in its own repository](https://github.com/tmcw/geojson_datasource).

                     -> TileStream          \
    url=tilejson url -> 0/0/0.png -> TileMill             -> bbox[4] -> Mapnik -> JIT -> 0/0/0 -> tiled data server
                     -> *Mapnik Tileserver  /

### Building

if you've got a similar install to me, you can run

    make do

and rebuild and install in one swipe.

### Components

Includes [yajl](http://lloyd.github.com/yajl/) for
JSON parsing.

## Authors

* (tmcw) Tom MacWright

Licensed LGPL
