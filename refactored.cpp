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
                if (filePaths.find(name) == filePaths.end()) {
                    std::string rel = fullPath;
                    for (auto &c : rel) if (c == '\\') c = '/';
                    // make it relative to project root
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
            if (goldFiles.count(filename) && resultPaths.count(filename)) {
                alreadyCopied.insert(filename);
                lines.push_back("catch { file copy -force " + src + " ../ }");
                continue;
            }
        }
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
                lines.push_back(beforeBrace + newInside + "}");
                continue;
            }
        }
        lines.push_back(line);
    }
    in.close();

    // Append missing file copy
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
    std::string ncPath = "scripts/nc.tcl";

    auto goldFiles = getGoldFiles(goldPath);
    std::cout << "[INFO] Gold files: " << goldFiles.size() << "\n";

    std::map<std::string,std::string> resultPaths;
    scanDirectoryRecursive("results", resultPaths);
    std::cout << "[INFO] Files found in results: " << resultPaths.size() << "\n";

    processNcTcl(ncPath, goldFiles, resultPaths);
    std::cout << "✅ Done. nc.tcl updated (independent of SYSRTEMP).\n";
    return 0;
}
