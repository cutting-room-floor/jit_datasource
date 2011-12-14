#ifndef FILE_DATASOURCE_HPP
#define FILE_DATASOURCE_HPP

// mapnik
#include <mapnik/datasource.hpp>

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
    const mapnik::projection merc_;
    const mapnik::projection wgs84_;
    int type_;
    mutable mapnik::layer_descriptor desc_;
    mutable std::string url_;
    mutable std::string thisurl_;
    mutable mapnik::box2d<double> extent_;
};


#endif // FILE_DATASOURCE_HPP
