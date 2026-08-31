# SpineWallpaper

## 项目介绍

SpineWallpaper 是一个基于 C++、Win32、SFML 和 Spine Runtime 的 Windows 动态壁纸播放器。
## 当前项目状态

当前主要功能包括：

- 系统托盘图标，以及“设置”和“退出”菜单；
- 按配置文件加载指定的 `.json`、`.skel` 或 `.skel.bytes` Spine 文件；
- 循环播放配置文件指定的动画；
- 将 SFML 窗口挂载到桌面 `WorkerW`，作为动态壁纸显示；
- 主显示器尺寸检测和窗口尺寸更新；
- PMA 渲染和纹理 RGB 预乘；
- 最大帧率限制，可设置为按显示器；
- 可设置其他软件最大化或全屏时无操作、暂停播放或停止播放，退出全屏后自动恢复；
- 可在设置中开启开机自动启动（默认关闭）；

## 目录结构

```text
.
├─ spine-cpp/                    # Spine C++ 3.8 Runtime
│  ├─ include/
│  └─ src/
├─ spine-sfml/                   # Spine SFML 适配层
│  └─ spine/
├─ dependencies/                # CMake 自动下载的 SFML 依赖目录，自动忽略
│  └─ SFML-2.4.1/
│     ├─ include/
│     └─ lib/
├─ src/                          # 项目 C++ 源代码
│  ├─ launcher.cpp               # 启动器和 VC++ Runtime 检查
│  ├─ main.cpp                   # 核心程序入口和系统托盘
│  ├─ startup_registration.cpp   # Windows 用户级开机启动注册
│  ├─ renderer.cpp               # Spine 壁纸渲染逻辑
│  ├─ renderer.h
│  ├─ texture_loader.cpp         # 纹理加载
│  ├─ texture_loader.h
├─ build/                        # CMake 构建目录，自动忽略
├─ dist/                         # CMake 编译输出，自动忽略
├─ release/                      # 最终分发目录，自动忽略
│  ├─ SpineWallpaper.exe         # 启动器
│  ├─ bin/
│  │  └─ SpineWallpaperCore.exe  # 壁纸核心程序
│  └─ SpineWallpaper.ini.example # 配置示例
├─ cmake/
│  └─ package-release.cmake      # Release 版本打包脚本
├─ CMakeLists.txt
├─ .gitignore
├─ LICENSE
└─ README.md
```

## 构建环境

- Windows 10/11 x64。项目使用 Win32 API，目前不支持在 Linux 或 macOS 上直接构建；
- CMake 3.20 或更高版本；
- 能被 CMake 识别的 Windows C++17 工具链，以及 Windows SDK。
- 注意：仓库默认使用 `dependencies/SFML-2.4.1` 下的预编译静态 `*.lib`，当前版本按 MSVC ABI 提供。因此，使用仓库默认依赖时应选择匹配的 MSVC 工具链。若默认依赖尚未准备好，或使用其他 SFML 版本/工具链，请通过 `SFML_ROOT` 指定匹配的 `include/` 和 `lib/` 目录，并根据需要调整静态库和运行时设置。

## 使用 CMake 构建和打包

项目不绑定 Visual Studio 生成器。下面的命令同时适用于单配置生成器（如 Ninja、MinGW Makefiles）和多配置生成器（如 Visual Studio）；在本仓库根目录执行：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target SpineWallpaperPackage --parallel
```

其中，单配置生成器使用 `CMAKE_BUILD_TYPE`，多配置生成器使用 `--config Release`；两者同时保留可以让这组命令适配更多 CMake 生成器。若 CMake 没有自动选中所需工具链，可以在配置时显式指定生成器，例如：

首次配置时，如果默认的 `dependencies/SFML-2.4.1` 目录不存在，CMake 会下载并解压SFML 2.4.1；目录已存在时不会下载。可以使用 `-DSPINEWALLPAPER_DOWNLOAD_SFML=OFF` 禁用自动下载，也可以通过 `-DSFML_ROOT=<path>` 使用已经准备好的其他 SFML 安装。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target SpineWallpaperPackage --parallel
```

如果使用 MSVC，也可以在安装了 Visual Studio Build Tools 和 Windows SDK 的开发者命令提示符中执行上述通用命令；不需要打开 Visual Studio IDE。

构建 `SpineWallpaperPackage` 完成后，CMake 会自动完成打包：

- CMake 构建目录位于 `build/`；
- 编译输出位于 `dist/<配置>/`；
- 最终分发目录位于本仓库根目录 `release/`，至少包含：
  `SpineWallpaper.exe`、`bin/SpineWallpaperCore.exe` 和  `SpineWallpaper.ini.example`。

如需构建 Debug 版本，可执行：

```powershell
cmake --build build --config Debug --parallel
```

## VC++ Runtime 处理

`SpineWallpaper.exe` 是一个不依赖 MSVC 动态运行库的启动器。它会：

1. 检测当前架构所需的 `MSVCP140.dll`、`VCRUNTIME140.dll` 和
   `VCRUNTIME140_1.dll`；
2. 如果缺失，从微软官方 v14 Redistributable 永久链接下载对应安装包；
3. 通过 UAC 启动静默安装；
4. 安装成功后启动 `bin/SpineWallpaperCore.exe`。

因此，首次运行缺少运行库的电脑需要网络连接，并且用户需要同意 UAC 权限请求。网络不可用或用户拒绝安装时，程序会提示错误并退出。

官方说明：[Latest Supported Visual C++ Redistributable Downloads](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)

## 使用说明

将同一 Spine 资源组的骨骼文件、atlas 和纹理放在任意位置，并保持对应的基础文件名。例如：

```text
MySpine/
├─ character.json
├─ character.atlas
└─ character.png
```

在启动器旁边创建 `SpineWallpaper.ini`，内容示例：

```ini
spine_file=D:\Wallpapers\MySpine\character.json
animation=idle
force_premultiplied_channel=true
force_premultiplied_alpha=true
```

相对路径以 `SpineWallpaper.ini` 所在目录为基准。播放器不会播放缺少 atlas、无法解析或没有指定动画的 Spine 文件。设置窗口中的最大帧率选择为“符合显示器”时，会自动读取主显示器当前刷新率；不选择时默认仍为 30 FPS。

设置中的“其他软件最大化/全屏时”默认是“无操作”。选择“暂停播放”会保留当前壁纸画面，暂停动画更新；选择“停止播放”会隐藏桌面渲染窗口并释放当前播放资源。检测到前台窗口退出最大化或全屏后，播放器会自动恢复。

### 开机自动启动

设置窗口中的“开机自动启动”默认关闭。开启后，程序会将启动器注册到当前用户的`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，不需要管理员权限，也不会安装服务或增加常驻进程。

注册项保存的是当前程序的绝对路径。程序整体移动后，首次手动启动新位置的程序时，会根据已保存的设置自动更新注册路径；

## 第三方组件和授权

- Spine C++ 3.8 运行时：使用前和再分发时必须遵守 Spine Runtimes
  License Agreement 及 Spine Editor License Agreement；相关说明位于
  `spine-cpp/LICENSE.txt` 和 `spine-sfml/LICENSE.txt`。
- SFML 2.4.1：遵守 SFML 的 zlib/png 等相关许可证，许可证文件位于
  `dependencies/SFML-2.4.1/license.md`。
- 本项目原创代码：MIT，见 `LICENSE`。
