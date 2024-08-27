#include <windows.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <urlmon.h>
#include <zip.h>
#include <vector>
#include <sstream>

#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Shell32.lib")

namespace fs = std::filesystem;
using namespace std;

char Path[255] = "";
string Command = "";
bool Outputs = false;
bool CantRun = false;

vector<string> split(const string& s, const string& delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    string token;
    vector<string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
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

void run_bat(string fp) {
    ifstream inputFile(fp);

    if (!inputFile.is_open()) {
        cerr << "[AutoShell BATCH Env] Error: Could not open the file." << std::endl;
    }

    string line;
    while (getline(inputFile, line)) {
        system(line.c_str());
    }

    inputFile.close();
}

LPCWSTR string_to_lpcwstr(const std::string& str) {
    int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(wideCharCount, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], wideCharCount);
    return wstr.c_str();
}

std::wstring string_to_wstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* out = static_cast<std::ofstream*>(userp);
    size_t totalSize = size * nmemb;
    out->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

template <typename T>
std::string to_string(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

void downloadFile(const std::string& url, const std::string& path) {
    std::wstring sURL = string_to_wstring(url);
    std::wstring dFile = string_to_wstring(path);

    HRESULT hr = URLDownloadToFile(NULL, sURL.c_str(), dFile.c_str(), 0, NULL);
    if (S_OK == hr) {
        std::cout << "The file is saved to " << path << std::endl;
    }
    else {
        std::cout << "Unable to download the file. Error code: " << hr << std::endl;
    }
}

void extractZip(const char* zipfilename, const char* extract_dir) {
    int err = 0;
    zip* archive = zip_open(zipfilename, ZIP_RDONLY, &err);
    if (!archive) {
        std::cerr << "Failed to open zip archive: " << zipfilename << std::endl;
        return;
    }

    zip_int64_t num_entries = zip_get_num_entries(archive, 0);
    for (zip_int64_t i = 0; i < num_entries; ++i) {
        struct zip_stat st;
        zip_stat_init(&st);

        const char* name = zip_get_name(archive, i, 0);
        if (zip_stat(archive, name, 0, &st) != 0) {
            std::cerr << "Failed to stat file in zip: " << name << std::endl;
            continue;
        }

        if (std::string(name).find("__MACOSX/") != std::string::npos) {
            continue;
        }

        std::string proper_name(name);
        fs::path entry_name(proper_name);
        cout << entry_name << endl;
        cout << extract_dir << endl;
        fs::path full_path = fs::path(extract_dir) / entry_name;

        std::cout << "Processing: " << full_path.string() << std::endl;

        if (entry_name.filename().empty() && entry_name.extension().empty()) {
            create_directories(full_path);
            continue;
        }

        if (full_path.has_parent_path()) {
            create_directories(full_path.parent_path());
        }

        char* contents = new char[st.size];
        zip_file* file = zip_fopen(archive, name, 0);
        if (!file) {
            std::cerr << "Failed to open file in zip: " << name << std::endl;
            delete[] contents;
            continue;
        }

        zip_fread(file, contents, st.size);
        zip_fclose(file);

        std::ofstream fout(full_path, std::ios::binary);
        if (fout) {
            fout.write(contents, st.size);
            fout.close();
        }
        else {
            std::cerr << "Failed to create file: " << full_path.string() << std::endl;
        }

        delete[] contents;
    }

    zip_close(archive);
}

void run(vector<string> Cmd) {
    bool commandUsed = false;

    if (Cmd[0] == "echo") {
        commandUsed = true;
        string echo;

        for (int i = 1; i < Cmd.size(); i++) {
            if (i == 1) {
                echo = Cmd[i];
            }
            else {
                echo += " " + Cmd[i];
            }
        }

        cout << echo << endl;
    }

    if (Cmd[0] == "cd") {
        commandUsed = true;

        if (Cmd.size() > 1) {
            fs::path newPath = Cmd[1];

            if (!newPath.is_absolute()) {
                newPath = fs::path(Path) / newPath;
            }

            try {
                newPath = fs::canonical(newPath);
            }
            catch (fs::filesystem_error& err) {
                cout << "[ERR]: " << err.code() << endl;
            }

            if (fs::exists(newPath) && fs::is_directory(newPath)) {
                fs::current_path(newPath);
                strncpy_s(Path, newPath.string().c_str(), sizeof(Path) - 1);
                Path[sizeof(Path) - 1] = '\0';
                cout << "Changed directory to: " << Path << endl;
            }
            else {
                cout << "Invalid path or directory does not exist." << endl;
            }
        }
        else {
            cout << "Current directory: " << Path << endl;
        }
    }

    if (Cmd[0] == "mkdir") {
        commandUsed = true;

        if (Cmd.size() > 1) {
            fs::path newDirPath = Cmd[1];

            if (!newDirPath.is_absolute()) {
                newDirPath = fs::path(Path) / newDirPath;
            }

            try {
                newDirPath = fs::canonical(newDirPath.parent_path()) / newDirPath.filename();
            }
            catch (const fs::filesystem_error& e) {
                cout << "Error resolving path: " << e.what() << endl;
            }

            if (fs::exists(newDirPath)) {
                if (fs::is_directory(newDirPath)) {
                    cout << "Directory already exists: " << newDirPath.string() << endl;
                }
                else {
                    cout << "A file with the same name already exists: " << newDirPath.string() << endl;
                }
            }
            else {
                try {
                    fs::create_directory(newDirPath);
                    cout << "Created directory: " << newDirPath.string() << endl;
                }
                catch (const fs::filesystem_error& e) {
                    cout << "Error creating directory: " << e.what() << endl;
                }
            }
        }
        else {
            cout << "Usage: mkdir <directory_path>" << endl;
        }
    }

    if (Cmd[0] == "dir") {
        commandUsed = true;
        fs::path listPath = Path;
        fs::current_path(Path);

        cout << endl;

        if (Cmd.size() > 1) {
            if (!listPath.is_absolute()) {
                listPath = fs::path(Path) / listPath;
            }
        }

        try {
            if (fs::exists(listPath) && fs::is_directory(listPath)) {
                for (const auto& entry : fs::directory_iterator(listPath)) {
                    if (fs::is_directory(entry.status())) {
                        cout << entry.path().filename().string() << endl;
                    }
                }
            }
            else {
                cout << "Path does not exist or is not a directory." << endl;
            }
        }
        catch (const fs::filesystem_error& e) {
            cout << "Filesystem error: " << e.what() << endl;
        }
    }

    if (Cmd[0] == "title") {
        commandUsed = true;

        if (Cmd.size() > 1) {
            wstring title;

            for (int i = 1; i < Cmd.size(); i++) {
                if (i == 1) {
                    title = wstring(Cmd[i].begin(), Cmd[i].end());
                }
                else {
                    title += L" " + wstring(Cmd[i].begin(), Cmd[i].end());
                }
            }

            SetConsoleTitleW(title.c_str());

            cout << "Set Console Title: " << wstring_to_string(title) << endl;
        }
        else {
            SetConsoleTitleW(L"AutoShell CLI");

            cout << "Reset Title" << endl;
        }
    }

    if (Cmd[0] == "start") {
        commandUsed = true;

        if (Cmd.size() > 1) {
            fs::path filePath = Cmd[1];

            if (!filePath.is_absolute()) {
                filePath = fs::path(Path) / filePath;
            }

            if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
                if (filePath.filename().extension() == ".ass") {
                    ifstream inputFile(filePath.string());

                    if (!inputFile.is_open()) {
                        cerr << "[AutoShell Env] Error: Could not open the file." << std::endl;
                    }

                    string line;
                    while (getline(inputFile, line)) {
                        if (line != "\\n" && line != " ") { 
                            cout << line << endl;
                            vector<string> Cmd = split(line, " ");
                            if (!CantRun) { run(Cmd); }
                            if (Cmd[0] == "]]") {
                                CantRun = false;
                            }
                        }
                    }

                    inputFile.close();
                }
                else {
                    if (filePath.filename().extension() == ".bat") {
                        run_bat(filePath.string());
                    }
                    else {
                        string command = "start \"\" \"" + filePath.string() + "\"";
                        system(command.c_str());
                    }
                }
            }
            else {
                cout << "File does not exist or is not a regular file: " << filePath << endl;
            }
        }
        else {
            cout << "Usage: start <filename>" << endl;
        }
    }
   
    if (Cmd[0] == "webdl") {
        commandUsed = true;
        if (Cmd.size() == 3) {
            std::string url = Cmd[1];
            fs::path filePath = Cmd[2];

            if (!filePath.is_absolute()) {
                filePath = fs::path(Path) / filePath;
            }

            string path = filePath.string();

            downloadFile(url, path);
        }
        else {
            std::cout << "Usage: webdl <URL> <path>" << std::endl;
        }
    }

    if (Cmd[0] == "@echo") {
        commandUsed = true;
        if (Cmd[1] == "off") {
            cout.setstate(std::ios_base::failbit);
        }
        else {
            if (Cmd[1] == "on") {
                cout.clear();
                cout.flush();
            }
            else {
                cout << "Usage: @echo <on/off>" << endl;
            }
        }
    }

    if (Cmd[0] == "unzip") {
        commandUsed = true;
        if (Cmd.size() == 3) {
            const char* zip = Cmd[1].c_str();
            fs::path filePath = Cmd[2];

            if (!filePath.is_absolute()) {
                filePath = fs::path(Path) / filePath;
            }

            std::string pathStr = filePath.string();
            const char* path = pathStr.c_str();

            cout << path << endl;

            extractZip(zip, path);
        }
        else {
            cout << "Usage: unzip <zip file> <extract folder>" << endl;
        }
    }

    if (Cmd[0] == "exit") {
        exit(0);
    }

    if (Cmd[0] == "--") {
        commandUsed = true;
    }

    if (Cmd[0] == "--[[") {
        commandUsed = true;
        CantRun = true;
    }

    if (Cmd[0] == "]]") {
        commandUsed = true;
        CantRun = false;
    }

    if (Cmd[0] == "" || Cmd[0] == " " || Cmd[0] == "\\n") {
        commandUsed = true;
    }
    
    if (!commandUsed) {
        cout << "Command " << Cmd[0] << " doesn't exist." << endl;
    }
}

void Loop() {
    while (true) {
        Command = "";
        getline(cin, Command);
        vector<string> Cmd = split(Command, " ");
        if (!CantRun) { run(Cmd); }
        if (Cmd[0] == "]]") {
            CantRun = false;
        }
        cout << endl;
    }
}

void run_as(string fp) {
    ifstream inputFile(fp);

    if (!inputFile.is_open()) {
        cerr << "[AutoShell Env] Error: Could not open the file." << std::endl;
    }

    string line;
    while (getline(inputFile, line)) {
        if (line != "\\n" && line != " " && line != "") {
            vector<string> Cmd = split(line, " ");
            if (!CantRun) { run(Cmd); }
            if (Cmd[0] == "]]") {
                CantRun = false;
            }
        }
    }

    inputFile.close();
}

int main(int argc, char* argv[]) {
    SetConsoleTitleW(L"AutoShell CLI");
    if (argc > 1) {
        string filePath = argv[1];
        if (filePath.find(".ass")) {
            cout << "[AutoShell Env] Running: " << filePath << std::endl;
            cout << endl;
            run_as(filePath);
            cout << endl;
            getline(cin, Command);
        }
        else {
            if (filePath.find(".bat")) {
                cout << "[AutoShell BATCH Env] Running: " << filePath << std::endl;
                cout << endl;
                run_bat(filePath);
                cout << endl;
                getline(cin, Command);
            }
        }
    }
    else {
        _fullpath(Path, argv[0], sizeof(Path));
        string strPath = Path;
        size_t pos = strPath.find("AutoShell.exe");
        if (pos != string::npos) {
            strPath = strPath.substr(0, pos);
        }
        strncpy_s(Path, strPath.c_str(), sizeof(Path));
        Path[sizeof(Path) - 1] = '\0';
        string ThePath = Path;
        ThePath = ThePath + "Updater.exe";
        ShellExecute(NULL, _T("open"), string_to_wstring(ThePath).c_str(), NULL, NULL, SW_SHOWNORMAL);
        cout << "AutoShell v1.1.0\n\n";
        Loop();
    }
}