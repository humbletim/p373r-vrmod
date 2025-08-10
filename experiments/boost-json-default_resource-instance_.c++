// FIXME: figure out if a BOOST_XXX define option exists to normalize this between MSVC and Clang++ usage
// (for now this ensures this global named symbol is available at link time)
// -- humbletim 2025.08.06
#include <boost/json.hpp>
boost::json::detail::default_resource boost::json::detail::default_resource::instance_;
