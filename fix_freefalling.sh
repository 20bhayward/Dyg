#\!/bin/bash
# Fix accesses to private m_isFreeFalling vector

# Function to replace direct access with accessor method calls
fix_lines() {
    local file=$1
    local line_num=$2
    local var_name=$3
    
    # Extract the index expression inside the brackets
    local index_expr=$(sed -n "${line_num}p" "$file" | grep -o "chunkBelow->m_isFreeFalling\[.*\]" | sed 's/chunkBelow->m_isFreeFalling\[\(.*\)\]/\1/')
    
    # Create the replacement line
    local new_line=$(sed -n "${line_num}p" "$file" | sed "s/chunkBelow->m_isFreeFalling\[.*\]/chunkBelow->setFreeFalling($index_expr, true)/")
    
    # Replace the line
    sed -i "${line_num}s/.*/$new_line/" "$file"
}

# Find all direct access lines and fix them
grep -n "chunkBelow->m_isFreeFalling\[" src/World.cpp | while read -r line_info; do
    line_num=$(echo "$line_info" | cut -d':' -f1)
    fix_lines "src/World.cpp" "$line_num" "chunkBelow"
done

# Fix chunks of code that check the size before accessing
sed -i 's/if (targetIdx >= 0 && targetIdx < static_cast<int>(chunkBelow->m_isFreeFalling\.size())) {\s*chunkBelow->setFreeFalling/chunkBelow->setFreeFalling/g' src/World.cpp
