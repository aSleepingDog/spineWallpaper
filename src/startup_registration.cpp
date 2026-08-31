#include "startup_registration.h"

#include <algorithm>
#include <cwctype>
#include <system_error>
#include <vector>

#include <windows.h>

namespace startup_registration {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"SpineWallpaper";

std::wstring trim(std::wstring value) {
    const auto isSpace = [](wchar_t character) {
        return std::iswspace(character) != 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](wchar_t character) {
        return !isSpace(character);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](wchar_t character) {
        return !isSpace(character);
    }).base(), value.end());
    return value;
}

std::wstring executableFromCommand(const std::wstring& command) {
    const std::wstring value = trim(command);
    if (value.empty())
        return {};

    if (value.front() == L'"') {
        const std::size_t closingQuote = value.find(L'"', 1);
        return closingQuote == std::wstring::npos
            ? std::wstring{}
            : value.substr(1, closingQuote - 1);
    }

    const std::size_t separator = value.find_first_of(L" \t");
    return value.substr(0, separator == std::wstring::npos ? value.size() : separator);
}

std::filesystem::path absolutePath(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : absolute.lexically_normal();
}

bool samePath(const std::filesystem::path& left, const std::filesystem::path& right) {
    const std::wstring leftValue = absolutePath(left).wstring();
    const std::wstring rightValue = absolutePath(right).wstring();
    return CompareStringOrdinal(leftValue.c_str(), -1, rightValue.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool readRegisteredCommand(std::wstring& command) {
    HKEY key = nullptr;
    const LSTATUS openResult = RegOpenKeyExW(
        HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key);
    if (openResult != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD byteCount = 0;
    LSTATUS result = RegQueryValueExW(
        key, kValueName, nullptr, &type, nullptr, &byteCount);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || byteCount == 0) {
        RegCloseKey(key);
        return false;
    }

    std::vector<wchar_t> buffer((byteCount + sizeof(wchar_t) - 1) / sizeof(wchar_t));
    result = RegQueryValueExW(
        key, kValueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &byteCount);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS)
        return false;

    if (!buffer.empty() && buffer.back() == L'\0')
        buffer.pop_back();
    command.assign(buffer.begin(), buffer.end());
    return true;
}

void setError(std::wstring* errorMessage, LSTATUS error) {
    if (!errorMessage)
        return;

    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(
        flags, nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length != 0 && buffer != nullptr) {
        *errorMessage = trim(std::wstring(buffer, length));
        LocalFree(buffer);
    } else {
        *errorMessage = L"Windows 错误代码: " + std::to_wstring(error);
    }
}

} // namespace

bool isRegisteredFor(const std::filesystem::path& executablePath) {
    std::wstring command;
    if (!readRegisteredCommand(command))
        return false;

    const std::wstring registeredPath = executableFromCommand(command);
    return !registeredPath.empty() && samePath(registeredPath, executablePath);
}

bool setRegistered(const std::filesystem::path& executablePath,
                   bool enabled,
                   std::wstring* errorMessage) {
    if (errorMessage)
        errorMessage->clear();

    if (!enabled) {
        HKEY key = nullptr;
        const LSTATUS openResult = RegOpenKeyExW(
            HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key);
        if (openResult == ERROR_FILE_NOT_FOUND)
            return true;
        if (openResult != ERROR_SUCCESS) {
            setError(errorMessage, openResult);
            return false;
        }

        const LSTATUS deleteResult = RegDeleteValueW(key, kValueName);
        RegCloseKey(key);
        if (deleteResult == ERROR_FILE_NOT_FOUND)
            return true;
        if (deleteResult != ERROR_SUCCESS) {
            setError(errorMessage, deleteResult);
            return false;
        }
        return true;
    }

    const auto normalizedPath = absolutePath(executablePath);
    std::error_code pathError;
    if (normalizedPath.empty() ||
        !std::filesystem::is_regular_file(normalizedPath, pathError)) {
        if (errorMessage)
            *errorMessage = L"开机启动程序不存在：" + normalizedPath.wstring();
        return false;
    }

    HKEY key = nullptr;
    DWORD disposition = 0;
    const LSTATUS createResult = RegCreateKeyExW(
        HKEY_CURRENT_USER, kRunKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE, nullptr, &key, &disposition);
    if (createResult != ERROR_SUCCESS) {
        setError(errorMessage, createResult);
        return false;
    }

    const std::wstring command = L"\"" + normalizedPath.wstring() + L"\"";
    if (command.size() >= 260) {
        if (errorMessage)
            *errorMessage = L"开机启动路径过长（Windows Run 注册项限制为 260 个字符）。";
        return false;
    }
    const DWORD byteCount = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    const LSTATUS setResult = RegSetValueExW(
        key, kValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()), byteCount);
    RegCloseKey(key);
    if (setResult != ERROR_SUCCESS) {
        setError(errorMessage, setResult);
        return false;
    }
    return true;
}

} // namespace startup_registration
