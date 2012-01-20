#include <mapnik/map.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/params.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/config_error.hpp>

// Tests.
#include <assert.h>

#include <iostream>
#include <string>
#include <map>

int main (int argc , char** argv) {
    using namespace mapnik;
    try {
        datasource_cache::instance()->register_datasources("/usr/local/lib/mapnik/input");
        freetype_engine::register_font("/usr/local/lib/mapnik/fonts");

        /*
        Map m(256, 256);
        mapnik::load_map(m, "test_tm.xml", false);

        m.zoom_to_box(box2d<double>(-7907869.1982711945,
            5212393.832822738, -7905423.213366068,
            5214839.817727863));


        image_32 buf(m.width(),m.height());
        agg_renderer<image_32> ren(m,buf);
        ren.apply();

        save_to_file<image_data_32>(buf.data(),"demo.png","png");
        std::clog << "demo.png written.\n";

        */

        parameters p;
        p["type"] = "jit";
        p["url"] = "http://localhost:9000/view/osm.road.json";

        boost::shared_ptr<datasource> ds = datasource_cache::instance()->create(p);

        ds->bind();

        assert(ds->type() == 0);
        std::clog << "+ type: " << ds->type() << std::endl;
        assert(ds->get_geometry_type() == 0);
        std::clog << "+ geometry type: " << ds->get_geometry_type() << std::endl;
        // TODO: test assert for this
        std::clog << "envelope: " << ds->envelope() << std::endl;

        mapnik::layer_descriptor ld = ds->get_descriptor();

        assert(ld.get_encoding() == "utf-8");
        std::clog << "+ encoding: " << ld.get_encoding() << std::endl;

        std::vector<mapnik::attribute_descriptor> const& desc = ld.get_descriptors();
        std::vector<mapnik::attribute_descriptor>::const_iterator itr = desc.begin();
        std::vector<mapnik::attribute_descriptor>::const_iterator end = desc.end();

        while (itr != end) {
            std::clog << itr->get_type() << std::endl;
        }

        std::map<std::string, mapnik::parameters> stats = ds->get_statistics();

        std::map<std::string, mapnik::parameters>::iterator it;

        for (it = stats.begin(); it != stats.end(); it++) {
            std::clog << "name: " << it->first << std::endl;
            parameters::const_iterator k = it->second.begin();
            for (; k != it->second.end(); ++k) {
                std::clog << "  " << k->first << " = " << k->second << "\n";
            }
        }
    }
    catch ( const mapnik::config_error & ex ) {
        std::cerr << "### Configuration error: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch ( const std::exception & ex ) {
        std::cerr << "### std::exception: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch ( ... ) {
        std::cerr << "### Unknown exception." << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
