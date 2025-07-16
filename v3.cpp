#include <iostream>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <string>
#include <sstream>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>

bool is_regular_file(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

bool is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

// List all files in a directory (non-recursive)
std::unordered_set<std::string> get_gold_filenames(const std::string& golds_dir) {
    std::unordered_set<std::string> gold_files;
    DIR* dir = opendir(golds_dir.c_str());
    if (!dir) return gold_files;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            gold_files.insert(entry->d_name);
        }
    }
    closedir(dir);
    return gold_files;
}

// Recursively find all files in SYSRTEMP matching golds filenames
void find_result_files(
    const std::string& dir,
    const std::unordered_set<std::string>& gold_files,
    std::unordered_map<std::string, std::string>& found
) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string fullpath = dir + "/" + name;
        if (entry->d_type == DT_DIR) {
            find_result_files(fullpath, gold_files, found);
        } else if (entry->d_type == DT_REG) {
            if (gold_files.count(name)) {
                found[name] = fullpath;
            }
        }
    }
    closedir(d);
}

// Read SYSRTEMP value from Makefile
std::string get_sysrtemp_value(const std::string& makefile_path) {
    std::ifstream in(makefile_path);
    std::string line;
    std::regex sysrtemp_regex(R"(SYSRTEMP\s*=\s*(.*))");
    std::smatch match;
    while (std::getline(in, line)) {
        if (std::regex_match(line, match, sysrtemp_regex)) {
            return match[1].str();
        }
    }
    return "";
}

// Replace SYSRTEMP line in Makefile
bool update_makefile(const std::string& makefile_path) {
    std::ifstream in(makefile_path);
    if (!in) return false;
    std::vector<std::string> lines;
    std::string line;
    bool changed = false;
    std::regex sysrtemp_regex(R"(SYSRTEMP\s*=.*)");
    while (std::getline(in, line)) {
        if (std::regex_match(line, sysrtemp_regex)) {
            lines.push_back("SYSRTEMP = .");
            changed = true;
        } else {
            lines.push_back(line);
        }
    }
    in.close();
    if (changed) {
        std::ofstream out(makefile_path, std::ios::trunc);
        for (const auto& l : lines) out << l << "\n";
    }
    return changed;
}

// Read all lines from a file
std::vector<std::string> read_lines(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

// Write all lines to a file
void write_lines(const std::string& path, const std::vector<std::string>& lines) {
    std::ofstream out(path, std::ios::trunc);
    for (const auto& l : lines) out << l << "\n";
}

int main() {
    std::string golds_dir = "golds";
    std::string makefile_path = "Makefile";
    std::string nctcl_path = "scripts/nc.tcl";

    // 1. Get gold filenames
    if (!is_directory(golds_dir)) {
        std::cerr << "golds/ folder not found.\n";
        return 1;
    }
    auto gold_files = get_gold_filenames(golds_dir);
    if (gold_files.empty()) {
        std::cout << "No files in golds/. Nothing to do.\n";
        return 0;
    }

    // 2. Get SYSRTEMP value
    std::string sysrtemp = get_sysrtemp_value(makefile_path);
    if (sysrtemp.empty() || !is_directory(sysrtemp)) {
        std::cerr << "SYSRTEMP not found or folder does not exist.\n";
        return 1;
    }

    // 3. Find result files matching golds
    std::unordered_map<std::string, std::string> found_files;
    find_result_files(sysrtemp, gold_files, found_files);

    // 4. Read nc.tcl lines
    auto nc_lines = read_lines(nctcl_path);

    // 5. Track changes
    bool nc_changed = false;
    std::unordered_set<std::string> already_handled;

    // 6. For each gold file, ensure correct file copy line in nc.tcl
    for (const auto& kv : found_files) {
        const std::string& fname = kv.first;
        const std::string& result_path = kv.second;
        // Build the source path as used in nc.tcl (with forward slashes)
        std::string rel_source = result_path;
        // Remove leading "./" if present
        if (rel_source.substr(0, 2) == "./") rel_source = rel_source.substr(2);

        // Regex to match file copy -force for this file
        std::regex copy_regex("file copy -force [^ ]*" + fname + R"(\s+[^ ]*)");
        bool found = false;
        for (auto& line : nc_lines) {
            if (std::regex_search(line, copy_regex)) {
                // Fix destination to ../
                line = "file copy -force ../" + rel_source + " ../";
                nc_changed = true;
                found = true;
                already_handled.insert(fname);
                break;
            }
        }
        if (!found) {
            // Append new line
            nc_lines.push_back("file copy -force ../" + rel_source + " ../");
            nc_changed = true;
            already_handled.insert(fname);
        }
    }

    // 7. Update nc.tcl if changed
    if (nc_changed) {
        write_lines(nctcl_path, nc_lines);
    }

    // 8. Update Makefile if needed
    bool makefile_changed = update_makefile(makefile_path);

    // 9. Print summary
    if (!nc_changed && !makefile_changed) {
        std::cout << "✅ Everything already perfect. No changes made.\n";
    } else {
        if (nc_changed) {
            std::cout << "✅ Updated scripts/nc.tcl with correct file copy commands for:\n";
            for (const auto& fname : already_handled) {
                std::cout << "  - " << fname << "\n";
            }
        }
        if (makefile_changed) {
            std::cout << "✅ Updated Makefile: set SYSRTEMP = .\n";
        }
    }
    return 0;
}