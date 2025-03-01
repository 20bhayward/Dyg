#!/bin/bash

# Script to build a simplified Vulkan implementation for PixelPhys2D
set -e

echo "===== Building PixelPhys2D with simplified Vulkan support ====="

# Check for Vulkan SDK
echo "Checking for Vulkan headers and libraries..."
if [ -d "/usr/include/vulkan" ] || [ -d "/usr/local/include/vulkan" ]; then
    echo "Vulkan headers found"
else
    echo "WARNING: Vulkan headers not found. You may need to install the Vulkan SDK."
    echo "On Ubuntu/Debian: sudo apt-get install libvulkan-dev"
    echo "On Fedora: sudo dnf install vulkan-headers vulkan-loader-devel"
    echo "On Arch: sudo pacman -S vulkan-headers"
    echo "Alternatively, download the Vulkan SDK from https://vulkan.lunarg.com/"
fi

# Create a simple test program to verify Vulkan runtime
echo "Creating Vulkan test program..."
mkdir -p vulkan_test
cat > vulkan_test/vulkan_test.cpp << 'EOF'
#include <vulkan/vulkan.h>
#include <iostream>

int main() {
    // Initialize the Vulkan library
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::cout << "Vulkan is available!" << std::endl;
    std::cout << "Found " << extensionCount << " Vulkan extensions" << std::endl;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    
    if (result == VK_SUCCESS) {
        std::cout << "Successfully created Vulkan instance!" << std::endl;
        vkDestroyInstance(instance, nullptr);
        return 0;
    } else {
        std::cerr << "Failed to create Vulkan instance. Error code: " << result << std::endl;
        return 1;
    }
}
EOF

# Compile the test program
echo "Compiling Vulkan test program..."
cd vulkan_test
g++ -o vulkan_test vulkan_test.cpp -lvulkan
if [ $? -eq 0 ]; then
    echo "Vulkan test program compiled successfully!"
    
    # Run the test program
    echo "Running Vulkan test program..."
    ./vulkan_test
    if [ $? -eq 0 ]; then
        echo "Vulkan runtime is working correctly!"
        echo ""
        echo "===== You are now ready to implement Vulkan in the main project ====="
        echo "You have successfully compiled and run a Vulkan test program."
        echo "The Vulkan backend stub implementation is ready in your project."
        echo ""
        echo "Next steps to complete the Vulkan integration:"
        echo "1. Implement the VulkanBackend.cpp with real functionality"
        echo "2. Create GLSL shaders and add a Vulkan shader compiler"
        echo "3. Create Vulkan pipelines for the different rendering passes"
        echo "4. Update the renderer to properly handle Vulkan resource creation"
        echo ""
        echo "You can build the main project with Vulkan support using:"
        echo "  ./build_vulkan.sh"
    else
        echo "ERROR: Vulkan test program failed to run!"
        echo "Your system may not have a Vulkan-compatible GPU or driver."
    fi
else
    echo "ERROR: Failed to compile Vulkan test program!"
    echo "Make sure you have the Vulkan development headers and libraries installed."
fi

cd ..
echo ""
echo "===== Vulkan test completed ====="