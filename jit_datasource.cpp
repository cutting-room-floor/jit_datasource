// file plugin
#include "jit_datasource.hpp"
#include "jit_featureset.hpp"

// curl
#include "basiccurl.h"

// boost
#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>

// mapnik
#include <mapnik/box2d.hpp>
#include <mapnik/proj_transform.hpp>
#include <mapnik/projection.hpp>

using mapnik::datasource;
using mapnik::parameters;

DATASOURCE_PLUGIN(jit_datasource)

jit_datasource::jit_datasource(parameters const& params, bool bind)
: datasource(params),
    type_(datasource::Vector),
    desc_(*params_.get<std::string>("type"),
        *params_.get<std::string>("encoding","utf-8")),
    url_(*params_.get<std::string>("url","")),
    extent_()
{
    if (url_.empty()) throw mapnik::datasource_exception("JIT Plugin: missing <url> parameter");
    if (bind)
    {
        this->bind();
    }
}

mapnik::projection const *_merc =  new mapnik::projection("+init=epsg:3857");
mapnik::projection const *_wgs84 = new mapnik::projection("+init=epsg:4326");
mapnik::proj_transform const *transformer_ = new mapnik::proj_transform(*_merc, *_wgs84);

void jit_datasource::bind() const
{
    if (is_bound_) return;

    extent_.init(-20037508.34,-20037508.34,20037508.34,20037508.34);

    is_bound_ = true;
}

jit_datasource::~jit_datasource() { }

// This name must match the plugin filename, eg 'jit.input'
std::string const jit_datasource::name_="jit";

std::string jit_datasource::name()
{
    return name_;
}

int jit_datasource::type() const
{
    return type_;
}

mapnik::box2d<double> jit_datasource::envelope() const
{
    if (!is_bound_) bind();

    return extent_;
}

mapnik::layer_descriptor jit_datasource::get_descriptor() const
{
    if (!is_bound_) bind();

    return desc_;
}

mapnik::featureset_ptr jit_datasource::features(mapnik::query const& q) const
{
    if (!is_bound_) bind();

    mapnik::box2d <double> bb = q.get_unbuffered_bbox();

    if (bb.width() == 0) {
        // Invalid tiles mean we'll do dangerous math.
        return mapnik::featureset_ptr();
    }

    const double MAXEXTENT = 20037508.34;
    const double D2R = M_PI / 180.0;

    transformer_->backward(bb);
    // we're in mercator for a second
    // FIXME: breaks under z2
    double z = ceil(-(std::log(bb.width() / MAXEXTENT) - std::log(2.0)) / std::log(2.0));
    // and then back in lat/lon
    transformer_->forward(bb);

    double d = 256.0 * std::pow(2.0, z - 1.0);

    mapnik::coord2d c = bb.center();


    double Bc = (256.0 * std::pow(2.0, z)) / 360.0;
    double Cc = (256.0 * std::pow(2.0, z)) / (2 * M_PI);

    double f = std::min(std::max(std::sin(D2R * c.y), -0.9999), 0.9999);
    double x = floor(d + c.x * Bc);

    double lr = std::log((1.0 + f) / (1.0 - f));
    // The deathline
    double y = d + (0.5 * lr) * (-Cc);

    std::clog << "tile: " << (x / 256.0) << ", " << (y / 256.0) << "\n";

    /*
    double px = M_PI * bb.minx() / 180.0;
    double py = M_PI * bb.miny() / 180.0;
    py = std::log(std::tan(0.25 * M_PI + 0.5 * py));
    double ax = 0.15915494309189535;
    double by = -0.15915494309189535;
    px = ax * px + 0 * py + 0.5;
    py = 0 * px + by * py + 0.5;

    double zoom = floor((
        std::log(2) -
        std::log(q.get_bbox().width() / (20037508.34 * 2)) /
        std::log(2)));

    double power = std::pow(2.0, zoom);

    px = ceil(px * power);
    py = ceil(py * power);

    thisurl_ = boost::replace_all_copy(
        boost::replace_all_copy(
        boost::replace_all_copy(url_,
                "{z}", boost::lexical_cast<std::string>(zoom)),
                "{x}", boost::lexical_cast<std::string>(px)),
                "{y}", boost::lexical_cast<std::string>(py));

    // std::clog << thisurl_ << "\n";
    std::clog << "fetching\n";
    std::clog << thisurl_ << "\n";
    CURL_LOAD_DATA* resp = grab_http_response(thisurl_.c_str());

    std::clog << "fetched\n";

    if (resp != NULL)
    {
        char *blx = new char[resp->nbytes + 1];
        memcpy(blx, resp->data, resp->nbytes);
        blx[resp->nbytes] = '\0';

        delete[] blx;

        std::string dstring = boost::trim_left_copy(std::string(resp->data));
    }
    else
    {
        std::clog << "fail" << "\n";
    }

    // if the query box intersects our world extent then query for features
    if (extent_.intersects(q.get_bbox()))
    {
        return boost::make_shared<jit_featureset>(q.get_bbox(), desc_.get_encoding());
    }
    */

    // otherwise return an empty featureset pointer
    return mapnik::featureset_ptr();
}

mapnik::featureset_ptr jit_datasource::features_at_point(mapnik::coord2d const& pt) const
{
    if (!is_bound_) bind();

    // features_at_point is rarely used - only by custom applications,
    // so for this sample plugin let's do nothing...
    return mapnik::featureset_ptr();
}
