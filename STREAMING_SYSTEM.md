# PixelPhys2D Streaming System Implementation

This document outlines the implementation plan for adding a chunk streaming system to PixelPhys2D, based on the approach described in the Noita GDC talk.

## Core Requirements

- Implement a system where the world is divided into 512×512 pixel chunks
- Only keep 12 chunks loaded at any time
- Stream chunks in/out based on player position
- Save chunks to disk when unloaded
- Load chunks from disk when needed
- Generate new chunks procedurally if they don't exist
- Ensure consistent physics across chunk boundaries

## Implementation Checklist

### Phase 1: Core Data Structures
- [ ] Create `ChunkCoord` struct for addressing chunks
- [ ] Create `ChunkManager` class to handle chunk loading/unloading
- [ ] Implement chunk priority based on distance from player
- [ ] Setup storage structure for chunk files

### Phase 2: Serialization System
- [ ] Add serialization methods to `Chunk` class
- [ ] Add deserialization methods to `Chunk` class 
- [ ] Implement modification tracking for chunks
- [ ] Create folder structure for saving/loading chunks

### Phase 3: World Integration
- [ ] Modify `World` class to use `ChunkManager`
- [ ] Update chunk access methods to handle streaming
- [ ] Modify physics update to handle missing neighboring chunks
- [ ] Implement player position tracking for chunk loading

### Phase 4: Testing
- [ ] Create `TestWorld` class to demonstrate streaming
- [ ] Implement visualization of loaded/unloaded chunks
- [ ] Test chunk streaming with player movement
- [ ] Ensure physics work correctly across chunk boundaries

### Phase 5: Optimization
- [ ] Implement asynchronous chunk loading/unloading
- [ ] Add compression for chunk files
- [ ] Optimize serialization/deserialization speed
- [ ] Implement chunk generation optimizations

## Technical Design

### ChunkCoord Struct
```cpp
struct ChunkCoord {
    int x;
    int y;
    
    bool operator==(const ChunkCoord& other) const {
        return x == other.x && y == other.y;
    }
};

struct ChunkCoordHash {
    std::size_t operator()(const ChunkCoord& coord) const {
        return std::hash<int>()(coord.x) ^ (std::hash<int>()(coord.y) << 1);
    }
};
```

### ChunkManager Class
```cpp
class ChunkManager {
public:
    ChunkManager(int chunkSize = 512);
    ~ChunkManager();
    
    // Core chunk operations
    Chunk* getChunk(int chunkX, int chunkY, bool loadIfNeeded = true);
    void updateActiveChunks(int playerX, int playerY);
    void update();
    
    // Save all modified chunks
    void saveAllModifiedChunks();
    
    // Chunk file operations
    bool saveChunk(const ChunkCoord& coord);
    std::unique_ptr<Chunk> loadChunk(const ChunkCoord& coord);
    bool isChunkLoaded(const ChunkCoord& coord) const;
    
    // Check if chunk exists on disk
    bool chunkExistsOnDisk(const ChunkCoord& coord) const;
    
    // Get path for chunk file
    std::string getChunkFilePath(const ChunkCoord& coord) const;
    
private:
    // Map of loaded chunks
    std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> m_loadedChunks;
    
    // Set of modified chunks that need saving
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_dirtyChunks;
    
    // Currently active chunk coordinates
    std::vector<ChunkCoord> m_activeChunks;
    
    // Maximum number of chunks to keep loaded (like Noita's "12 chunks")
    const int MAX_LOADED_CHUNKS = 12;
    
    // Size of chunks in world units (Noita uses 512x512)
    const int m_chunkSize;
    
    // Base folder for chunk storage
    std::string m_chunkStoragePath;
    
    // Finds the furthest chunk from the player to unload
    ChunkCoord findFurthestChunk(int playerX, int playerY) const;
    
    // Calculate distance between chunk and player
    float calculateChunkDistance(const ChunkCoord& coord, int playerX, int playerY) const;
    
    // Create new chunk
    std::unique_ptr<Chunk> createNewChunk(const ChunkCoord& coord);
};
```

### Chunk Serialization Extensions
```cpp
class Chunk {
public:
    // Add new methods for serialization
    bool serialize(std::ostream& out) const;
    bool deserialize(std::istream& in);
    
    // Add a "modified" flag to track if chunk needs saving
    bool isModified() const { return m_isModified; }
    void setModified(bool modified) { m_isModified = modified; }
    
private:
    // New flag to track modifications since last save
    bool m_isModified = false;
};
```

### World Class Modifications
```cpp
class World {
public:
    World(int width, int height);
    ~World();
    
    // Update world with player position
    void updatePlayerPosition(int playerX, int playerY);
    
    // Get material at position (with streaming support)
    MaterialType get(int x, int y) const;
    void set(int x, int y, MaterialType material);
    
    // Update physics with streaming support
    void update();
    
    // Save world state
    void save();
    
private:
    // Replace chunk vector with ChunkManager
    ChunkManager m_chunkManager;
    
    // Track player position
    int m_playerX;
    int m_playerY;
    
    // Convert between world and chunk coordinates
    void worldToChunkCoords(int worldX, int worldY, int& chunkX, int& chunkY, int& localX, int& localY) const;
    
    // Get chunk at world position with streaming support
    Chunk* getChunkAt(int x, int y, bool loadIfNeeded = true);
};
```

## Implementation Details

### Chunk Loading/Unloading Logic

```cpp
void ChunkManager::updateActiveChunks(int playerX, int playerY) {
    // Convert player position to chunk coordinates
    int playerChunkX = playerX / m_chunkSize;
    int playerChunkY = playerY / m_chunkSize;
    
    // 1. Determine which chunks should be active (in a square around player)
    std::vector<ChunkCoord> desiredChunks;
    
    // Calculate view distance (chunks visible from player)
    // For 12 chunks in a square pattern, we need 3x4 or similar
    const int viewDistance = 2; // 5x5 grid = 25 chunks
    
    // Collect coordinates of chunks that should be loaded
    for (int y = playerChunkY - viewDistance; y <= playerChunkY + viewDistance; y++) {
        for (int x = playerChunkX - viewDistance; x <= playerChunkX + viewDistance; x++) {
            desiredChunks.push_back({x, y});
        }
    }
    
    // 2. Sort chunks by distance to player (closest first)
    std::sort(desiredChunks.begin(), desiredChunks.end(),
              [this, playerX, playerY](const ChunkCoord& a, const ChunkCoord& b) {
                  return calculateChunkDistance(a, playerX, playerY) < 
                         calculateChunkDistance(b, playerX, playerY);
              });
    
    // 3. Keep only closest MAX_LOADED_CHUNKS
    if (desiredChunks.size() > MAX_LOADED_CHUNKS) {
        desiredChunks.resize(MAX_LOADED_CHUNKS);
    }
    
    // 4. Unload chunks that are no longer needed
    std::vector<ChunkCoord> chunksToUnload;
    for (const auto& pair : m_loadedChunks) {
        const ChunkCoord& coord = pair.first;
        // If the chunk is not in the desired list, unload it
        if (std::find(desiredChunks.begin(), desiredChunks.end(), coord) == desiredChunks.end()) {
            chunksToUnload.push_back(coord);
        }
    }
    
    // Save and unload chunks
    for (const ChunkCoord& coord : chunksToUnload) {
        // Save if modified
        if (m_dirtyChunks.count(coord) > 0) {
            saveChunk(coord);
            m_dirtyChunks.erase(coord);
        }
        m_loadedChunks.erase(coord);
    }
    
    // 5. Load new chunks
    for (const ChunkCoord& coord : desiredChunks) {
        if (m_loadedChunks.count(coord) == 0) {
            // If the chunk exists on disk, load it
            if (chunkExistsOnDisk(coord)) {
                m_loadedChunks[coord] = loadChunk(coord);
            } else {
                // Otherwise, create a new empty chunk
                m_loadedChunks[coord] = createNewChunk(coord);
            }
        }
    }
    
    // Update active chunks list
    m_activeChunks = desiredChunks;
}
```

### Chunk Serialization Implementation

```cpp
bool Chunk::serialize(std::ostream& out) const {
    // Write header with version and dimensions
    uint8_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&m_posX), sizeof(m_posX));
    out.write(reinterpret_cast<const char*>(&m_posY), sizeof(m_posY));
    
    // Write materials grid (as MaterialType enum values)
    uint32_t gridSize = static_cast<uint32_t>(m_grid.size());
    out.write(reinterpret_cast<const char*>(&gridSize), sizeof(gridSize));
    out.write(reinterpret_cast<const char*>(m_grid.data()), gridSize * sizeof(MaterialType));
    
    // No need to store rendering data, it can be recalculated on load
    // No need to store physics state, it will be recalculated
    
    // Reset modified flag
    const_cast<Chunk*>(this)->setModified(false);
    
    return out.good();
}

bool Chunk::deserialize(std::istream& in) {
    // Read header
    uint8_t version;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&m_posX), sizeof(m_posX));
    in.read(reinterpret_cast<char*>(&m_posY), sizeof(m_posY));
    
    // Read materials grid
    uint32_t gridSize;
    in.read(reinterpret_cast<char*>(&gridSize), sizeof(gridSize));
    m_grid.resize(gridSize);
    in.read(reinterpret_cast<char*>(m_grid.data()), gridSize * sizeof(MaterialType));
    
    // Initialize remaining chunk state
    m_isDirty = true; // Mark for update when loaded
    m_shouldUpdateNextFrame = true;
    m_inactivityCounter = 0;
    m_isFreeFalling.resize(WIDTH * HEIGHT, false);
    
    // Rebuild pixel data for rendering
    updatePixelData();
    
    // Reset modified flag since we just loaded
    setModified(false);
    
    return in.good();
}
```

### Chunk File Storage

```cpp
std::string ChunkManager::getChunkFilePath(const ChunkCoord& coord) const {
    // Create multi-level directory structure to avoid too many files in one folder
    // Format: chunks/x/y.chunk where x and y are chunk coordinates
    std::ostringstream oss;
    oss << m_chunkStoragePath << "/";
    oss << coord.x << "/";
    oss << coord.y << ".chunk";
    return oss.str();
}

bool ChunkManager::saveChunk(const ChunkCoord& coord) {
    auto it = m_loadedChunks.find(coord);
    if (it == m_loadedChunks.end()) {
        return false; // Chunk not loaded
    }
    
    const Chunk* chunk = it->second.get();
    
    // Skip if not modified
    if (!chunk->isModified()) {
        return true;
    }
    
    // Create directory structure if needed
    std::string filePath = getChunkFilePath(coord);
    std::string dirPath = filePath.substr(0, filePath.find_last_of("/"));
    
    // Create directories using system-specific methods
    // std::filesystem::create_directories(dirPath); // C++17
    // For now, use system("mkdir -p " + dirPath) or similar
    
    // Open file for writing
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Serialize chunk data
    bool success = chunk->serialize(file);
    
    // Remove from dirty list if success
    if (success) {
        m_dirtyChunks.erase(coord);
    }
    
    return success;
}

std::unique_ptr<Chunk> ChunkManager::loadChunk(const ChunkCoord& coord) {
    std::string filePath = getChunkFilePath(coord);
    
    // Open file for reading
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return nullptr; // File not found or can't be opened
    }
    
    // Create new chunk with correct position
    auto chunk = std::make_unique<Chunk>(coord.x * m_chunkSize, coord.y * m_chunkSize);
    
    // Deserialize chunk data
    if (!chunk->deserialize(file)) {
        return nullptr; // Failed to deserialize
    }
    
    return chunk;
}
```

## Optimization Opportunities

1. **Asynchronous Loading**
   - Implement a worker thread for loading/saving chunks
   - Use a priority queue for load requests
   - Add synchronization between main thread and worker thread

2. **Compression**
   - Implement run-length encoding for sparse chunks
   - Use zlib or similar for chunk compression
   - Only compress chunks with lots of empty space

3. **Prefetching**
   - Load chunks in the direction of player movement
   - Predict which chunks will be needed next
   - Preload chunks based on player velocity

4. **Adaptive Chunk Count**
   - Adjust MAX_LOADED_CHUNKS based on available memory
   - Use smaller chunks when memory constrained
   - Load more chunks on high-end systems

## Progress Tracking

| Task | Status | Notes |
|------|--------|-------|
| Create ChunkCoord struct | Not Started | |
| Create ChunkManager class | Not Started | |
| Implement chunk priority | Not Started | |
| Add serialization to Chunk | Not Started | |
| Add deserialization to Chunk | Not Started | |
| Modify World class | Not Started | |
| Update chunk access methods | Not Started | |
| Modify physics update | Not Started | |
| Create TestWorld | Not Started | |
| Implement visualization | Not Started | |
| Test with player movement | Not Started | |
| Async loading/unloading | Not Started | |
| Add compression | Not Started | |
| Optimize serialization | Not Started | |

## Validation Requirements

Before marking this implementation as complete, we must validate:

1. **Functionality Tests**
   - [ ] Can load and save chunks without data loss
   - [ ] Physics works correctly at chunk boundaries
   - [ ] No visible seams between chunks
   - [ ] No performance degradation during chunk loading/unloading

2. **Performance Tests**
   - [ ] Memory usage remains stable regardless of world size
   - [ ] Chunk streaming doesn't cause frame rate drops
   - [ ] Loading/saving operations don't block the main thread
   - [ ] Large worlds work with limited memory

3. **Edge Cases**
   - [ ] Correctly handles very large numbers of chunks
   - [ ] Handles corrupted or missing chunk files
   - [ ] Works during rapid player movement
   - [ ] Can recover from crashes during saving

## References

- Noita GDC Talk: "Exploring the Tech and Design of Noita"
- Implementation based on the algorithm described at 9:53 in the talk
- Chunk size (512x512) and loaded chunk count (12) match Noita's approach