#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>

struct PatchData {
    std::vector<int> original;  // -1 represents wildcard ??
    std::vector<uint8_t> patched;
};

// Trim whitespace from string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// Parse hex byte sequence
std::vector<int> parseHexBytes(const std::string& hexStr, bool allowWildcard) {
    std::vector<int> bytes;
    std::istringstream iss(hexStr);
    std::string token;

    while (iss >> token) {
        if (token == "??") {
            if (allowWildcard) {
                bytes.push_back(-1);  // Wildcard
            }
            else {
                std::cerr << "错误：PATCHED 部分不允许使用通配符 ??" << std::endl;
                return {};
            }
        }
        else {
            try {
                int byte = std::stoi(token, nullptr, 16);
                if (byte < 0 || byte > 255) {
                    std::cerr << "错误：无效的字节值(你的mc是不是下载有问题?去mcappx下载):" << token << std::endl;
                    return {};
                }
                bytes.push_back(byte);
            }
            catch (...) {
                std::cerr << "错误:解析不了这个十六进制值:" << token << std::endl;
                return {};
            }
        }
    }

    return bytes;
}

// Read patch file
bool readPatchFile(const std::string& patchFile, PatchData& patchData) {
    std::ifstream file(patchFile);
    if (!file.is_open()) {
        std::cerr << "错误:不能打开补丁文件" << patchFile << std::endl;
        return false;
    }

    std::string line;
    std::string originalHex;
    std::string patchedHex;
    bool inOriginal = false;
    bool inPatched = false;

    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty()) continue;

        if (line.find("ORIGINAL") != std::string::npos) {
            inOriginal = true;
            inPatched = false;
            continue;
        }

        if (line.find("PATCHED") != std::string::npos) {
            inOriginal = false;
            inPatched = true;
            continue;
        }

        if (inOriginal) {
            originalHex += line + " ";
        }
        else if (inPatched) {
            patchedHex += line + " ";
        }
    }

    file.close();

    // Parse ORIGINAL (allow wildcards)
    patchData.original = parseHexBytes(originalHex, true);
    if (patchData.original.empty()) {
        std::cerr << "错误：无法解析原始字节序列" << std::endl;
        return false;
    }

    // Parse PATCHED (no wildcards)
    std::vector<int> patchedTemp = parseHexBytes(patchedHex, false);
    if (patchedTemp.empty()) {
        std::cerr << "错误：无法解析原始字节序列" << std::endl;
        return false;
    }

    patchData.patched.resize(patchedTemp.size());
    for (size_t i = 0; i < patchedTemp.size(); i++) {
        patchData.patched[i] = static_cast<uint8_t>(patchedTemp[i]);
    }

    // Check length mismatch
    if (patchData.original.size() != patchData.patched.size()) {
        std::cerr << "Warning: ORIGINAL and PATCHED length mismatch ("
            << patchData.original.size() << " vs "
            << patchData.patched.size() << ")" << std::endl;
        std::cerr << "Will use shorter length for patching" << std::endl;
    }

    return true;
}

// Read binary file
bool readBinaryFile(const std::string& filename, std::vector<uint8_t>& data) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file: " << filename << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    data.resize(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    return true;
}

// Write binary file
bool writeBinaryFile(const std::string& filename, const std::vector<uint8_t>& data) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "错误:糟糕,创建不了输出文件!建议使用管理员权限运行:" << filename << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();

    return true;
}

// Find pattern in binary data
size_t findPattern(const std::vector<uint8_t>& data, const std::vector<int>& pattern) {
    if (pattern.empty() || data.size() < pattern.size()) {
        return std::string::npos;
    }

    for (size_t i = 0; i <= data.size() - pattern.size(); i++) {
        bool match = true;

        for (size_t j = 0; j < pattern.size(); j++) {
            if (pattern[j] != -1 && data[i + j] != static_cast<uint8_t>(pattern[j])) {
                match = false;
                break;
            }
        }

        if (match) {
            return i;
        }
    }

    return std::string::npos;
}

// Apply patch
bool applyPatch(std::vector<uint8_t>& data, const PatchData& patchData) {
    size_t offset = findPattern(data, patchData.original);

    if (offset == std::string::npos) {
        std::cerr << "错误：文件中未找到匹配的模式" << std::endl;
        return false;
    }

    std::cout << "在偏移处找到模式: 0x" << std::hex << std::uppercase
        << offset << std::dec << " (" << offset << ")" << std::endl;

    // Apply patch
    size_t patchSize = std::min(patchData.original.size(), patchData.patched.size());
    for (size_t i = 0; i < patchSize; i++) {
        data[offset + i] = patchData.patched[i];
    }

    std::cout << "Patched " << patchSize << " bytes" << std::endl;

    // Check for multiple matches
    size_t nextOffset = findPattern(
        std::vector<uint8_t>(data.begin() + offset + 1, data.end()),
        patchData.original
    );

    if (nextOffset != std::string::npos) {
        std::cerr << "警告：找到多个模式匹配，仅修补了第一个" << std::endl;
    }

    return true;
}

int main(int argc, char* argv[]) {
    std::cout << "===二进制修补程序===" << std::endl;

    if (argc < 3) {
        std::cerr << "用法: " << argv[0] << " <input_file> <patch_file> <patch_file2>  ..." << std::endl;
        std::cerr << "示例: " << argv[0] << " Minecraft.Windows.exe patch.txt" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
	//backup original file to .bak
	std::ifstream src(inputFile, std::ios::binary);
	std::ofstream dst(inputFile + ".bak", std::ios::binary);
	dst << src.rdbuf();
	//close orignal and backup files
	src.close();
	dst.close();

    std::vector<uint8_t> data;
    if (!readBinaryFile(inputFile, data)) {
		std::cerr << "无法读取输入文件: " << inputFile << std::endl;
        return 1;
    }

	
    for (int i = 2; i < argc; i++) {
		std::string patchFile = argv[i];
        PatchData patchData;
        if (!readPatchFile(patchFile, patchData)) {
			std::cerr << "无法读取补丁文件: " << patchFile << std::endl;
        }
        std::cout << "原始尺寸: " << patchData.original.size() << " bytes" << std::endl;
        std::cout << "补丁尺寸: " << patchData.patched.size() << " bytes" << std::endl;
        if (!applyPatch(data, patchData)) {
			std::cerr << "Failed to apply patch from file: " << patchFile << std::endl;
        }
	}
	std::string outputFile = inputFile; // Overwrite input file
    if (!writeBinaryFile(outputFile, data)) {
        return 1;
    }
    std::cout << "修补成功!输出文件: " << outputFile << std::endl;

    return 0;
}
