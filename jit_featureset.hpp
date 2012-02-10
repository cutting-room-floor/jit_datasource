#ifndef JIT_FEATURESET_HPP
#define JIT_FEATURESET_HPP

#include <mapnik/datasource.hpp>
#include <boost/scoped_ptr.hpp>
#include <vector>

class jit_featureset : public mapnik::Featureset
{
public:
    jit_featureset(mapnik::box2d<double> const& box,
                   int zoom, std::string const& url,
                   std::string const& encoding);
    virtual ~jit_featureset();
    mapnik::feature_ptr next();

private:
    mapnik::box2d<double> box_;
    mutable unsigned int feature_id_;
    mutable std::string input_string_;
    boost::shared_ptr<mapnik::transcoder> tr_;
    mutable std::vector<mapnik::feature_ptr> features_;
};

#endif // JIT_FEATURESET_HPP
