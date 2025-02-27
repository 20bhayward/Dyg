#pragma once

#include <cmath>
#include <functional>

namespace dyg {
namespace util {

/**
 * A simple 3D vector class for positions and coordinates
 */
class Vector3 {
public:
    int x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(int x, int y, int z) : x(x), y(y), z(z) {}

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    bool operator==(const Vector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    int distanceSquared(const Vector3& other) const {
        int dx = x - other.x;
        int dy = y - other.y;
        int dz = z - other.z;
        return dx*dx + dy*dy + dz*dz;
    }

    double distance(const Vector3& other) const {
        return std::sqrt(static_cast<double>(distanceSquared(other)));
    }

    // Hash function for Vector3 to be used as key in unordered_map
    struct Hash {
        std::size_t operator()(const Vector3& v) const {
            // Simple hash combination
            std::size_t h1 = std::hash<int>{}(v.x);
            std::size_t h2 = std::hash<int>{}(v.y);
            std::size_t h3 = std::hash<int>{}(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
};

} // namespace util
} // namespace dyg