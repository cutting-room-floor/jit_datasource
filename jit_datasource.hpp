#ifndef FILE_DATASOURCE_HPP
#define FILE_DATASOURCE_HPP

// mapnik
#include <mapnik/datasource.hpp>

const std::string MERCATOR_PROJ4 = "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0.0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over";

class jit_datasource : public mapnik::datasource
{
public:
    // constructor
    jit_datasource(mapnik::parameters const& params, bool bind=true);
    virtual ~jit_datasource ();
    int type() const;
    static std::string name();
    mapnik::featureset_ptr features(mapnik::query const& q) const;
    mapnik::featureset_ptr features_at_point(mapnik::coord2d const& pt) const;
    mapnik::box2d<double> envelope() const;
    mapnik::layer_descriptor get_descriptor() const;
    void bind() const;

private:
    // recommended naming convention of datasource members:
    // name_, type_, extent_, and desc_
    static const std::string name_;
    int type_;
    mutable mapnik::layer_descriptor desc_;
    mutable std::string url_;
    mutable std::string tileurl_;
    mutable std::string thisurl_;
    mutable int minzoom_;
    mutable int maxzoom_;
    mutable mapnik::box2d<double> extent_;
};


#endif // FILE_DATASOURCE_HPP
