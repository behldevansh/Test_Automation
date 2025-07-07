#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <regex>
#include <dirent.h>
#include <sys/types.h>
 
using namespace std;
 
set<string> getGoldFiles(const string& goldPath) {
    set<string> goldFiles;
    DIR* dir = opendir(goldPath.c_str());
    if (!dir) {
        cerr << "Error: Could not open Gold folder: " << goldPath << endl;
        return goldFiles;
    }
 
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name != "." && name != "..") {
            goldFiles.insert(name);
        }
    }
    closedir(dir);
    return goldFiles;
}
 
void updateNcTcl(const string& ncTclPath, const set<string>& goldFiles) {
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
            string filename = src.substr(src.find_last_of('/') + 1);
 
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
            string srcGuess = "../output/**/" + file; // Placeholder
            updatedContent += "file copy -force " + srcGuess + " ../\n";
        }
    }
 
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
    while (getline(inFile, line)) {
        if (line.find("SYSRTEMP=") == 0) {
            line = "SYSRTEMP=.";
        }
        updatedContent += line + "\n";
    }
    inFile.close();
 
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
    updateNcTcl(ncTclPath, goldFiles);
    updateMakefile(makefilePath);
 
    cout << "Refactoring complete." << endl;
    return 0;
}