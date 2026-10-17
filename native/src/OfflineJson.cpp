// Compile Boost.JSON once for the network-free core build. The full build uses
// its already pinned nlohmann_json dependency; both retain JSON value types.
#include <boost/json/src.hpp>
