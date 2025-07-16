//Working Till File Force Copy nc and makefile both

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <regex>
#include <dirent.h>
#include <sys/stat.h>

// ✅ recursively scan directories
void scanDirectoryRecursive(const std::string &basePath, std::set<std::string> &files) {
    DIR *dir = opendir(basePath.c_str());
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = basePath + "/" + name;

        struct stat pathStat;
        if (stat(fullPath.c_str(), &pathStat) == 0) {
            if (S_ISDIR(pathStat.st_mode)) {
                // recurse
                scanDirectoryRecursive(fullPath, files);
            } else if (S_ISREG(pathStat.st_mode)) {
                files.insert(name); // just the filename
            }
        }
    }
    closedir(dir);
}

// ✅ read all filenames from golds (no recursion needed)
std::set<std::string> getGoldFiles(const std::string &goldPath) {
    std::set<std::string> goldFiles;
    DIR *dir = opendir(goldPath.c_str());
    if (!dir) {
        std::cerr << "Error: golds folder not found at " << goldPath << "\n";
        return goldFiles;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = goldPath + "/" + name;
        struct stat pathStat;
        if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISREG(pathStat.st_mode)) {
            goldFiles.insert(name);
        }
    }
    closedir(dir);
    return goldFiles;
}

// ✅ get SYSRTEMP from Makefile
std::string getSYSRTEMP(const std::string &makefilePath) {
    std::ifstream in(makefilePath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open Makefile at " << makefilePath << "\n";
        return {};
    }
    std::string line;
    std::regex re("^\\s*SYSRTEMP\\s*=\\s*(.*)$");
    std::smatch m;
    std::string sysrtemp;
    while (std::getline(in, line)) {
        if (std::regex_search(line, m, re)) {
            sysrtemp = m[1].str();
            break;
        }
    }
    return sysrtemp;
}

// ✅ update Makefile
void updateMakefile(const std::string &makefilePath) {
    std::ifstream in(makefilePath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open Makefile for update.\n";
        return;
    }
    std::vector<std::string> lines;
    std::string line;
    std::regex re("^\\s*SYSRTEMP\\s*=");
    while (std::getline(in, line)) {
        if (std::regex_search(line, re)) {
            lines.push_back("SYSRTEMP = .");
        } else {
            lines.push_back(line);
        }
    }
    in.close();

    std::ofstream out(makefilePath, std::ios::trunc);
    for (auto &l : lines) out << l << "\n";
}

// ✅ process nc.tcl
void processNcTcl(const std::string &ncPath,
                  const std::set<std::string> &goldFiles,
                  const std::set<std::string> &resultFiles) {
    std::ifstream in(ncPath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open nc.tcl at " << ncPath << "\n";
        return;
    }

    std::vector<std::string> lines;
    std::string line;
    std::regex re(R"(file copy -force\s+([^\s]+)\s+([^\s]+))");

    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            std::string src = m[1].str();
            std::string dest = m[2].str();

            // extract filename from src manually
            size_t pos = src.find_last_of("/\\");
            std::string filename = (pos == std::string::npos) ? src : src.substr(pos + 1);

            if (goldFiles.count(filename) && resultFiles.count(filename)) {
                std::string newLine = "file copy -force " + src + " ../";
                lines.push_back(newLine);
                continue;
            }
        }
        lines.push_back(line);
    }
    in.close();

    std::ofstream out(ncPath, std::ios::trunc);
    for (auto &l : lines) out << l << "\n";
}

int main() {
    std::string goldPath = "golds";
    std::string makefilePath = "Makefile";
    std::string ncPath = "scripts/nc.tcl";

    // step1: gold files
    std::set<std::string> goldFiles = getGoldFiles(goldPath);
    std::cout << "Gold files found: " << goldFiles.size() << "\n";

    // step2: read SYSRTEMP
    std::string sysrtemp = getSYSRTEMP(makefilePath);
    if (sysrtemp.empty()) {
        std::cerr << "Error: Could not find SYSRTEMP in Makefile.\n";
        return 1;
    }

    // prepend results/
    std::string sysrtempPath = "results/" + sysrtemp;

    // step3: recursively scan results/SYSRTEMP
    std::set<std::string> resultFiles;
    scanDirectoryRecursive(sysrtempPath, resultFiles);
    std::cout << "Result files found: " << resultFiles.size() << "\n";

    // step4: process nc.tcl
    processNcTcl(ncPath, goldFiles, resultFiles);

    // step5: update Makefile
    updateMakefile(makefilePath);

    std::cout << "✅ Done. nc.tcl and Makefile updated.\n";
    return 0;
}
