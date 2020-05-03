#include <algorithm>
#include <vector>

namespace std_h {
	template <typename T>
	static bool isFind(std::vector<T> &v, T &value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			return true;
		}

		return false;
	}
}
