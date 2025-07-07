#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <regex>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

set<string> getGoldFiles(const string& goldPath) {
    set<string> goldFiles;
    for (const auto& entry : fs::recursive_directory_iterator(goldPath)) {
        if (fs::is_regular_file(entry)) {
            goldFiles.insert(entry.path().filename().string());
        }
    }
    return goldFiles;
}

string extractSYSRTEMP(const string& makefilePath) {
    ifstream inFile(makefilePath);
    if (!inFile) {
        cerr << "Error: Could not open Makefile for reading." << endl;
        return "";
    }

    string line;
    regex pattern(R"(SYSRTEMP\s*=\s*(.*))");
    smatch match;

    while (getline(inFile, line)) {
        if (regex_match(line, match, pattern)) {
            return match[1];
        }
    }
    return "";
}

void updateNcTcl(const string& ncTclPath, const set<string>& goldFiles, const string& sysrtempPath) {
    ifstream inFile(ncTclPath);
    if (!inFile) {
        cerr << "Error: Could not open nc.tcl for reading." << endl;
        return;
    }

    string updatedContent;
    string line;
    set<string> foundFiles;
    regex copyRegex(R"(file copy -force\s+([^\s]+)\s+([^\s]+))");

    while (getline(inFile, line)) {
        smatch match;
        if (regex_search(line, match, copyRegex)) {
            string src = match[1];
            string dst = match[2];
            string filename = fs::path(src).filename().string();

            if (goldFiles.count(filename)) {
                foundFiles.insert(filename);
                if (dst != "../") {
                    line = "file copy -force " + src + " ../";
                }
            }
        }
        updatedContent += line + "\n";
    }
    inFile.close();

    for (const auto& file : goldFiles) {
        if (!foundFiles.count(file)) {
            string guessPath = sysrtempPath.empty() ? "../output/**/" : "../" + sysrtempPath + "/";
            updatedContent += "file copy -force " + guessPath + file + " ../\n";
        }
    }

    // Optional: Backup original
    fs::copy_file(ncTclPath, ncTclPath + ".bak", fs::copy_options::overwrite_existing);

    ofstream outFile(ncTclPath);
    if (!outFile) {
        cerr << "Error: Could not open nc.tcl for writing." << endl;
        return;
    }
    outFile << updatedContent;
    outFile.close();
}

void updateMakefile(const string& makefilePath) {
    ifstream inFile(makefilePath);
    if (!inFile) {
        cerr << "Error: Could not open Makefile for reading." << endl;
        return;
    }

    string updatedContent;
    string line;
    regex sysRegex(R"(SYSRTEMP\s*=)");

    while (getline(inFile, line)) {
        if (regex_search(line, sysRegex)) {
            line = "SYSRTEMP=.";
        }
        updatedContent += line + "\n";
    }
    inFile.close();

    // Optional: Backup
    fs::copy_file(makefilePath, makefilePath + ".bak", fs::copy_options::overwrite_existing);

    ofstream outFile(makefilePath);
    if (!outFile) {
        cerr << "Error: Could not open Makefile for writing." << endl;
        return;
    }
    outFile << updatedContent;
    outFile.close();
}

int main() {
    string goldFolder = "Gold";
    string ncTclPath = "scripts/nc.tcl";
    string makefilePath = "Makefile";

    set<string> goldFiles = getGoldFiles(goldFolder);
    string sysrtemp = extractSYSRTEMP(makefilePath);

    updateNcTcl(ncTclPath, goldFiles, sysrtemp);
    updateMakefile(makefilePath);

    cout << "✅ Refactoring complete. Check .bak files for backup if needed." << endl;
    return 0;
}
