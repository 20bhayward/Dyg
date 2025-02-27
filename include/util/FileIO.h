#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace dyg {
namespace util {

/**
 * File I/O utilities for saving and loading chunks
 */
class FileIO {
public:
    /**
     * Save raw binary data to a file
     * @param filePath Path to the file
     * @param data Data to save
     * @param useCompression Whether to use RLE compression
     * @return True if successful
     */
    static bool saveToFile(const std::string& filePath, const std::vector<uint8_t>& data, bool useCompression);
    
    /**
     * Load raw binary data from a file
     * @param filePath Path to the file
     * @param useCompression Whether to use RLE decompression
     * @return Loaded data or empty vector if failed
     */
    static std::vector<uint8_t> loadFromFile(const std::string& filePath, bool useCompression);
    
    /**
     * Check if a file exists
     * @param filePath Path to the file
     * @return True if the file exists
     */
    static bool fileExists(const std::string& filePath);
    
    /**
     * Create directory if it doesn't exist
     * @param dirPath Path to the directory
     * @return True if successful
     */
    static bool createDirectory(const std::string& dirPath);

private:
    /**
     * Compress data using run-length encoding
     * @param data Data to compress
     * @return Compressed data
     */
    static std::vector<uint8_t> compressRLE(const std::vector<uint8_t>& data);
    
    /**
     * Decompress data using run-length encoding
     * @param compressedData Compressed data
     * @return Decompressed data
     */
    static std::vector<uint8_t> decompressRLE(const std::vector<uint8_t>& compressedData);
};

} // namespace util
} // namespace dyg