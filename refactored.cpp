// #include <iostream>
// #include <fstream>
// #include <unordered_set>
// #include <unordered_map>
// #include <regex>
// #include <string>
// #include <sstream>
// #include <dirent.h>
// #include <sys/stat.h>

// using namespace std;

// // 🔁 Recursively scan directory for matching gold files
// void scanDirectory(const string& basePath, const unordered_set<string>& goldFiles, unordered_map<string, string>& fileMap) {
//     DIR* dir = opendir(basePath.c_str());
//     if (!dir) return;

//     struct dirent* entry;
//     while ((entry = readdir(dir)) != nullptr) {
//         string name = entry->d_name;
//         if (name == "." || name == "..") continue;

//         string fullPath = basePath + "/" + name;

//         struct stat st;
//         if (stat(fullPath.c_str(), &st) == -1) continue;

//         if (S_ISDIR(st.st_mode)) {
//             scanDirectory(fullPath, goldFiles, fileMap);  // recursive
//         } else if (S_ISREG(st.st_mode)) {
//             if (goldFiles.count(name)) {
//                 fileMap[name] = fullPath;
//             }
//         }
//     }
//     closedir(dir);
// }

// // 📁 Get all file names from golds/
// unordered_set<string> getGoldFilenames(const string& goldsPath) {
//     unordered_set<string> goldFiles;
//     DIR* dir = opendir(goldsPath.c_str());
//     if (!dir) {
//         cerr << "❌ Could not open golds folder!" << endl;
//         return goldFiles;
//     }

//     struct dirent* entry;
//     while ((entry = readdir(dir)) != nullptr) {
//         string name = entry->d_name;
//         if (name == "." || name == "..") continue;
//         goldFiles.insert(name);
//     }

//     closedir(dir);
//     return goldFiles;
// }

// // ✏️ Fix the Makefile to set SYSRTEMP = .
// bool patchMakefile(const string& makefilePath) {
//     ifstream in(makefilePath);
//     stringstream buffer;
//     string line;
//     bool modified = false;
//     regex sysrtemp_pattern(R"(SYSRTEMP\s*=)");

//     while (getline(in, line)) {
//         if (regex_search(line, sysrtemp_pattern)) {
//             buffer << "SYSRTEMP = ." << endl;
//             modified = true;
//         } else {
//             buffer << line << endl;
//         }
//     }
//     in.close();

//     if (modified) {
//         ofstream out(makefilePath);
//         out << buffer.str();
//     }

//     return modified;
// }

// // ✏️ Patch nc.tcl — fix existing and add missing file copy -force commands
// bool patchNcTcl(const string& ncTclPath, const unordered_map<string, string>& fileMap) {
//     ifstream in(ncTclPath);
//     stringstream buffer;
//     string line;

//     regex copy_pattern(R"(file copy -force\s+(.*?)\s+(.*))");

//     unordered_set<string> alreadyHandled;
//     bool modified = false;

//     cout << "\n🔍 [DEBUG] Scanning nc.tcl..." << endl;

//     while (getline(in, line)) {
//         smatch match;
//         if (regex_match(line, match, copy_pattern)) {
//             string source = match[1].str();
//             string destination = match[2].str();

//             // Extract filename
//             size_t lastSlash = source.find_last_of("/");
//             string filename = (lastSlash != string::npos) ? source.substr(lastSlash + 1) : source;

//             if (fileMap.count(filename)) {
//                 alreadyHandled.insert(filename);

//                 if (destination != "../") {
//                     cout << "[DEBUG] ➤ Fixing destination for: " << filename << endl;
//                     buffer << "file copy -force " << source << " ../" << endl;
//                     modified = true;
//                 } else {
//                     buffer << line << endl;
//                 }
//             } else {
//                 buffer << line << endl; // not in golds
//             }
//         } else {
//             buffer << line << endl; // not a copy line
//         }
//     }
//     in.close();

//     // ➕ Append missing copy commands
//     for (const auto& [filename, fullpath] : fileMap) {
//         if (!alreadyHandled.count(filename)) {
//             cout << "[DEBUG] ➕ Adding missing file copy for: " << filename << endl;
//             buffer << "file copy -force ../" << fullpath << " ../" << endl;
//             modified = true;
//         }
//     }

//     if (modified) {
//         ofstream out(ncTclPath);
//         out << buffer.str();
//     } else {
//         cout << "[DEBUG] 🎉 nc.tcl is already up to date." << endl;
//     }

//     return modified;
// }

// int main() {
//     string goldsPath = "golds";
//     string makefilePath = "Makefile";
//     string ncTclPath = "scripts/nc.tcl";

//     // Step 1: Load gold file names
//     unordered_set<string> goldFiles = getGoldFilenames(goldsPath);
//     if (goldFiles.empty()) {
//         cerr << "❌ No files found in golds folder." << endl;
//         return 1;
//     }

//     // Step 2: Read SYSRTEMP path from Makefile
//     ifstream makefile(makefilePath);
//     string sysrtempPath;
//     regex sysrtemp_regex(R"(SYSRTEMP\s*=\s*(.+))");
//     string line;
//     while (getline(makefile, line)) {
//         smatch match;
//         if (regex_match(line, match, sysrtemp_regex)) {
//             sysrtempPath = match[1].str();
//             break;
//         }
//     }
//     makefile.close();

//     if (sysrtempPath.empty()) {
//         cerr << "❌ SYSRTEMP not found in Makefile." << endl;
//         return 1;
//     }

//     // Step 3: Find matching files in SYSRTEMP folder
//     unordered_map<string, string> fileMap;
//     scanDirectory(sysrtempPath, goldFiles, fileMap);
//     if (fileMap.empty()) {
//         cerr << "⚠️ No matching files found in results directory." << endl;
//     }

//     // Step 4: Patch files
//     bool changedNcTcl = patchNcTcl(ncTclPath, fileMap);
//     bool changedMakefile = patchMakefile(makefilePath);

//     // Step 5: Report
//     if (!changedNcTcl && !changedMakefile) {
//         cout << "\n✅ Everything already perfect. No changes made." << endl;
//     } else {
//         cout << "\n✅ Refactoring complete." << endl;
//         if (changedNcTcl) cout << "  → nc.tcl updated." << endl;
//         if (changedMakefile) cout << "  → Makefile updated." << endl;
//     }

//     return 0;
// }
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <string>
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>
#include <cstring>

using namespace std;

string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Recursively scan directory and collect files with names in golds
void scanDir(const string &base, const string &rel,
             const unordered_set<string> &goldNames,
             unordered_map<string,string> &found) {
    string path = base + "/" + rel;
    DIR *dir = opendir(path.c_str());
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        string name = entry->d_name;
        if (name == "." || name == "..") continue;
        string full = path + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                string nextRel = rel.empty() ? name : (rel + "/" + name);
                scanDir(base, nextRel, goldNames, found);
            } else if (S_ISREG(st.st_mode)) {
                if (goldNames.count(name)) {
                    string relPath = rel.empty() ? name : (rel + "/" + name);
                    found[name] = base + "/" + relPath;
                }
            }
        }
    }
    closedir(dir);
}

int main() {
    // 1. Read gold names
    unordered_set<string> goldNames;
    {
        DIR *d = opendir("golds");
        if (!d) {
            cerr << "❌ golds/ folder not found\n";
            return 1;
        }
        struct dirent *ent;
        while ((ent = readdir(d)) != nullptr) {
            string fn = ent->d_name;
            if (fn == "." || fn == "..") continue;
            string full = string("golds/") + fn;
            struct stat st;
            if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
                goldNames.insert(fn);
            }
        }
        closedir(d);
    }
    if (goldNames.empty()) {
        cerr << "❌ No files in golds/\n";
        return 1;
    }

    // 2. Parse Makefile
    string sysrtemp;
    vector<string> makeLines;
    {
        ifstream mf("Makefile");
        if (!mf) {
            cerr << "❌ Makefile not found\n";
            return 1;
        }
        string line;
        regex reSys(R"(SYSRTEMP\s*=\s*(.+))");
        smatch m;
        while (getline(mf, line)) {
            makeLines.push_back(line);
            if (regex_search(line, m, reSys)) {
                sysrtemp = trim(m[1]);
            }
        }
        mf.close();
    }
    if (sysrtemp.empty()) {
        cerr << "❌ SYSRTEMP not found in Makefile\n";
        return 1;
    }

    // 3. Scan SYSRTEMP recursively
    unordered_map<string,string> found;
    scanDir(sysrtemp, "", goldNames, found);
    if (found.empty()) {
        cerr << "❌ No matching files in SYSRTEMP folder\n";
        return 1;
    }

    // 4. Read nc.tcl
    string ncPath = "scripts/nc.tcl";
    ifstream ncin(ncPath);
    if (!ncin) {
        cerr << "❌ scripts/nc.tcl not found\n";
        return 1;
    }
    vector<string> ncLines;
    string l;
    while (getline(ncin, l)) ncLines.push_back(l);
    ncin.close();

    regex reCopy(R"(file\s+copy\s+-force\s+([^\s]+)\s+([^\s]+))");
    bool changedNc = false;
    unordered_set<string> handled;
    for (string &line : ncLines) {
        smatch m;
        if (regex_search(line, m, reCopy)) {
            string src = m[1];
            string dst = m[2];
            string fname = src.substr(src.find_last_of("/\\") + 1);
            if (goldNames.count(fname)) {
                handled.insert(fname);
                if (dst != "../") {
                    line = "file copy -force " + src + " ../";
                    changedNc = true;
                }
            }
        }
    }
    // Append missing
    for (auto &kv : found) {
        if (!handled.count(kv.first)) {
            string newLine = "file copy -force ../" + kv.second + " ../";
            ncLines.push_back(newLine);
            changedNc = true;
        }
    }

    // 5. Write nc.tcl if changed
    if (changedNc) {
        ofstream ncout(ncPath, ios::trunc);
        for (auto &x : ncLines) ncout << x << "\n";
        ncout.close();
        cout << "✅ Updated scripts/nc.tcl\n";
    }

    // 6. Update Makefile if needed
    bool changedMf = false;
    for (string &line : makeLines) {
        smatch m;
        if (regex_search(line, m, reCopy)) continue; // skip copy lines
        if (regex_search(line, m, regex(R"(SYSRTEMP\s*=\s*(.+))"))) {
            string val = trim(m[1]);
            if (val != ".") {
                line = "SYSRTEMP = .";
                changedMf = true;
            }
        }
    }
    if (changedMf) {
        ofstream mfout("Makefile", ios::trunc);
        for (auto &x : makeLines) mfout << x << "\n";
        mfout.close();
        cout << "✅ Updated Makefile SYSRTEMP to '.'\n";
    }

    if (!changedNc && !changedMf) {
        cout << "✅ Everything already perfect. No changes made.\n";
    }

    // 7. Summary
    for (auto &kv : found) {
        cout << "✔ Found: " << kv.first << " at " << kv.second << "\n";
    }

    return 0;
}

