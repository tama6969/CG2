#include "StringUtility.h"
#include <Windows.h>

namespace StringUtility {
    std::string ConvertString(const std::wstring& str) {
        if (str.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, str.data(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, str.data(), -1, result.data(), size, nullptr, nullptr);
        result.pop_back();
        return result;
    }

    std::wstring ConvertString(const std::string& str) {
        if (str.empty()) {
            return std::wstring();
        }

        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
        if (sizeNeeded == 0) {
            return std::wstring();
        }
        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
        return result;
    }
}
