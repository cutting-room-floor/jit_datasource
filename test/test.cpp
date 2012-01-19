#include <mapnik/map.hpp>
#include <mapnik/datasource_cache.hpp>
#include <mapnik/font_engine_freetype.hpp>
#include <mapnik/agg_renderer.hpp>
#include <mapnik/load_map.hpp>
#include <mapnik/image_util.hpp>
#include <mapnik/config_error.hpp>

#include <iostream>

int main (int argc , char** argv) {
    using namespace mapnik;
    try {
        datasource_cache::instance()->register_datasources("/usr/local/lib/mapnik/input");
        freetype_engine::register_font("/usr/local/lib/mapnik/fonts");

        Map m(256, 256);
        mapnik::load_map(m, "test_tm.xml", false);

        m.zoom_to_box(box2d<double>(-7907869.1982711945,
            5212393.832822738, -7905423.213366068,
            5214839.817727863));

        image_32 buf(m.width(),m.height());
        agg_renderer<image_32> ren(m,buf);
        ren.apply();

        save_to_file<image_data_32>(buf.data(),"demo.png","png");
        std::cout << "Three maps have been rendered using AGG in the current directory:\n"
            "- demo.png\n"
            "Have a look!\n";

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
