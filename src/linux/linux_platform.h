#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace linux_platform {

// main() stores the command line; Configure() reads it like CommandLineToArgvW.
void SetArguments(int argc, char** argv);
std::vector<std::wstring> Arguments();

std::filesystem::path ExecutablePath();
// $XDG_DATA_HOME, or ~/.local/share
std::filesystem::path DataHome();

// A modal message box. Falls back to stderr when no display is available.
void ShowError(const char* title, const std::string& text);
void OpenFolder(const std::filesystem::path& folder);

// Use the portal or native helpers; return false on cancellation or an unavailable chooser.
bool ChooseFile(std::filesystem::path& selected, bool folder, const std::string& title);

}
