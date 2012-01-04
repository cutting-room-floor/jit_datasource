#!/usr/bin/env node

var mapnik = require('mapnik'),
    fs = require('fs'),
    express = require('express'),
    sm = new (require('sphericalmercator'))(),
    app = express.createServer(),
    path = require('path'),
    port = 20020;

var stylesheet = path.join(__dirname, 'test.xml');


try { var pool = require('generic-pool').Pool; } catch (e) {}

create = function(size) {
    if (!pool) {
        throw new Error('map pool requires generic-pool to be installed:\nnpm install -g generic-pool');
    }
    return {
        max: size || 5,
        pools: {},
        acquire: function(id, options, callback) {
            if (!this.pools[id]) {
                var that = this;
                this.pools[id] = pool({
                    name: id,
                    create: options.create,
                    destroy: options.destroy,
                    max: that.max,
                    idleTimeoutMillis: options.idleTimeoutMillis || 5000,
                    log: false
                    //reapIntervalMillis
                    //priorityRange
                });
            }
            this.pools[id].acquire(callback, options.priority);
        },
        release: function(id, obj) {
            this.pools[id] && this.pools[id].release(obj);
        }
    };
};


// create a pool of 10 maps
// this allows us to manage concurrency under high load
var maps = create(1);
var proj4 = '+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over';

var acquire = function(id,options,callback) {
    methods = {
        create: function(cb) {
            var obj = new mapnik.Map(256, 256, proj4);
            obj.load(id,{strict:true},function(err,obj) {
                if (err) callback(err,null);
                if (options.buffer_size) obj.buffer_size(options.buffer_size);
                cb(obj)
            })
        },
        destroy: function(obj) {
            obj.clear();
            delete obj;
        }
    };
    maps.acquire(id, methods,
        function(obj) {
            callback(null, obj);
        }
    );
};

app.get('/:z/:x/:y.png', function(req, res, next){
    acquire('test.xml', {}, function(err, map) {
        if (err) {
            console.log(err);
        }
        // calculate the bounding box for each tile
        var bbox = sm.bbox(parseInt(req.params.x, 10),
            parseInt(req.params.y, 10),
            parseInt(req.params.z, 10), false);
        // render map
        var im = new mapnik.Image(map.width, map.height);
        map.extent = bbox;
        map.render(im, function(err, im) {
          maps.release('test.xml', map);
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

app.listen(port);
console.log("Server running on port " + port);
