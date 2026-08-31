#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <shellapi.h>
#include <wininet.h>

#include <string>

namespace {

#if defined(_M_ARM64)
constexpr wchar_t kRedistributableUrl[] = L"https://aka.ms/vc14/vc_redist.arm64.exe";
#elif defined(_M_IX86)
constexpr wchar_t kRedistributableUrl[] = L"https://aka.ms/vc14/vc_redist.x86.exe";
#else
constexpr wchar_t kRedistributableUrl[] = L"https://aka.ms/vc14/vc_redist.x64.exe";
#endif

constexpr wchar_t kCoreExecutable[] = L"bin\\SpineWallpaperCore.exe";
constexpr wchar_t kBootstrapUserAgent[] = L"SpineWallpaper/1.0";

std::wstring modulePath() {
    std::wstring result;
    result.resize(32768);
    const DWORD length = GetModuleFileNameW(nullptr, result.data(),
                                             static_cast<DWORD>(result.size()));
    if (length == 0 || length >= result.size())
        return {};
    result.resize(length);
    return result;
}

std::wstring directoryOf(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::wstring joinPath(const std::wstring& directory, const wchar_t* name) {
    if (directory.empty())
        return name;
    if (directory.back() == L'\\' || directory.back() == L'/')
        return directory + name;
    return directory + L"\\" + name;
}

bool regularFileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool runtimeAvailable(const std::wstring& applicationDirectory) {
    wchar_t systemDirectory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return false;

    const std::wstring systemPath(systemDirectory, length);
    constexpr const wchar_t* requiredDlls[] = {
        L"msvcp140.dll",
        L"vcruntime140.dll",
        L"vcruntime140_1.dll",
    };

    for (const auto* dll : requiredDlls) {
        const auto appLocal = joinPath(applicationDirectory, dll);
        const auto systemLocal = joinPath(systemPath, dll);
        if (!regularFileExists(appLocal) && !regularFileExists(systemLocal))
            return false;
    }
    return true;
}

bool makeTemporaryInstallerPath(std::wstring& path) {
    wchar_t temporaryDirectory[MAX_PATH]{};
    const DWORD directoryLength = GetTempPathW(MAX_PATH, temporaryDirectory);
    if (directoryLength == 0 || directoryLength >= MAX_PATH)
        return false;

    wchar_t temporaryFile[MAX_PATH]{};
    if (GetTempFileNameW(temporaryDirectory, L"spw", 0, temporaryFile) == 0)
        return false;
    DeleteFileW(temporaryFile);

    path = temporaryFile;
    const auto extension = path.find_last_of(L'.');
    if (extension != std::wstring::npos)
        path.erase(extension);
    path += L".exe";
    return true;
}

bool downloadRedistributable(const std::wstring& destination) {
    HINTERNET internet = InternetOpenW(
        kBootstrapUserAgent,
        INTERNET_OPEN_TYPE_PRECONFIG,
        nullptr,
        nullptr,
        0);
    if (!internet)
        return false;

    DWORD timeout = 30000;
    InternetSetOptionW(internet, INTERNET_OPTION_CONNECT_TIMEOUT,
                       &timeout, sizeof(timeout));
    InternetSetOptionW(internet, INTERNET_OPTION_RECEIVE_TIMEOUT,
                       &timeout, sizeof(timeout));

    HINTERNET download = InternetOpenUrlW(
        internet,
        kRedistributableUrl,
        nullptr,
        0,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD |
            INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES,
        0);
    if (!download) {
        InternetCloseHandle(internet);
        return false;
    }

    HANDLE output = CreateFileW(
        destination.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY,
        nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(download);
        InternetCloseHandle(internet);
        return false;
    }

    char buffer[64 * 1024];
    bool success = true;
    DWORD bytesRead = 0;
    do {
        if (!InternetReadFile(download, buffer, sizeof(buffer), &bytesRead)) {
            success = false;
            break;
        }
        if (bytesRead == 0)
            break;

        DWORD bytesWritten = 0;
        if (!WriteFile(output, buffer, bytesRead, &bytesWritten, nullptr) ||
            bytesWritten != bytesRead) {
            success = false;
            break;
        }
    } while (true);

    CloseHandle(output);
    InternetCloseHandle(download);
    InternetCloseHandle(internet);

    LARGE_INTEGER size{};
    HANDLE verify = success ? CreateFileW(destination.c_str(), GENERIC_READ,
                                           FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                           FILE_ATTRIBUTE_NORMAL, nullptr)
                            : INVALID_HANDLE_VALUE;
    if (verify != INVALID_HANDLE_VALUE) {
        success = GetFileSizeEx(verify, &size) && size.QuadPart >= 1024 * 1024;
        CloseHandle(verify);
    } else {
        success = false;
    }

    if (!success)
        DeleteFileW(destination.c_str());
    return success;
}

bool runRedistributable(const std::wstring& installer) {
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS;
    execute.lpVerb = L"runas";
    execute.lpFile = installer.c_str();
    execute.lpParameters = L"/install /quiet /norestart";
    execute.nShow = SW_HIDE;

    if (!ShellExecuteExW(&execute))
        return false;

    WaitForSingleObject(execute.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(execute.hProcess, &exitCode);
    CloseHandle(execute.hProcess);
    return exitCode == ERROR_SUCCESS || exitCode == ERROR_SUCCESS_REBOOT_REQUIRED;
}

void showError(const wchar_t* message) {
    MessageBoxW(nullptr, message, L"Spine 壁纸播放器", MB_OK | MB_ICONERROR);
}

bool launchCore(const std::wstring& core) {
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.lpFile = core.c_str();
    execute.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&execute) != FALSE;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const auto self = modulePath();
    if (self.empty()) {
        showError(L"无法确定启动器所在目录。");
        return 1;
    }

    const auto applicationDirectory = directoryOf(self);
    const auto core = joinPath(applicationDirectory, kCoreExecutable);
    if (!regularFileExists(core)) {
        showError(L"找不到 bin\\SpineWallpaperCore.exe，请确认文件没有被删除或单独移动。");
        return 1;
    }

    if (!runtimeAvailable(applicationDirectory)) {
        MessageBoxW(
            nullptr,
            L"检测到缺少对应的 Microsoft Visual C++ Redistributable。\n"
            L"程序将从微软官方地址下载并启动安装，安装过程可能会弹出 UAC 权限确认。",
            L"Spine 壁纸播放器",
            MB_OK | MB_ICONINFORMATION);

        std::wstring installer;
        if (!makeTemporaryInstallerPath(installer) ||
            !downloadRedistributable(installer)) {
            showError(L"下载 Visual C++ Redistributable 失败，请检查网络连接后重试。");
            return 1;
        }

        const bool installed = runRedistributable(installer);
        DeleteFileW(installer.c_str());
        if (!installed || !runtimeAvailable(applicationDirectory)) {
            showError(L"Visual C++ Redistributable 安装失败、被取消，或安装后仍无法找到运行库。");
            return 1;
        }
    }

    if (!launchCore(core)) {
        showError(L"启动 bin\\SpineWallpaperCore.exe 失败。");
        return 1;
    }
    return 0;
}
