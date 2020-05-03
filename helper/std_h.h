#include <algorithm>
#include <vector>

#ifndef STD_H
#define STD_H

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

	// need_update : bad code to help recode, need to be erased in futur
	template <typename T>
	static int getIndex(const std::vector<T>& v, const T& value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			return std::distance(v.begin(), it_find);
		}
		return -1;
	}
}

#endif // !STD_H
