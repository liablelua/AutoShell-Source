#include <windows.h>
#include <string>
#include <iostream>
#include <tchar.h>
#include <urlmon.h>
#include <fstream>
#include <sstream>
#include <string>
#include <Psapi.h>
#include <cstdio>
#include <shlobj.h>
#include <filesystem>
#pragma comment(lib, "Urlmon.lib")

bool RegisterFileAssociation(const std::wstring& extension, const std::wstring& fileType, const std::wstring& description, const std::wstring& appPath, const std::wstring& iconPath) {
    HKEY hKey;
    LONG lResult;
    DWORD dwDisposition;
    lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, extension.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (lResult != ERROR_SUCCESS) {
        std::wcerr << L"Failed to create/open registry key for the extension. Error code: " << lResult << std::endl;
        return false;
    }
    lResult = RegSetValueEx(hKey, NULL, 0, REG_SZ, (const BYTE*)fileType.c_str(), (fileType.size() + 1) * sizeof(wchar_t));
    if (lResult != ERROR_SUCCESS) {
        std::wcerr << L"Failed to set registry value for the file type. Error code: " << lResult << std::endl;
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);
    lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, fileType.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (lResult != ERROR_SUCCESS) {
        std::wcerr << L"Failed to create/open registry key for the file type. Error code: " << lResult << std::endl;
        return false;
    }
    lResult = RegSetValueEx(hKey, NULL, 0, REG_SZ, (const BYTE*)description.c_str(), (description.size() + 1) * sizeof(wchar_t));
    if (lResult != ERROR_SUCCESS) {
        std::wcerr << L"Failed to set registry value for the description. Error code: " << lResult << std::endl;
        RegCloseKey(hKey);
        return false;
    }
    std::wstring commandKey = fileType + L"\\shell\\open\\command";
    lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, commandKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition);
    if (lResult != ERROR_SUCCESS) {
        std::wcerr << L"Failed to create/open registry key for the command. Error code: " << lResult << std::endl;
        return false;
    }
    std::wstring command = L"\"" + appPath + L"\" \"%1\"";
    lResult = RegSetValueEx(hKey, NULL, 0, REG_SZ, (const BYTE*)command.c_str(), (command.size() + 1) * sizeof(wchar_t));
    if (lResult != ERROR_SUCCESS) {
        std::wcerr << L"Failed to set registry value for the command. Error code: " << lResult << std::endl;
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);
    if (!iconPath.empty()) {
        std::wstring iconKey = fileType + L"\\DefaultIcon";
        lResult = RegCreateKeyEx(HKEY_CLASSES_ROOT, iconKey.c_str(), 0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDisposition);
        if (lResult != ERROR_SUCCESS) {
            std::wcerr << L"Failed to create/open registry key for the icon. Error code: " << lResult << std::endl;
            return false;
        }

        lResult = RegSetValueEx(hKey, NULL, 0, REG_SZ, (const BYTE*)iconPath.c_str(), (iconPath.size() + 1) * sizeof(wchar_t));
        if (lResult != ERROR_SUCCESS) {
            std::wcerr << L"Failed to set registry value for the icon. Error code: " << lResult << std::endl;
            RegCloseKey(hKey);
            return false;
        }
        RegCloseKey(hKey);
    }

    std::wcout << L"File association registered successfully!" << std::endl;
    return true;
}

std::wstring GetExecutablePath() {
    wchar_t buffer[MAX_PATH];
    DWORD length = GetModuleFileName(NULL, buffer, MAX_PATH);
    if (length == 0) {
        std::cerr << "Failed to get executable path. Error code: " << GetLastError() << std::endl;
        return NULL;
    }
    return std::wstring(buffer, length);
}

std::wstring string_to_wstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring TrimToLastBackslash(const std::wstring& path) {
    size_t lastSlashPos = path.rfind(L'\\');
    if (lastSlashPos != std::wstring::npos) {
        return path.substr(0, lastSlashPos + 1);
    }
    return path;
}

void downloadFile(const std::string& url, const std::string& path) {
    std::wstring sURL = string_to_wstring(url);
    std::wstring dFile = string_to_wstring(path);

    HRESULT hr = URLDownloadToFile(NULL, sURL.c_str(), dFile.c_str(), 0, NULL);
    if (S_OK != hr) {
        std::cerr << "An error occured. Couldn't check for an update." << std::endl;
    }
}

bool IsFileTypeRegistered(const std::wstring& fileType) {
    HKEY hKey;
    LONG lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, fileType.c_str(), 0, KEY_READ, &hKey);
    if (lResult == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

std::string stringFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file " << filePath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}

std::string wstring_to_string(const std::wstring& wstr) {
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        return "";
    }
    std::string str(bufferSize - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], bufferSize, nullptr, nullptr);
    return str;
}

bool CloseApplication(const std::string& exeName) {
    DWORD processes[1024], processCount;
    if (!EnumProcesses(processes, sizeof(processes), &processCount)) {
        std::cerr << "Failed to enumerate processes." << std::endl;
        return false;
    }
    processCount /= sizeof(DWORD);
    for (DWORD i = 0; i < processCount; ++i) {
        if (processes[i] == 0) continue;
        TCHAR processName[MAX_PATH] = TEXT("<unknown>");
        HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processes[i]);
        if (processHandle) {
            HMODULE hMod;
            DWORD cbNeeded;
            if (EnumProcessModules(processHandle, &hMod, sizeof(hMod), &cbNeeded)) {
                GetModuleBaseName(processHandle, hMod, processName, sizeof(processName) / sizeof(TCHAR));
            }
            CloseHandle(processHandle);
            std::wstring wsProcessName(processName);
            std::string strProcessName(wsProcessName.begin(), wsProcessName.end());
            if (strProcessName == exeName) {
                HANDLE terminateHandle = OpenProcess(PROCESS_TERMINATE, FALSE, processes[i]);
                if (terminateHandle) {
                    if (TerminateProcess(terminateHandle, 0)) {
                        std::cout << "Terminated process: " << exeName << std::endl;
                    }
                    else {
                        std::cerr << "Failed to terminate process: " << exeName << std::endl;
                    }
                    CloseHandle(terminateHandle);
                }
                else {
                    std::cerr << "Failed to open process for termination: " << exeName << std::endl;
                }
                return true;
            }
        }
    }
    std::cerr << "Application not found." << std::endl;
    return false;
}

char* wstring_to_char(const std::wstring& wstr) {
    int bufferSize = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        throw std::runtime_error("Error in WideCharToMultiByte: " + std::to_string(GetLastError()));
    }
    char* charBuffer = new char[bufferSize];
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, charBuffer, bufferSize, nullptr, nullptr);
    return charBuffer;
}

bool DeleteFileFromPath(const std::wstring& filePath) {
    Sleep(1000);
    if (DeleteFile(filePath.c_str())) {
        std::wcout << L"File deleted successfully: " << filePath << std::endl;
        return true;
    }
    else {
        DWORD errorCode = GetLastError();
        if (errorCode == ERROR_ACCESS_DENIED || errorCode == ERROR_SHARING_VIOLATION) {
            std::wcout << L"File is in use. Waiting before attempting to delete: " << filePath << std::endl;
            Sleep(5000);
            if (DeleteFile(filePath.c_str())) {
                std::wcout << L"File deleted successfully after waiting: " << filePath << std::endl;
                return true;
            }
            else {
                std::wcerr << "Failed to delete file after waiting: " << filePath << " Error: " << GetLastError() << std::endl;
                return false;
            }
        }
        else {
            std::wcerr << "Failed to delete file: " << filePath << " Error: " << GetLastError() << std::endl;
            return false;
        }
    }
}

bool StartProcessAndWait(const std::wstring& applicationPath) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcess(
        NULL,
        const_cast<LPWSTR>(applicationPath.c_str()),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        NULL,
        &si,
        &pi
    )) {
        std::wcerr << L"CreateProcess failed. Error code: " << GetLastError() << std::endl;
        return false;
    }
    DWORD waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        std::wcerr << L"Failed to wait for process to exit. Error code: " << GetLastError() << std::endl;
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}

int main() {
    std::wstring extension = L".ass";
    std::wstring fileType = L"ASS.info";
    std::wstring description = L"A runnable script-type for AutoShell.";
    std::wstring appPath = TrimToLastBackslash(GetExecutablePath()) + L"AutoShell.exe";
    std::wstring iconPath = TrimToLastBackslash(GetExecutablePath()) + L"shell.ico";

    if (!IsFileTypeRegistered(fileType)) {
        if (!RegisterFileAssociation(extension, fileType, description, appPath, iconPath)) {
            int result = MessageBox(NULL, TEXT(".ass is required for AutoShell Script files to run, click YES to open the Updater in administrator so the program can update registry keys."), TEXT("Updater"), MB_YESNO | MB_ICONINFORMATION);
            if (result == IDYES) {
                ShellExecute(NULL, _T("runas"), GetExecutablePath().c_str(), NULL, NULL, SW_RESTORE);
                exit(0);
            }
        }
    }

    std::wstring ws(TrimToLastBackslash(GetExecutablePath()) + L"version.int");
    std::wstring sw(TrimToLastBackslash(GetExecutablePath()) + L"app.int");
    std::string path(ws.begin(), ws.end());
    std::string path2(sw.begin(), sw.end());

    std::string currentVersion = stringFile(path2);
    downloadFile("https://raw.githubusercontent.com/liablelua/AutoShell/main/version.int", path);
    std::string githubVersion = stringFile(path);

    if (currentVersion != githubVersion) {
        int newResult = MessageBox(NULL, TEXT("An update is available! Download now?"), TEXT("Updater"), MB_YESNO | MB_ICONINFORMATION);
        if (newResult == IDYES) {
            if (CloseApplication("AutoShell.exe")) {
                std::cout << "Updating!" << std::endl;
                if (DeleteFileFromPath(appPath)) {
                    std::cout << "Downloading new AutoShell.exe" << std::endl;
                    downloadFile("https://raw.githubusercontent.com/liablelua/AutoShell/main/dl/AutoShell.exe", wstring_to_string(appPath));
                    std::cout << "Downloaded new AutoShell.exe" << std::endl << "Downloading required DLL's" << std::endl;
                    std::cout << "Found Dependencies" << std::endl;
                    std::wstring dllpathinteger = TrimToLastBackslash(GetExecutablePath()) + L"dlls.txt";
                    downloadFile("https://raw.githubusercontent.com/liablelua/AutoShell/main/dll.int", wstring_to_string(dllpathinteger));
                    std::ifstream file("dlls.txt");
                    std::string line;
                    if (file.is_open()) { 
                        while (std::getline(file, line)) {
                            if (DeleteFileFromPath(TrimToLastBackslash(GetExecutablePath()) + string_to_wstring(line))) {
                                std::cout << "Deleted " << line << std::endl;
                            }
                            std::cout << "Downloading " << line << std::endl;
                            downloadFile("https://raw.githubusercontent.com/liablelua/AutoShell/main/dl/" + line, wstring_to_string(TrimToLastBackslash(GetExecutablePath())) + line);
                            std::cout << "Downloading " << line << std::endl;
                        }
                        file.close();
                    }
                    else { 
                        std::cerr << "Unable to open file :(" << std::endl;
                        exit(0);
                    }
                    std::cout << "Downloaded! Updating the Current Version on the Hard-drive." << std::endl;
                    downloadFile("https://raw.githubusercontent.com/liablelua/AutoShell/main/version.int", wstring_to_string(TrimToLastBackslash(GetExecutablePath())) + "app.int");
                    std::cout << "Cleaning up." << std::endl;
                    DeleteFileFromPath(TrimToLastBackslash(GetExecutablePath()) + string_to_wstring("dlls.txt"));
                    ShellExecute(NULL, _T("open"), appPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
                } 
                else {
                    std::cerr << "Failed to delete the old executable file." << std::endl;
                }
            }
            else {
                std::cerr << "Failed to close process." << std::endl;
            }
        }
        else {
            exit(0);
        }
    }

    DeleteFileFromPath(TrimToLastBackslash(GetExecutablePath()) + string_to_wstring("version.int"));

    exit(0);

    return 0;
}
