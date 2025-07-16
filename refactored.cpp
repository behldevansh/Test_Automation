#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <regex>
#include <dirent.h>
#include <sys/stat.h>

// ✅ Recursively scan a directory and collect full paths
void scanDirectoryRecursive(const std::string &basePath, std::map<std::string,std::string> &filePaths) {
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
                scanDirectoryRecursive(fullPath, filePaths);
            } else if (S_ISREG(pathStat.st_mode)) {
                // only store first occurrence
                if (filePaths.find(name) == filePaths.end()) {
                    // store path relative to project, prefix "../" like original
                    std::string relativePath = fullPath;
                    // ensure relativePath uses forward slashes
                    for (auto &c : relativePath) if (c == '\\') c = '/';
                    // if fullPath is like "results/output/ref_5g/mtbf_analysis/xyz.csv"
                    // we want "../output/ref_5g/mtbf_analysis/xyz.csv"
                    if (relativePath.rfind("results/", 0) == 0) {
                        relativePath = "../" + relativePath.substr(8);
                    }
                    filePaths[name] = relativePath;
                }
            }
        }
    }
    closedir(dir);
}

// ✅ Read filenames from golds
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

// ✅ Parse SYSRTEMP from Makefile
std::string getSYSRTEMP(const std::string &makefilePath) {
    std::ifstream in(makefilePath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open Makefile.\n";
        return {};
    }
    std::string line;
    std::regex re("^\\s*SYSRTEMP\\s*=\\s*(.*)$");
    std::smatch m;
    while (std::getline(in, line)) {
        if (std::regex_search(line, m, re)) {
            return m[1].str();
        }
    }
    return {};
}

// ✅ Update Makefile
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

// ✅ Process nc.tcl
void processNcTcl(const std::string &ncPath,
                  const std::set<std::string> &goldFiles,
                  const std::map<std::string,std::string> &resultPaths) {
    std::ifstream in(ncPath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open nc.tcl.\n";
        return;
    }

    std::vector<std::string> lines;
    std::set<std::string> alreadyCopied; // track which golds are handled
    std::string line;

    std::regex reCopy(R"(file copy -force\s+([^\s]+)\s+([^\s]+))");
    std::regex reAnyCommandWithBraces(R"(^(\S[^\{]*\{)([^}]*)\})");
    std::regex reDumpArg(R"(-dump\s+(\.\.[^}\s]+))");

    while (std::getline(in, line)) {
        std::smatch m;

        // ---- Handle file copy lines ----
        if (std::regex_search(line, m, reCopy)) {
            std::string src = m[1].str();
            size_t pos = src.find_last_of("/\\");
            std::string filename = (pos == std::string::npos) ? src : src.substr(pos + 1);
            if (goldFiles.count(filename) && resultPaths.count(filename)) {
                alreadyCopied.insert(filename);
                std::string newLine = "catch { file copy -force " + src + " ../ }";
                lines.push_back(newLine);
                continue;
            }
        }

        // ---- Handle universal dump lines ----
        if (std::regex_search(line, m, reAnyCommandWithBraces)) {
            std::string beforeBrace = m[1].str();
            std::string inside = m[2].str();
            std::smatch mdump;
            if (std::regex_search(inside, mdump, reDumpArg)) {
                std::string dumpPath = mdump[1].str();
                size_t pos = dumpPath.find_last_of("/\\");
                std::string dumpFile = (pos == std::string::npos) ? dumpPath : dumpPath.substr(pos + 1);
                std::string newInside = std::regex_replace(
                    inside,
                    reDumpArg,
                    std::string("-dump ../") + dumpFile
                );
                std::string newLine = beforeBrace + newInside + "}";
                lines.push_back(newLine);
                continue;
            }
        }

        lines.push_back(line);
    }
    in.close();

    // ---- Append missing file copy commands ----
    for (const auto &gold : goldFiles) {
        if (resultPaths.count(gold) && !alreadyCopied.count(gold)) {
            std::string fullPath = resultPaths.at(gold);
            lines.push_back("catch { file copy -force " + fullPath + " ../ }");
        }
    }

    std::ofstream out(ncPath, std::ios::trunc);
    for (auto &l : lines) out << l << "\n";
}

int main() {
    std::string goldPath = "golds";
    std::string makefilePath = "Makefile";
    std::string ncPath = "scripts/nc.tcl";

    auto goldFiles = getGoldFiles(goldPath);
    std::cout << "Gold files found: " << goldFiles.size() << "\n";

    std::string sysrtemp = getSYSRTEMP(makefilePath);
    if (sysrtemp.empty()) {
        std::cerr << "Error: No SYSRTEMP found.\n";
        return 1;
    }

    std::string sysrtempPath = "results/" + sysrtemp;
    std::map<std::string,std::string> resultPaths;
    scanDirectoryRecursive(sysrtempPath, resultPaths);
    std::cout << "Result files found: " << resultPaths.size() << "\n";

    processNcTcl(ncPath, goldFiles, resultPaths);
    updateMakefile(makefilePath);

    std::cout << "✅ Done. nc.tcl updated with missing file copies & Makefile fixed.\n";
    return 0;
}
