// #include <iostream>
// #include <fstream>
// #include <string>
// #include <vector>
// #include <set>
// #include <regex>
// #include <dirent.h>
// #include <sys/stat.h>

// // ✅ Recursively scan a directory and collect filenames
// void scanDirectoryRecursive(const std::string &basePath, std::set<std::string> &files) {
//     DIR *dir = opendir(basePath.c_str());
//     if (!dir) return;

//     struct dirent *entry;
//     while ((entry = readdir(dir)) != nullptr) {
//         std::string name = entry->d_name;
//         if (name == "." || name == "..") continue;

//         std::string fullPath = basePath + "/" + name;

//         struct stat pathStat;
//         if (stat(fullPath.c_str(), &pathStat) == 0) {
//             if (S_ISDIR(pathStat.st_mode)) {
//                 scanDirectoryRecursive(fullPath, files); // recurse
//             } else if (S_ISREG(pathStat.st_mode)) {
//                 files.insert(name); // store filename only
//             }
//         }
//     }
//     closedir(dir);
// }

// // ✅ Read filenames from golds directory
// std::set<std::string> getGoldFiles(const std::string &goldPath) {
//     std::set<std::string> goldFiles;
//     DIR *dir = opendir(goldPath.c_str());
//     if (!dir) {
//         std::cerr << "Error: golds folder not found at " << goldPath << "\n";
//         return goldFiles;
//     }
//     struct dirent *entry;
//     while ((entry = readdir(dir)) != nullptr) {
//         std::string name = entry->d_name;
//         if (name == "." || name == "..") continue;

//         std::string fullPath = goldPath + "/" + name;
//         struct stat pathStat;
//         if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISREG(pathStat.st_mode)) {
//             goldFiles.insert(name);
//         }
//     }
//     closedir(dir);
//     return goldFiles;
// }

// // ✅ Parse SYSRTEMP from Makefile
// std::string getSYSRTEMP(const std::string &makefilePath) {
//     std::ifstream in(makefilePath);
//     if (!in.is_open()) {
//         std::cerr << "Error: Cannot open Makefile at " << makefilePath << "\n";
//         return {};
//     }
//     std::string line;
//     std::regex re("^\\s*SYSRTEMP\\s*=\\s*(.*)$");
//     std::smatch m;
//     std::string sysrtemp;
//     while (std::getline(in, line)) {
//         if (std::regex_search(line, m, re)) {
//             sysrtemp = m[1].str();
//             break;
//         }
//     }
//     return sysrtemp;
// }

// // ✅ Update Makefile to set SYSRTEMP = .
// void updateMakefile(const std::string &makefilePath) {
//     std::ifstream in(makefilePath);
//     if (!in.is_open()) {
//         std::cerr << "Error: Cannot open Makefile for update.\n";
//         return;
//     }
//     std::vector<std::string> lines;
//     std::string line;
//     std::regex re("^\\s*SYSRTEMP\\s*=");
//     while (std::getline(in, line)) {
//         if (std::regex_search(line, re)) {
//             lines.push_back("SYSRTEMP = .");
//         } else {
//             lines.push_back(line);
//         }
//     }
//     in.close();

//     std::ofstream out(makefilePath, std::ios::trunc);
//     for (auto &l : lines) out << l << "\n";
// }

// // ✅ Process nc.tcl
// void processNcTcl(const std::string &ncPath,
//                   const std::set<std::string> &goldFiles,
//                   const std::set<std::string> &resultFiles) {
//     std::ifstream in(ncPath);
//     if (!in.is_open()) {
//         std::cerr << "Error: Cannot open nc.tcl at " << ncPath << "\n";
//         return;
//     }

//     std::vector<std::string> lines;
//     std::string line;

//     std::regex reCopy(R"(file copy -force\s+([^\s]+)\s+([^\s]+))");
//     std::regex reDump(R"(sdaReliability::MTBFPreferences\s*\{([^}]*)\})");
//     std::regex reDumpArg(R"(-dump\s+(\.\.[^}\s]+))");

//     while (std::getline(in, line)) {
//         std::smatch m;

//         // ---- file copy handling ----
//         if (std::regex_search(line, m, reCopy)) {
//             std::string src = m[1].str();
//             size_t pos = src.find_last_of("/\\");
//             std::string filename = (pos == std::string::npos) ? src : src.substr(pos + 1);
//             if (goldFiles.count(filename) && resultFiles.count(filename)) {
//                 std::string newLine = "catch { file copy -force " + src + " ../ }";
//                 lines.push_back(newLine);
//                 continue;
//             }
//         }

//         // ---- dump handling ----
//         if (std::regex_search(line, m, reDump)) {
//             std::string inside = m[1].str();
//             std::smatch mdump;
//             if (std::regex_search(inside, mdump, reDumpArg)) {
//                 std::string dumpPath = mdump[1].str();
//                 size_t pos = dumpPath.find_last_of("/\\");
//                 std::string dumpFile = (pos == std::string::npos) ? dumpPath : dumpPath.substr(pos + 1);

//                 std::string newInside = std::regex_replace(
//                     inside,
//                     reDumpArg,
//                     std::string("-dump ../") + dumpFile
//                 );
//                 std::string newLine = "sdaReliability::MTBFPreferences {" + newInside + "}";
//                 lines.push_back(newLine);
//                 continue;
//             }
//         }

//         // default
//         lines.push_back(line);
//     }
//     in.close();

//     std::ofstream out(ncPath, std::ios::trunc);
//     for (auto &l : lines) out << l << "\n";
// }


// int main() {
//     std::string goldPath = "golds";
//     std::string makefilePath = "Makefile";
//     std::string ncPath = "scripts/nc.tcl";

//     // Step 1: gold files
//     std::set<std::string> goldFiles = getGoldFiles(goldPath);
//     std::cout << "Gold files found: " << goldFiles.size() << "\n";

//     // Step 2: read SYSRTEMP
//     std::string sysrtemp = getSYSRTEMP(makefilePath);
//     if (sysrtemp.empty()) {
//         std::cerr << "Error: Could not find SYSRTEMP in Makefile.\n";
//         return 1;
//     }

//     // Prepend results/
//     std::string sysrtempPath = "results/" + sysrtemp;

//     // Step 3: recursively scan results/SYSRTEMP
//     std::set<std::string> resultFiles;
//     scanDirectoryRecursive(sysrtempPath, resultFiles);
//     std::cout << "Result files found: " << resultFiles.size() << "\n";

//     // Step 4: process nc.tcl
//     processNcTcl(ncPath, goldFiles, resultFiles);

//     // Step 5: update Makefile
//     updateMakefile(makefilePath);

//     std::cout << "✅ Done. nc.tcl and Makefile updated.\n";
//     return 0;
// }

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <regex>
#include <dirent.h>
#include <sys/stat.h>
#include <map>

// ✅ Recursively scan a directory and collect filenames
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
                scanDirectoryRecursive(fullPath, files); // recurse into subfolder
            } else if (S_ISREG(pathStat.st_mode)) {
                files.insert(name); // store filename only
            }
        }
    }
    closedir(dir);
}

// ✅ Read filenames from golds directory
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

// ✅ Update Makefile to set SYSRTEMP = .
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
                  const std::set<std::string> &resultFiles) {
    std::ifstream in(ncPath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open nc.tcl at " << ncPath << "\n";
        return;
    }

    std::vector<std::string> lines;
    std::string line;

    // Regex for file copy
    std::regex reCopy(R"(file copy -force\s+([^\s]+)\s+([^\s]+))");

    // Regex for any command with { ... }
    std::regex reAnyCommandWithBraces(R"(^(\S[^\{]*\{)([^}]*)\})");
    // Regex to match -dump inside braces
    std::regex reDumpArg(R"(-dump\s+(\.\.[^}\s]+))");

    while (std::getline(in, line)) {
        std::smatch m;

        // ---- Handle file copy lines ----
        if (std::regex_search(line, m, reCopy)) {
            std::string src = m[1].str();
            size_t pos = src.find_last_of("/\\");
            std::string filename = (pos == std::string::npos) ? src : src.substr(pos + 1);
            if (goldFiles.count(filename) && resultFiles.count(filename)) {
                std::string newLine = "catch { file copy -force " + src + " ../ }";
                lines.push_back(newLine);
                continue;
            }
        }

        // ---- Handle any command with a -dump argument ----
        if (std::regex_search(line, m, reAnyCommandWithBraces)) {
            std::string beforeBrace = m[1].str(); // includes the '{'
            std::string inside = m[2].str();      // content inside {}
            std::smatch mdump;
            if (std::regex_search(inside, mdump, reDumpArg)) {
                std::string dumpPath = mdump[1].str();
                size_t pos = dumpPath.find_last_of("/\\");
                std::string dumpFile = (pos == std::string::npos) ? dumpPath : dumpPath.substr(pos + 1);
                // Replace dump path with ../filename
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

        // ---- Keep other lines as-is ----
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

    // Step 1: gold files
    std::set<std::string> goldFiles = getGoldFiles(goldPath);
    std::cout << "Gold files found: " << goldFiles.size() << "\n";

    // Step 2: read SYSRTEMP
    std::string sysrtemp = getSYSRTEMP(makefilePath);
    if (sysrtemp.empty()) {
        std::cerr << "Error: Could not find SYSRTEMP in Makefile.\n";
        return 1;
    }

    // Prepend results/
    std::string sysrtempPath = "results/" + sysrtemp;

    // Step 3: recursively scan results/SYSRTEMP
    std::set<std::string> resultFiles;
    scanDirectoryRecursive(sysrtempPath, resultFiles);
    std::cout << "Result files found: " << resultFiles.size() << "\n";

    // Step 4: process nc.tcl
    processNcTcl(ncPath, goldFiles, resultFiles);

    // Step 5: update Makefile
    updateMakefile(makefilePath);

    std::cout << "✅ Done. nc.tcl and Makefile updated.\n";
    return 0;
}
