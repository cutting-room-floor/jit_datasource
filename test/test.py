#encoding: utf8
import itertools
from nose.tools import *
import unittest, mapnik

class JitDatasource(unittest.TestCase):
    ids = itertools.count(0)

    def test_constructor(self, *args, **kw):
        ds = mapnik.CreateDatasource({'type':'jit', 'url':'http://localhost:9000/view/osm.road.json'})
        return ds

    def test_statistics(self, *args, **kw):
        ds = mapnik.CreateDatasource({'type':'jit', 'url':'http://localhost:9000/view/osm.road.json'})
        stats = ds.statistics()
        assert_true(stats.has_key('z_order'))
        assert_equal(stats['z_order']['minimum'], -10)

    def test_geometry_type(self, *args, **kw):
        ds = mapnik.CreateDatasource({'type':'jit', 'url':'http://localhost:9000/view/osm.road.json'})
        t = ds.describe()
        eq_(str(t['geometry_type']), 'LineString')
        return ds

if __name__ == "__main__":
    [eval(run)() for run in dir() if 'test_' in run]
