// Credit ChatGPT for the comment code!
#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <fstream>
#include <locale>
#include <codecvt>
#include <shlwapi.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <csignal>
#include "json.hpp"

// Link the Shlwapi library for certain Windows functions
#pragma comment(lib, "Shlwapi.lib")

// Using the nlohmann JSON library
using json = nlohmann::json;

// Global synchronization variables
std::mutex mtx;
std::condition_variable cv;
std::atomic<bool> exitFlag(false); // Flag to signal the program to exit
int desktopWidth, desktopHeight; // Desktop resolution dimensions

// Function to convert a wide string (wstring) to a regular string
std::string WStringToString(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

// Function to log messages to the console and optionally to a file
void Log(const std::string& message, bool toFile = true) {
    std::lock_guard<std::mutex> lock(mtx); // Ensure thread safety
    std::cout << message << std::endl;
    if (toFile) {
        std::ofstream logFile("log.txt", std::ios_base::app); // Append log messages to a file
        logFile << message << std::endl;
    }
}

// Function to check if a process with a specific name is running
bool IsProcessRunning(const std::wstring& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        Log("Failed to create snapshot of processes.");
        return false;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    bool found = false;
    if (Process32First(snapshot, &processEntry)) {
        do {
            if (processName == processEntry.szExeFile) {
                found = true; // Process found
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }
    else {
        Log("Failed to get the first process entry.");
    }

    CloseHandle(snapshot); // Always clean up handles
    return found;
}

// Function to change the screen resolution
bool ChangeResolution(int width, int height) {
    DEVMODE devMode = { 0 };
    devMode.dmSize = sizeof(devMode);
    devMode.dmPelsWidth = width;
    devMode.dmPelsHeight = height;
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    // Test the resolution change to ensure it's supported
    LONG result = ChangeDisplaySettings(&devMode, CDS_TEST);
    if (result != DISP_CHANGE_SUCCESSFUL) {
        Log("Resolution settings are not supported.");
        return false;
    }

    // Apply the resolution change
    result = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
    if (result != DISP_CHANGE_SUCCESSFUL) {
        Log("Failed to change resolution. Retrying...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Wait and retry
        result = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
        if (result != DISP_CHANGE_SUCCESSFUL) {
            Log("Failed to change resolution after retry.");
            return false;
        }
    }

    Log("Resolution is now " + std::to_string(width) + "x" + std::to_string(height));
    return true;
}

// Function to get the class name of a window given its handle
std::wstring GetWindowClassName(HWND hwnd) {
    wchar_t className[256];
    GetClassName(hwnd, className, 256); // Retrieves the class name of the specified window
    return std::wstring(className);
}

// Function to check if a specific game is active (foreground window matches the game window class name)
bool IsGameActive(const std::wstring& gameClassName) {
    HWND foregroundWindow = GetForegroundWindow(); // Get the current foreground window
    if (!foregroundWindow) return false;

    std::wstring currentClassName = GetWindowClassName(foregroundWindow);
    return gameClassName == currentClassName; // Check if the class name matches the game's class name
}

// Function to set a program to run at startup by modifying the registry
bool SetStartupRegistryEntry(const std::wstring& programName, const std::wstring& executablePath) {
    HKEY hKey;
    // Open the Run key in the registry for the current user
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        Log("Error opening registry key: " + std::to_string(result));
        return false;
    }

    // Set the program name and path in the Run key
    result = RegSetValueExW(hKey, programName.c_str(), 0, REG_SZ, (const BYTE*)executablePath.c_str(), (executablePath.size() + 1) * sizeof(wchar_t));
    if (result != ERROR_SUCCESS) {
        Log("Error setting registry value: " + std::to_string(result));
        RegCloseKey(hKey);
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

// Function to monitor the game process and adjust the screen resolution accordingly
void MonitorGameResolution(const std::wstring& fortniteProcessName, const std::wstring& fortniteWindowClassName, int gameWidth, int gameHeight, int desktopWidth, int desktopHeight) {
    Log("Waiting to detect if game opens.");

    // Get the current screen resolution
    int currentScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int currentScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Start the monitoring loop
    while (true) {
        currentScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        currentScreenHeight = GetSystemMetrics(SM_CYSCREEN);

        // If the game is running and active, change to the game resolution
        if (IsProcessRunning(fortniteProcessName) && IsGameActive(fortniteWindowClassName)) {
            if (currentScreenWidth != gameWidth || currentScreenHeight != gameHeight) {
                Log("Game is now active, changing resolution to: " + std::to_string(gameWidth) + "x" + std::to_string(gameHeight));
                ChangeResolution(gameWidth, gameHeight);
            }
        }
        // If the game is not active, revert to the desktop resolution
        else if (currentScreenWidth != desktopWidth || currentScreenHeight != desktopHeight) {
            Log("Reverting resolution to: " + std::to_string(desktopWidth) + "x" + std::to_string(desktopHeight));
            ChangeResolution(desktopWidth, desktopHeight);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before checking again
    }
}

int main(int argc, char* argv[]) {
    int gameWidth, gameHeight;
    std::string gameResolutionInput;
    std::string desktopResolutionInput;
    std::string onStartUpInput;
    std::wstring fortniteProcessName = L"FortniteClient-Win64-Shipping.exe"; // Fortnite process name
    std::wstring fortniteWindowClassName = L"UnrealWindow"; // Fortnite window class name
    std::string jsonFilePath = "C:\\Resolution.json"; // Default path to the JSON configuration file
    wchar_t exePath[MAX_PATH];

    // Get the path of the current executable
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        Log("Error retrieving executable path.");
        return 1;
    }

    std::wstring executablePath = exePath;
    std::wstring programName = PathFindFileNameW(executablePath.c_str()); // Get the executable name from the path

    // If a JSON file path is provided as a command line argument, use it
    if (argc > 1) {
        jsonFilePath = argv[1];
    }

    // Load the JSON configuration file
    std::ifstream file(jsonFilePath);
    if (file.is_open()) {
        json j;
        Log("Attempting to load JSON from '" + jsonFilePath + "'");
        try {
            file >> j;
            // Extract values from the JSON file
            gameWidth = j.at("GameWidth").get<int>();
            gameHeight = j.at("GameHeight").get<int>();
            desktopWidth = j.at("DesktopWidth").get<int>();
            desktopHeight = j.at("DesktopHeight").get<int>();
            gameResolutionInput = j.at("GameResolutionInput").get<std::string>();
            desktopResolutionInput = j.at("DesktopResolutionInput").get<std::string>();
            Log("Loaded JSON from '" + jsonFilePath + "'");
        }
        catch (json::parse_error& e) {
            Log("JSON Parse Error: " + std::string(e.what()));
            return 1;
        }
        catch (json::out_of_range& e) {
            Log("JSON Key Error: " + std::string(e.what()));
            return 1;
        }
    }
    else {
        // Prompt the user for input if no JSON file is found
        Log("Welcome, what width do you want while playing Fortnite?");
        std::cin >> gameWidth;
        Log("Welcome, what height do you want while playing Fortnite?");
        std::cin >> gameHeight;
        Log(std::to_string(gameWidth) + "x" + std::to_string(gameHeight));
        Log("Is that the correct resolution Y/N?");
        std::cin >> gameResolutionInput;

        Log("Welcome, what width do you want while not playing Fortnite?");
        std::cin >> desktopWidth;
        Log("Welcome, what height do you want while not playing Fortnite?");
        std::cin >> desktopHeight;
        Log(std::to_string(desktopWidth) + "x" + std::to_string(desktopHeight));
        Log("Is that the correct resolution Y/N?");
        std::cin >> desktopResolutionInput;

        Log("Would you like to have the application start when the computer turns on Y/N?");
        std::cin >> onStartUpInput;

        // Set the program to run at startup if the user chooses to
        if (onStartUpInput == "Y") {
            if (SetStartupRegistryEntry(programName, executablePath)) {
                Log("Program added to startup successfully.");
            }
            else {
                Log("Failed to add program to startup.");
            }
        }

        // Save the user's input to a JSON file
        if (gameResolutionInput == "Y" || desktopResolutionInput == "Y") {
            json j;
            j["GameWidth"] = gameWidth;
            j["GameHeight"] = gameHeight;
            j["DesktopWidth"] = desktopWidth;
            j["DesktopHeight"] = desktopHeight;
            j["GameResolutionInput"] = gameResolutionInput;
            j["DesktopResolutionInput"] = desktopResolutionInput;

            std::ofstream outFile(jsonFilePath);
            if (!outFile.is_open()) {
                Log("Cannot open file to write to.");
                return 1;
            }

            outFile << j.dump(4); // Save the JSON with indentation for readability
            Log("Successfully saved JSON to '" + jsonFilePath + "'");
        }
        else {
            Log("Resolution setup cancelled by user.");
            return 1;
        }
    }

    // Start monitoring the game resolution
    MonitorGameResolution(fortniteProcessName, fortniteWindowClassName, gameWidth, gameHeight, desktopWidth, desktopHeight);

    return 0;
}