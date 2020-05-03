#include <algorithm>
#include <vector>

namespace std_h {
	template <typename T>
	static bool isFind(const std::vector<T> &v, const T &value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			return true;
		}

		return false;
	}

	template <typename T>
	static bool erase(std::vector<T>& v, const T& value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			v.erase(it_find);
			return true;
		}
		return false;
	}
}
