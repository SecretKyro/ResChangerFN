#include <windows.h>
#include <iostream>
#include <tlhelp32.h>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

void Log(const std::string& message) {
    std::cout << message << std::endl;
}

void GetCurrentResolution() {
    int currentScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int currentScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    Sleep(5000);
}

bool IsProcessRunning(const std::wstring& processName) {
    bool found = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, L"Failed to create snapshot of processes.", L"Process Running Error!", MB_ICONERROR | MB_OK);
        return false;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (processName == processEntry.szExeFile) {
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &processEntry));
    }
    else {
        MessageBox(NULL, L"Failed to get the first process entry.", L"Process Running Error!", MB_ICONERROR | MB_OK);
    }

    CloseHandle(snapshot);
    return found;
}

bool ChangeResolution(int width, int height) {
    DEVMODE devMode;
    ZeroMemory(&devMode, sizeof(devMode));
    devMode.dmSize = sizeof(devMode);
    devMode.dmPelsWidth = width;
    devMode.dmPelsHeight = height;
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    LONG result = ChangeDisplaySettings(&devMode, CDS_TEST);

    if (result == DISP_CHANGE_SUCCESSFUL) {
        result = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
        if (result == DISP_CHANGE_SUCCESSFUL) {
            return true;
        }
        else {
            MessageBox(NULL, L"Failed to change resolution.", L"Resolution Error!", MB_ICONERROR | MB_OK);
            return false;
        }
    }
    else {
        MessageBox(NULL, L"Resolution settings are not supported.", L"Resolution Error!", MB_ICONERROR | MB_OK);
        return false;
    }
}

int main() {
    int newWidth = 0;
    int newHeight = 0;
    std::string userInput;
    std::wstring fortniteProcessName = L"FortniteClient-Win64-Shipping.exe";

    // Path to the JSON file
    std::ifstream file("C:\\c");

    if (file.is_open()) {
        // Parse the JSON fileRestart the program and try again.
        json j;
        Log("Attempting to load your Json at 'C://Resolution.json'");
        try {
            file >> j;
            newWidth = j.at("Width").get<int>();
            newHeight = j.at("Height").get<int>();
            userInput = j.at("Input").get<std::string>();
            Log("Loaded your Json at 'C://Resolution.json'");
        }
        catch (json::parse_error& e) {
            MessageBox(NULL, L"Json Parse Error", L"Json Error!", MB_ICONERROR | MB_OK);
            return 1;
        }
        catch (json::out_of_range& e) {
            MessageBox(NULL, L"Json Key Error", L"Json Error!", MB_ICONERROR | MB_OK);
            return 1;
        }
    }
    else {
        Log("Welcome, what width do you want while playing Fortnite?");
        std::cin >> newWidth;
        Log("Welcome, what height do you want while playing Fortnite?");
        std::cin >> newHeight;
        Log(std::to_string(newWidth) + "x" + std::to_string(newHeight));
        Log("Is that the correct resolution Y/N?");
        std::cin >> userInput;

        if (userInput == "Y") {
            // Create a JSON object
            json j;
            j["Width"] = newWidth;
            j["Height"] = newHeight;
            j["Input"] = userInput;

            // Path to the JSON file
            std::ofstream file("C:\\Resolution.json");

            if (!file.is_open()) {
                MessageBox(NULL, L"Can't opening file to write to.", L"Json Error!", MB_ICONERROR | MB_OK);
                return 1;
            }

            file << j.dump(4);
        }
    }

    if (userInput == "Y") {
        while (true) {
            int currentScreenWidth = GetSystemMetrics(SM_CXSCREEN);
            int currentScreenHeight = GetSystemMetrics(SM_CYSCREEN);
            if (IsProcessRunning(fortniteProcessName)) {
                if (currentScreenWidth != newWidth || currentScreenHeight != newHeight) {
                    ChangeResolution(newWidth, newHeight);
                    std::cout << "Resolution is now " << newWidth << "x" << newHeight << std::endl;
                }
            }
            else {
                ChangeResolution(1920, 1080);
                std::cout << "Resolution is now 1920x1080" << std::endl;
            }
            Sleep(5000); // Wait for 5 seconds before checking again
        }
    }
    else {
        MessageBox(NULL, L"Restart the program and try again.", L"Error!", MB_ICONERROR | MB_OK);
        return 1;
    }

    return 0;
}
