#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <regex>
#include <dirent.h>
#include <sys/stat.h>

// ✅ Recursively scan all folders under results
void scanDirectoryRecursive(const std::string &basePath, std::map<std::string,std::string> &filePaths) {
    DIR *dir = opendir(basePath.c_str());
    if (!dir) {
        std::cerr << "[DEBUG] Cannot open directory: " << basePath << "\n";
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string fullPath = basePath + "/" + name;
        struct stat pathStat;
        if (stat(fullPath.c_str(), &pathStat) == 0) {
            if (S_ISDIR(pathStat.st_mode)) {
                scanDirectoryRecursive(fullPath, filePaths);
            } else if (S_ISREG(pathStat.st_mode)) {
                if (filePaths.find(name) == filePaths.end()) {
                    std::string rel = fullPath;
                    for (auto &c : rel) if (c == '\\') c = '/';
                    if (rel.rfind("results/", 0) == 0) {
                        rel = "../" + rel.substr(8); // strip "results/"
                    } else {
                        rel = "../" + rel;
                    }
                    filePaths[name] = rel;
                }
            }
        }
    }
    closedir(dir);
}

// ✅ Read filenames from golds folder
std::set<std::string> getGoldFiles(const std::string &goldPath) {
    std::set<std::string> goldFiles;
    DIR *dir = opendir(goldPath.c_str());
    if (!dir) {
        std::cerr << "[DEBUG] Cannot open golds folder: " << goldPath << "\n";
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

// ✅ Update Makefile to SYSRTEMP = .
void updateMakefile(const std::string &makefilePath) {
    std::ifstream in(makefilePath);
    if (!in.is_open()) {
        std::cerr << "[DEBUG] Cannot open Makefile for update.\n";
        return;
    }
    std::vector<std::string> lines;
    std::string line;
    std::regex re("^\\s*SYSRTEMP\\s*=");
    while (std::getline(in, line)) {
        if (std::regex_search(line, re)) {
            std::cout << "[DEBUG] Updating SYSRTEMP line: " << line << " -> SYSRTEMP = .\n";
            lines.push_back("SYSRTEMP = .");
        } else {
            lines.push_back(line);
        }
    }
    in.close();
    std::ofstream out(makefilePath, std::ios::trunc);
    for (auto &l : lines) out << l << "\n";
}

// ✅ Process nc.tcl
void processNcTcl(const std::string &ncPath,
                  const std::set<std::string> &goldFiles,
                  const std::map<std::string,std::string> &resultPaths) {
    std::ifstream in(ncPath);
    if (!in.is_open()) {
        std::cerr << "[DEBUG] Cannot open nc.tcl at " << ncPath << "\n";
        return;
    }

    std::vector<std::string> lines;
    std::set<std::string> alreadyCopied;
    std::string line;

    std::regex reCopy(R"(file copy -force\s+([^\s]+)\s+([^\s]+))");
    std::regex reAnyCommandWithBraces(R"(^(\S[^\{]*\{)([^}]*)\})");
    std::regex reDumpArg(R"(-dump\s+(\.\.[^}\s]+))");

    while (std::getline(in, line)) {
        std::smatch m;
        if (std::regex_search(line, m, reCopy)) {
            std::string src = m[1].str();
            size_t pos = src.find_last_of("/\\");
            std::string filename = (pos == std::string::npos) ? src : src.substr(pos + 1);
            alreadyCopied.insert(filename);
        }
        lines.push_back(line);
    }
    in.close();

    std::cout << "[DEBUG] Gold files count: " << goldFiles.size() << "\n";
    std::cout << "[DEBUG] Files found in results: " << resultPaths.size() << "\n";

    std::cout << "[DEBUG] Golds that will be checked:\n";
    for (const auto &gold : goldFiles) {
        if (resultPaths.count(gold)) {
            std::cout << "  ✅ Found in results: " << gold << " -> " << resultPaths.at(gold);
            if (alreadyCopied.count(gold)) {
                std::cout << " (already copied)\n";
            } else {
                std::cout << " (will append)\n";
            }
        } else {
            std::cout << "  ❌ NOT found in results: " << gold << "\n";
        }
    }

    // append missing file copy
    for (const auto &gold : goldFiles) {
        if (resultPaths.count(gold) && !alreadyCopied.count(gold)) {
            std::string fullPath = resultPaths.at(gold);
            std::cout << "[DEBUG] Appending: " << fullPath << "\n";
            lines.push_back("catch { file copy -force " + fullPath + " ../ }");
        }
    }

    std::ofstream out(ncPath, std::ios::trunc);
    for (auto &l : lines) out << l << "\n";
    std::cout << "[DEBUG] nc.tcl write complete.\n";
}

int main() {
    std::string goldPath = "golds";
    std::string ncPath = "scripts/nc.tcl";
    std::string makefilePath = "Makefile";

    std::cout << "[DEBUG] Scanning golds folder...\n";
    auto goldFiles = getGoldFiles(goldPath);

    std::cout << "[DEBUG] Scanning results folder recursively...\n";
    std::map<std::string,std::string> resultPaths;
    scanDirectoryRecursive("results", resultPaths);

    processNcTcl(ncPath, goldFiles, resultPaths);

    updateMakefile(makefilePath);

    std::cout << "✅ Done. Check output above.\n";
    return 0;
}
