#include <string>

static bool starts_with(const std::string str, const std::string prefix)
{
	return ((prefix.size() <= str.size()) && std::equal(prefix.begin(), prefix.end(), str.begin()));
}
