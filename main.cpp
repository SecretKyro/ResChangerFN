// Credit AI for the comments.
#include <windows.h>      // Windows API functions and types
#include <iostream>       // Input and output stream objects
#include <tlhelp32.h>     // Toolhelp library for process snapshots
#include <fstream>        // File stream handling
#include "json.hpp"       // JSON library for handling JSON objects

using json = nlohmann::json; // Alias for the JSON library namespace

// Function to log messages to the console
void Log(std::string message) {
    std::cout << message << std::endl;
}

// Function to check if a process with a given name is currently running
bool IsProcessRunning(const std::wstring& processName) {
    bool found = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // Take a snapshot of all running processes

    // Check if snapshot creation was successful
    if (snapshot == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, L"Failed to create snapshot of processes.", L"Process Running Error!", MB_ICONERROR | MB_OK);
        return false;
    }

    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32); // Initialize the structure size

    // Retrieve information about the first process in the snapshot
    if (Process32First(snapshot, &processEntry)) {
        do {
            // Compare the process name with the given process name
            if (processName == processEntry.szExeFile) {
                found = true;
                break; // Process found, exit loop
            }
        } while (Process32Next(snapshot, &processEntry)); // Move to the next process
    }
    else {
        MessageBox(NULL, L"Failed to get the first process entry.", L"Process Running Error!", MB_ICONERROR | MB_OK);
    }

    CloseHandle(snapshot); // Close the snapshot handle
    return found; // Return whether the process was found
}

// Function to change the screen resolution to specified width and height
bool ChangeResolution(int width, int height) {
    DEVMODE devMode;
    ZeroMemory(&devMode, sizeof(devMode)); // Initialize the DEVMODE structure
    devMode.dmSize = sizeof(devMode); // Set the size of the structure
    devMode.dmPelsWidth = width; // Set the desired width
    devMode.dmPelsHeight = height; // Set the desired height
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT; // Specify which fields are being set

    LONG result = ChangeDisplaySettings(&devMode, CDS_TEST); // Test if the settings are valid

    if (result == DISP_CHANGE_SUCCESSFUL) {
        // Apply the resolution change if settings are valid
        result = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
        if (result == DISP_CHANGE_SUCCESSFUL) {
            std::cout << "Resolution is now " << width << "x" << height << std::endl;
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

// Main function
int main() {
    int newWidth = 0; // Variable to store new screen width
    int newHeight = 0; // Variable to store new screen height
    std::string userInput; // Variable to store user input
    std::wstring fortniteProcessName = L"FortniteClient-Win64-Shipping.exe"; // Process name to monitor

    // Path to the JSON file containing resolution settings
    std::ifstream file("C:\\Resolution.json");

    // Check if the JSON file can be opened
    if (file.is_open()) {
        json j;
        Log("Attempting to load JSON from 'C:\\Resolution.json'");
        try {
            file >> j; // Read JSON data from file
            newWidth = j.at("Width").get<int>(); // Get width from JSON
            newHeight = j.at("Height").get<int>(); // Get height from JSON
            userInput = j.at("Input").get<std::string>(); // Get user input from JSON
            Log("Loaded JSON from 'C:\\Resolution.json'");
        }
        catch (json::parse_error& e) {
            MessageBox(NULL, L"JSON Parse Error", L"JSON Error!", MB_ICONERROR | MB_OK);
            return 1;
        }
        catch (json::out_of_range& e) {
            MessageBox(NULL, L"JSON Key Error", L"JSON Error!", MB_ICONERROR | MB_OK);
            return 1;
        }
    }
    else {
        // Prompt the user for resolution settings if JSON file does not exist
        Log("Welcome, what width do you want while playing Fortnite?");
        std::cin >> newWidth; // Get width from user input
        Log("Welcome, what height do you want while playing Fortnite?");
        std::cin >> newHeight; // Get height from user input
        Log(std::to_string(newWidth) + "x" + std::to_string(newHeight));
        Log("Is that the correct resolution Y/N?");
        std::cin >> userInput; // Get confirmation from user

        if (userInput == "Y") {
            json j;
            j["Width"] = newWidth; // Store width in JSON
            j["Height"] = newHeight; // Store height in JSON
            j["Input"] = userInput; // Store user input in JSON

            std::ofstream file("C:\\Resolution.json");

            if (!file.is_open()) {
                MessageBox(NULL, L"Cannot open file to write to.", L"JSON Error!", MB_ICONERROR | MB_OK);
                return 1;
            }

            file << j.dump(4); // Write JSON data to file with indentation
        }
    }

    // If user confirmed the settings
    if (userInput == "Y") {
        while (true) {
            int currentScreenWidth = GetSystemMetrics(SM_CXSCREEN); // Get current screen width
            int currentScreenHeight = GetSystemMetrics(SM_CYSCREEN); // Get current screen height
            if (IsProcessRunning(fortniteProcessName)) { // Check if Fortnite process is running
                if (currentScreenWidth != newWidth || currentScreenHeight != newHeight) { // Check if resolution needs to be changed
                    ChangeResolution(newWidth, newHeight); // Change to the desired resolution
                }
            }
            else if (currentScreenWidth != 1920 || currentScreenHeight != 1080) { // Check if resolution needs to be reverted
                ChangeResolution(1920, 1080); // Revert to default resolution
            }
            else {
                Sleep(5000); // Wait for 5 seconds before checking again
            }
        }
    }
    else {
        MessageBox(NULL, L"Invalid input. Restart the program and try again.", L"Error!", MB_ICONERROR | MB_OK);
        return 1;
    }

    return 0; // Exit program successfully
}
