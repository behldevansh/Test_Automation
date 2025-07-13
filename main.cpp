#include <iostream>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <string>
#include <sstream>

namespace fs = std::filesystem;
using namespace std;

// Get all filenames in golds/
unordered_set<string> getGoldFilenames(const string& goldsPath) {
    unordered_set<string> goldFiles;
    for (const auto& entry : fs::directory_iterator(goldsPath)) {
        if (entry.is_regular_file()) {
            goldFiles.insert(entry.path().filename().string());
        }
    }
    return goldFiles;
}

// Recursively scan results directory for matching files
unordered_map<string, string> getResultFileMap(const string& resultsPath, const unordered_set<string>& goldFiles) {
    unordered_map<string, string> fileMap;
    for (const auto& entry : fs::recursive_directory_iterator(resultsPath)) {
        if (entry.is_regular_file()) {
            string filename = entry.path().filename().string();
            if (goldFiles.count(filename)) {
                fileMap[filename] = entry.path().string();
            }
        }
    }
    return fileMap;
}

// Modify Makefile's SYSRTEMP line (overwrite original)
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

// Patch nc.tcl (overwrite original), returns whether anything changed
bool patchNcTcl(const string& ncTclPath, const unordered_map<string, string>& fileMap) {
    ifstream in(ncTclPath);
    stringstream buffer;
    string line;

    regex copy_pattern(R"(file copy -force\s+\.\./.+?/([^/\s]+)\s+\.\./.+)");
    unordered_set<string> alreadyHandled;
    bool modified = false;

    while (getline(in, line)) {
        smatch match;
        if (regex_search(line, match, copy_pattern)) {
            string filename = match[1].str();
            if (fileMap.count(filename)) {
                alreadyHandled.insert(filename);
                std::size_t last_space = line.rfind(" ");
                string newLine = line.substr(0, last_space) + " ../";
                buffer << newLine << endl;
                if (newLine != line) modified = true;
            } else {
                buffer << line << endl;
            }
        } else {
            buffer << line << endl;
        }
    }
    in.close();

    // Append missing copy commands
    for (const auto& [filename, fullpath] : fileMap) {
        if (!alreadyHandled.count(filename)) {
            buffer << "file copy -force ../" << fullpath << " ../" << endl;
            modified = true;
        }
    }

    if (modified) {
        ofstream out(ncTclPath);
        out << buffer.str();
    }

    return modified;
}

int main() {
    string goldsPath = "golds";
    string makefilePath = "Makefile";
    string ncTclPath = "scripts/nc.tcl";

    // Step 1: Get gold files
    unordered_set<string> goldFiles = getGoldFilenames(goldsPath);

    // Step 2: Parse Makefile to extract SYSRTEMP
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
    unordered_map<string, string> fileMap = getResultFileMap(sysrtempPath, goldFiles);

    // Step 4: Patch nc.tcl and Makefile
    bool ncTclChanged = patchNcTcl(ncTclPath, fileMap);
    bool makefileChanged = patchMakefile(makefilePath);

    // Final Output
    if (!ncTclChanged && !makefileChanged) {
        cout << "✅ All files already up to date. No changes made." << endl;
    } else {
        cout << "✅ Refactoring complete. Files updated:" << endl;
        if (ncTclChanged) cout << " - " << ncTclPath << endl;
        if (makefileChanged) cout << " - " << makefilePath << endl;
    }

    return 0;
}
