#\!/bin/bash

# Create a backup
cp src/World.cpp src/World.cpp.orig

# Fix direct access to private m_isFreeFalling vector
sed -i 's/if (targetIdx >= 0 && targetIdx < static_cast<int>(chunkBelow->m_isFreeFalling\.size())) {//' src/World.cpp
sed -i 's/chunkBelow->m_isFreeFalling\[targetIdx\] = true;/chunkBelow->setFreeFalling(targetIdx, true);/' src/World.cpp
sed -i 's/}//' src/World.cpp
