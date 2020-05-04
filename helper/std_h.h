#include <algorithm>
#include <vector>

#ifndef STD_H
#define STD_H

namespace std_h {
template <class _InIt, class _Ty>
static bool isFind(const std::vector<_InIt> &v, const _Ty &value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			return true;
		}

		return false;
	}

	template <class _InIt, class _Ty>
	static bool erase(std::vector<_InIt> &v, const _Ty &value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			v.erase(it_find);
			return true;
		}
		return false;
	}

	// need_update : bad code to help recode, need to be erased in futur
	template <class _InIt, class _Ty>
	static int getIndex(const std::vector<_InIt> &v, const _Ty &value) {
		auto it_find = std::find(v.begin(), v.end(), value);

		if (it_find != v.end()) {
			return std::distance(v.begin(), it_find);
		}
		return -1;
	}

	template <typename T>
	static void appendArray(std::vector<T> &v1, const std::vector<T> &v2) {
		v1.insert(v1.end(), v2.begin(), v2.end());
	}
}

#endif // !STD_H
