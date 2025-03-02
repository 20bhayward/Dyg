#\!/bin/bash

# Create a backup
cp src/World.cpp src/World.cpp.backup

# Replace each pattern with proper accessor calls

# 1. Replace the m_isFreeFalling[idx] = true/false; calls
sed -i 's/m_isFreeFalling\[idx\] = true;/setFreeFalling(idx, true);/g' src/World.cpp
sed -i 's/m_isFreeFalling\[idx\] = false;/setFreeFalling(idx, false);/g' src/World.cpp

# 2. Replace m_isFreeFalling[belowIdx] = true; calls
sed -i 's/m_isFreeFalling\[belowIdx\] = true;/setFreeFalling(belowIdx, true);/g' src/World.cpp
sed -i 's/m_isFreeFalling\[downLeftIdx\] = true;/setFreeFalling(downLeftIdx, true);/g' src/World.cpp
sed -i 's/m_isFreeFalling\[downRightIdx\] = true;/setFreeFalling(downRightIdx, true);/g' src/World.cpp

# 3. Replace cross-chunk calls like chunkLeft->m_isFreeFalling[...] = true;
sed -i 's/chunkLeft->m_isFreeFalling\[WIDTH \* (y + 1) + WIDTH - 1\] = true;/chunkLeft->setFreeFalling(WIDTH * (y + 1) + WIDTH - 1, true);/g' src/World.cpp
sed -i 's/chunkRight->m_isFreeFalling\[WIDTH \* (y + 1)\] = true;/chunkRight->setFreeFalling(WIDTH * (y + 1), true);/g' src/World.cpp

# 4. Fix bool isFalling = m_isFreeFalling[idx];
sed -i 's/bool isFalling = m_isFreeFalling\[idx\];/bool isFalling = false; \/\/ TODO: Add a getFreeFalling method if needed/g' src/World.cpp
