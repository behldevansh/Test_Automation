#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <string>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>

using namespace std;

// 🔁 Recursively scan directory for matching gold files
void scanDirectory(const string& basePath, const unordered_set<string>& goldFiles, unordered_map<string, string>& fileMap) {
    DIR* dir = opendir(basePath.c_str());
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name == "." || name == "..") continue;

        string fullPath = basePath + "/" + name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) == -1) continue;

        if (S_ISDIR(st.st_mode)) {
            scanDirectory(fullPath, goldFiles, fileMap);  // recursive
        } else if (S_ISREG(st.st_mode)) {
            if (goldFiles.count(name)) {
                fileMap[name] = fullPath;
            }
        }
    }
    closedir(dir);
}

// 📁 Get all file names from golds/
unordered_set<string> getGoldFilenames(const string& goldsPath) {
    unordered_set<string> goldFiles;
    DIR* dir = opendir(goldsPath.c_str());
    if (!dir) {
        cerr << "❌ Could not open golds folder!" << endl;
        return goldFiles;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name == "." || name == "..") continue;
        goldFiles.insert(name);
    }

    closedir(dir);
    return goldFiles;
}

// ✏️ Fix the Makefile to set SYSRTEMP = .
bool patchMakefile(const string& makefilePath) {
    ifstream in(makefilePath);
    stringstream buffer;
    string line;
    bool modified = false;
    regex sysrtemp_pattern(R"(SYSRTEMP\s*=)");

    while (getline(in, line)) {
        if (regex_search(line, sysrtemp_pattern)) {
            buffer << "SYSRTEMP = ." << endl;
            modified = true;
        } else {
            buffer << line << endl;
        }
    }
    in.close();

    if (modified) {
        ofstream out(makefilePath);
        out << buffer.str();
    }

    return modified;
}

// ✏️ Patch nc.tcl — fix existing and add missing file copy -force commands
bool patchNcTcl(const string& ncTclPath, const unordered_map<string, string>& fileMap) {
    ifstream in(ncTclPath);
    stringstream buffer;
    string line;

    regex copy_pattern(R"(file copy -force\s+(.*?)\s+(.*))");

    unordered_set<string> alreadyHandled;
    bool modified = false;

    cout << "\n🔍 [DEBUG] Scanning nc.tcl..." << endl;

    while (getline(in, line)) {
        smatch match;
        if (regex_match(line, match, copy_pattern)) {
            string source = match[1].str();
            string destination = match[2].str();

            // Extract filename
            size_t lastSlash = source.find_last_of("/");
            string filename = (lastSlash != string::npos) ? source.substr(lastSlash + 1) : source;

            if (fileMap.count(filename)) {
                alreadyHandled.insert(filename);

                if (destination != "../") {
                    cout << "[DEBUG] ➤ Fixing destination for: " << filename << endl;
                    buffer << "file copy -force " << source << " ../" << endl;
                    modified = true;
                } else {
                    buffer << line << endl;
                }
            } else {
                buffer << line << endl; // not in golds
            }
        } else {
            buffer << line << endl; // not a copy line
        }
    }
    in.close();

    // ➕ Append missing copy commands
    for (const auto& [filename, fullpath] : fileMap) {
        if (!alreadyHandled.count(filename)) {
            cout << "[DEBUG] ➕ Adding missing file copy for: " << filename << endl;
            buffer << "file copy -force ../" << fullpath << " ../" << endl;
            modified = true;
        }
    }

    if (modified) {
        ofstream out(ncTclPath);
        out << buffer.str();
    } else {
        cout << "[DEBUG] 🎉 nc.tcl is already up to date." << endl;
    }

    return modified;
}

int main() {
    string goldsPath = "golds";
    string makefilePath = "Makefile";
    string ncTclPath = "scripts/nc.tcl";

    // Step 1: Load gold file names
    unordered_set<string> goldFiles = getGoldFilenames(goldsPath);
    if (goldFiles.empty()) {
        cerr << "❌ No files found in golds folder." << endl;
        return 1;
    }

    // Step 2: Read SYSRTEMP path from Makefile
    ifstream makefile(makefilePath);
    string sysrtempPath;
    regex sysrtemp_regex(R"(SYSRTEMP\s*=\s*(.+))");
    string line;
    while (getline(makefile, line)) {
        smatch match;
        if (regex_match(line, match, sysrtemp_regex)) {
            sysrtempPath = match[1].str();
            break;
        }
    }
    makefile.close();

    if (sysrtempPath.empty()) {
        cerr << "❌ SYSRTEMP not found in Makefile." << endl;
        return 1;
    }

    // Step 3: Find matching files in SYSRTEMP folder
    unordered_map<string, string> fileMap;
    scanDirectory(sysrtempPath, goldFiles, fileMap);
    if (fileMap.empty()) {
        cerr << "⚠️ No matching files found in results directory." << endl;
    }

    // Step 4: Patch files
    bool changedNcTcl = patchNcTcl(ncTclPath, fileMap);
    bool changedMakefile = patchMakefile(makefilePath);

    // Step 5: Report
    if (!changedNcTcl && !changedMakefile) {
        cout << "\n✅ Everything already perfect. No changes made." << endl;
    } else {
        cout << "\n✅ Refactoring complete." << endl;
        if (changedNcTcl) cout << "  → nc.tcl updated." << endl;
        if (changedMakefile) cout << "  → Makefile updated." << endl;
    }

    return 0;
}
