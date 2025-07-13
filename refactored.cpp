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

// 🔁 Recursively scan directory for matching files
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
            scanDirectory(fullPath, goldFiles, fileMap);  // Recursive call
        } else if (S_ISREG(st.st_mode)) {
            if (goldFiles.count(name)) {
                fileMap[name] = fullPath;
            }
        }
    }

    closedir(dir);
}

// 🔍 Load all filenames from golds/
unordered_set<string> getGoldFilenames(const string& goldsPath) {
    unordered_set<string> goldFiles;
    DIR* dir = opendir(goldsPath.c_str());
    if (!dir) {
        cerr << "❌ Cannot open golds directory!" << endl;
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

// ✏️ Modify Makefile's SYSRTEMP
bool patchMakefile(const string& makefilePath) {
    ifstream in(makefilePath);
    stringstream buffer;
    string line;
    bool modified = false;
    regex sysrtemp_pattern(R"(SYSRTEMP\s*=\s*.+)");

    while (getline(in, line)) {
        if (regex_match(line, sysrtemp_pattern)) {
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

// ✏️ Patch nc.tcl to fix paths and add missing copy commands
bool patchNcTcl(const string& ncTclPath, const unordered_map<string, string>& fileMap) {
    ifstream in(ncTclPath);
    stringstream buffer;
    string line;

    regex copy_pattern(R"(file copy -force\s+(.*?)\s+(.*))");
    unordered_set<string> alreadyHandled;
    bool modified = false;

    cout << "\n🔍 [DEBUG] Starting nc.tcl patch..." << endl;

    while (getline(in, line)) {
        smatch match;
        if (regex_match(line, match, copy_pattern)) {
            string source = match[1].str();
            string destination = match[2].str();

            size_t lastSlash = source.find_last_of("/");
            string filename = (lastSlash != string::npos) ? source.substr(lastSlash + 1) : source;

            cout << "[DEBUG] Matched line: " << line << endl;
            cout << "         → Filename: " << filename << ", Destination: " << destination << endl;

            if (fileMap.count(filename)) {
                alreadyHandled.insert(filename);
                if (destination != "../") {
                    buffer << "file copy -force " << source << " ../" << endl;
                    cout << "[DEBUG] ➤ Updated destination for: " << filename << endl;
                    modified = true;
                } else {
                    buffer << line << endl;
                }
            } else {
                buffer << line << endl;
            }
        } else {
            buffer << line << endl;
        }
    }
    in.close();

    // ➕ Add missing file copy -force commands
    for (const auto& [filename, fullpath] : fileMap) {
        if (!alreadyHandled.count(filename)) {
            cout << "[DEBUG] ➕ Appending missing file copy for: " << filename << endl;
            buffer << "file copy -force ../" << fullpath << " ../" << endl;
            modified = true;
        }
    }

    if (modified) {
        ofstream out(ncTclPath);
        out << buffer.str();
    } else {
        cout << "[DEBUG] 🎉 No changes needed in nc.tcl" << endl;
    }

    return modified;
}

int main() {
    string goldsPath = "golds";
    string makefilePath = "Makefile";
    string ncTclPath = "scripts/nc.tcl";

    // Step 1: Get gold filenames
    unordered_set<string> goldFiles = getGoldFilenames(goldsPath);

    // Step 2: Extract SYSRTEMP path
    ifstream makefile(makefilePath);
    string sysrtempPath;
    regex sysretemp_regex(R"(SYSRTEMP\s*=\s*(.+))");
    string line;
    while (getline(makefile, line)) {
        smatch match;
        if (regex_match(line, match, sysretemp_regex)) {
            sysrtempPath = match[1].str();
            break;
        }
    }
    makefile.close();

    if (sysrtempPath.empty()) {
        cerr << "❌ Error: Could not find SYSRTEMP in Makefile." << endl;
        return 1;
    }

    // Step 3: Map gold files to result file paths
    unordered_map<string, string> fileMap;
    scanDirectory(sysrtempPath, goldFiles, fileMap);

    // Step 4: Patch files
    bool changedNcTcl = patchNcTcl(ncTclPath, fileMap);
    bool changedMakefile = patchMakefile(makefilePath);

    // Step 5: Final result
    if (!changedNcTcl && !changedMakefile) {
        cout << "\n✅ All files already up to date. No changes made." << endl;
    } else {
        cout << "\n✅ Refactoring complete. Files updated:" << endl;
        if (changedNcTcl) cout << " - " << ncTclPath << endl;
        if (changedMakefile) cout << " - " << makefilePath << endl;
    }

    return 0;
}
