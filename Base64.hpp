#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/algorithm/string.hpp>

class Base64 {
public:
	static std::string decode(const std::string &val);
	static std::string encode(const std::string &val);
};
