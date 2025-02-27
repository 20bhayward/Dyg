#include "util/FileIO.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <algorithm>

namespace dyg {
namespace util {

bool FileIO::saveToFile(const std::string& filePath, const std::vector<uint8_t>& data, bool useCompression) {
    // Create directory if it doesn't exist
    size_t pos = filePath.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dirPath = filePath.substr(0, pos);
        if (!createDirectory(dirPath)) {
            std::cerr << "Failed to create directory: " << dirPath << std::endl;
            return false;
        }
    }

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return false;
    }

    std::vector<uint8_t> dataToWrite;
    if (useCompression) {
        dataToWrite = compressRLE(data);
        
        // Write a simple header to indicate compression
        uint8_t compressionFlag = 1;
        file.write(reinterpret_cast<const char*>(&compressionFlag), sizeof(compressionFlag));
        
        // Write the original size
        uint32_t originalSize = static_cast<uint32_t>(data.size());
        file.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));
    } else {
        dataToWrite = data;
        
        // Write a simple header to indicate no compression
        uint8_t compressionFlag = 0;
        file.write(reinterpret_cast<const char*>(&compressionFlag), sizeof(compressionFlag));
    }

    // Write the actual data
    file.write(reinterpret_cast<const char*>(dataToWrite.data()), dataToWrite.size());

    return !file.bad();
}

std::vector<uint8_t> FileIO::loadFromFile(const std::string& filePath, bool useCompression) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << filePath << std::endl;
        return {};
    }

    // Get file size
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize <= 0) {
        std::cerr << "File is empty: " << filePath << std::endl;
        return {};
    }

    // Read the compression flag
    uint8_t compressionFlag;
    file.read(reinterpret_cast<char*>(&compressionFlag), sizeof(compressionFlag));
    
    // If compressed, read the original size
    uint32_t originalSize = 0;
    if (compressionFlag == 1) {
        file.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
    }

    // Calculate remaining data size
    std::streamsize dataSize = fileSize - file.tellg();
    
    // Read the data
    std::vector<uint8_t> data(dataSize);
    file.read(reinterpret_cast<char*>(data.data()), dataSize);

    // Decompress if necessary
    if (compressionFlag == 1 && useCompression) {
        return decompressRLE(data);
    }

    return data;
}

bool FileIO::fileExists(const std::string& filePath) {
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

bool FileIO::createDirectory(const std::string& dirPath) {
    // Create directories recursively
    std::string path;
    for (char c : dirPath) {
        if (c == '/') {
            if (!path.empty() && !fileExists(path)) {
                #ifdef _WIN32
                    int result = mkdir(path.c_str());
                #else
                    int result = mkdir(path.c_str(), 0755);
                #endif
                if (result != 0) {
                    return false;
                }
            }
        }
        path += c;
    }
    
    // Create the final directory if it doesn't exist
    if (!path.empty() && !fileExists(path)) {
        #ifdef _WIN32
            int result = mkdir(path.c_str());
        #else
            int result = mkdir(path.c_str(), 0755);
        #endif
        if (result != 0) {
            return false;
        }
    }
    
    return true;
}

std::vector<uint8_t> FileIO::compressRLE(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> compressed;
    
    if (data.empty()) {
        return compressed;
    }
    
    uint8_t currentByte = data[0];
    uint8_t count = 1;
    
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i] == currentByte && count < 255) {
            count++;
        } else {
            compressed.push_back(count);
            compressed.push_back(currentByte);
            currentByte = data[i];
            count = 1;
        }
    }
    
    // Don't forget the last run
    compressed.push_back(count);
    compressed.push_back(currentByte);
    
    return compressed;
}

std::vector<uint8_t> FileIO::decompressRLE(const std::vector<uint8_t>& compressedData) {
    std::vector<uint8_t> decompressed;
    
    for (size_t i = 0; i < compressedData.size(); i += 2) {
        if (i + 1 >= compressedData.size()) {
            break;
        }
        
        uint8_t count = compressedData[i];
        uint8_t value = compressedData[i + 1];
        
        decompressed.insert(decompressed.end(), count, value);
    }
    
    return decompressed;
}

} // namespace util
} // namespace dyg