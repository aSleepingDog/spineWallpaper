#pragma once

#include <filesystem>
#include <string>

namespace startup_registration {

bool isRegisteredFor(const std::filesystem::path& executablePath);
bool setRegistered(const std::filesystem::path& executablePath,
                   bool enabled,
                   std::wstring* errorMessage = nullptr);

} // namespace startup_registration
