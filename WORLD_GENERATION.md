Procedural World Generation and Simulation Guide
This guide outlines a step-by-step method to create a 2D physics-based voxel world with multiple biomes, caves, and realistic physics for liquids, gases, and powders. We’ll cover terrain generation (including biomes like grassland, desert, mountains, underground and sky islands), cave formation techniques, fluid dynamics, gas and powder physics, optimization strategies (chunking and multithreading), and interactive world modifications. Throughout, code snippets in C++ are provided, with comments explaining each step, so you can integrate them directly into your project.
Multi-Biome Terrain Generation
A rich world generation uses multiple biomes (surface types and underground zones) created in several passes​
TERRARIA.WIKI.GG
​
TERRARIA.WIKI.GG
. We’ll generate a heightmap for the terrain, assign biomes based on noise functions, and refine the terrain with layers and features.
1. Heightmap Generation with Perlin Noise
Start by generating a heightmap using Perlin (or Simplex) noise to create natural 2D terrain variation. Perlin noise produces smooth hills and valleys by interpolating pseudorandom gradients. You can use an existing noise library or implement a noise function. Below is a simple example using a hypothetical perlinNoise(x, y) function that returns a value in [-1,1]:
cpp
Copy
#include <cmath>
#include <vector>

// World dimensions
const int WORLD_WIDTH = 1024;
const int WORLD_HEIGHT = 256;

// Heightmap array
std::vector<int> heightMap(WORLD_WIDTH);

// Noise parameters
float noiseScale = 0.005f;      // Frequency of the noise (higher = more frequent variation)
int octaves = 4;               // Number of noise octaves for fractal noise
float persistence = 0.5f;      // Amplitude reduction per octave

// Function to generate fractal Perlin noise height at x
float getTerrainHeight(int x) {
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float height = 0.0f;
    for(int o = 0; o < octaves; ++o) {
        float sampleX = x * noiseScale * frequency;
        // perlinNoise2D returns value in [-1,1]
        float noiseVal = perlinNoise2D(sampleX, 0.0f);
        height += noiseVal * amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    // Normalize height to [0, WORLD_HEIGHT)
    height = (height * 0.5f + 0.5f) * (WORLD_HEIGHT * 0.5f);
    return height;
}

// Generate the heightmap
for(int x = 0; x < WORLD_WIDTH; ++x) {
    heightMap[x] = (int) getTerrainHeight(x);
}
In this snippet, we combine multiple noise octaves to produce fractal noise (higher octaves adds more detail at smaller scales). We normalize the resulting value to a usable terrain height. Each x in the world gets a height value. This heightmap alone yields continuous hills and valleys, similar to Terraria’s basic terrain generation​
TERRARIA.WIKI.GG
.
2. Biome Map and Biome Assignment
To create distinct biomes (grassland, desert, snow, etc.), generate a separate biome noise map or use thresholds on the heightmap and world position. For example, use a very low-frequency noise or a gradient to assign biome regions. Terraria’s generator runs dedicated passes for biomes like snow (ice biome), jungle, desert, etc​
TERRARIA.WIKI.GG
, ensuring each appears in the world.Approach: Generate a 2D noise field (biomeNoise[x]) that varies slowly across the world (e.g., frequency 0.0005). Use this value to pick a biome per horizontal location. You can define ranges: e.g., biomeNoise < -0.3 = Desert, -0.3 to 0.2 = Grassland, >0.2 = Jungle (as an example). Additionally, use height: high elevations might become Mountain biome.
cpp
Copy
enum Biome { GRASSLAND, DESERT, MOUNTAIN, SNOW, JUNGLE };

std::vector<Biome> biomeMap(WORLD_WIDTH);

// Low-frequency noise for biomes
float biomeNoiseScale = 0.0005f;
for(int x = 0; x < WORLD_WIDTH; ++x) {
    float n = perlinNoise2D(x * biomeNoiseScale, 42.0f); // 42 as a random offset for variety
    // Assign biome based on noise thresholds and terrain height
    if(heightMap[x] > 0.8f * WORLD_HEIGHT) {
        biomeMap[x] = MOUNTAIN;
    } else if(n < -0.3f) {
        biomeMap[x] = DESERT;
    } else if(n > 0.4f) {
        biomeMap[x] = JUNGLE;
    } else if(n > 0.0f) {
        biomeMap[x] = SNOW;
    } else {
        biomeMap[x] = GRASSLAND;
    }
}
This code considers both noise and height. Mountains are forced in high areas. Otherwise, the noise decides desert vs jungle vs grassland. Adjust thresholds and biome types as needed (you can include more biomes like sky islands or underworld separately).Sky Biomes: To generate floating islands or cloud platforms in the sky, you can perform an additional pass after ground generation. For example, sample a very low-frequency noise at high altitudes to decide placement of floating land. Terraria explicitly generates floating islands in a separate pass​
TERRARIA.WIKI.GG
. You can iterate over the upper portion of the map (say y > 0.8*WORLD_HEIGHT) and carve out island shapes using noise or random ellipses. For simplicity, we skip detailed code, but the idea is to spawn clumps of land at sky level if the noise threshold is met.
3. Terrain Block Layers and Composition
For each column x, and for each y from bottom to top, fill blocks based on the height and biome:
Surface layer: The topmost solid tile at heightMap[x] will be biome-specific (e.g., grass or sand).
Underground layers: Below the surface, fill with dirt, then stone deeper down. Certain biomes replace these: e.g., desert biome might use sand and sandstone; snow biome uses snow blocks on top.
Bedrock/Underworld: At the very bottom region of the world, you might place an indestructible layer or special underworld blocks (lava rock, etc.), especially if creating a lava zone.
We also create special underground biomes (like crystal caves or lava zones) by replacing blocks in certain depth ranges or via noise. For instance, a “lava zone” could be defined for y < 20 (near bottom) where pockets of lava and obsidian appear. A “crystal cave” biome could be inserted at mid-depth by spawning crystal blocks in open cave areas (populated after cave generation).The following code builds the initial terrain blocks after height and biome maps are ready:
cpp
Copy
// Define a simple tile type system
enum TileType { AIR, DIRT, GRASS, SAND, STONE, SNOW_TILE, SANDSTONE, BEDROCK, WATER, LAVA, etc... };

struct Tile { TileType type; };

std::vector<std::vector<Tile>> worldGrid(WORLD_HEIGHT, std::vector<Tile>(WORLD_WIDTH));

// Fill terrain based on height and biome
for(int x = 0; x < WORLD_WIDTH; ++x) {
    int terrainHeight = heightMap[x];
    Biome biome = biomeMap[x];
    for(int y = 0; y < WORLD_HEIGHT; ++y) {
        Tile &tile = worldGrid[y][x];
        if(y > terrainHeight) {
            tile.type = AIR; // above ground is empty
        } else if(y == terrainHeight) {
            // Top surface block
            switch(biome) {
                case DESERT:    tile.type = SAND; break;
                case SNOW:      tile.type = SNOW_TILE; break;
                case MOUNTAIN:  tile.type = STONE; break;
                case JUNGLE:    // jungle top can be grass as well
                case GRASSLAND: tile.type = GRASS; break;
            }
        } else {
            // Below the top surface
            int depth = terrainHeight - y;
            if(depth < 4) {
                // Just below surface: dirt/sand/snow depending on biome
                if(biome == DESERT)      tile.type = SAND; 
                else if(biome == SNOW)   tile.type = SNOW_TILE;
                else                     tile.type = DIRT;
            } else if(y < 0.1 * WORLD_HEIGHT) {
                // Bottom 10% of world: make bedrock or underworld
                tile.type = BEDROCK;
            } else {
                // Default underground material
                if(biome == DESERT) tile.type = SANDSTONE;
                else                tile.type = STONE;
            }
        }
    }
}
This creates a stratified world:
Grassland/Jungle: grass top, dirt beneath, then stone.
Desert: sand top and a layer of sand, then sandstone as rock.
Snow: snow tiles at surface, then dirt or stone below.
Mountain: stone at surface (no soil) due to height, and stone below.
Bedrock at the bottom 10% ensures a solid base or could be used for a lava underworld layer.
You can further refine layers:
Add patches of ores, clay, or silt by random chance in stone layers (Terraria adds “shinies” like ores in a pass​
TERRARIA.WIKI.GG
).
Place specific biome features: e.g., cactus in deserts, or grass decoration in grasslands (these can be done after world gen as separate decoration passes).
4. Refinement Passes and Natural Features
Using multiple passes to refine the world leads to more natural results​
TERRARIA.WIKI.GG
. After initial block placement:
Smoothing: You can smooth out jagged terrain edges. For example, if a column is much taller than neighbors, reduce its height slightly (or fill some blocks above as floating dirt to create overhangs).
Beaches: At edges of water bodies or oceans, replace some dirt with sand. Terraria explicitly has a pass for beaches​
TERRARIA.WIKI.GG
.
Vegetation: Grow grass on dirt that is directly beneath a grass block (spreading grass mechanics), or add trees. Terraria’s generator spreads grass and places living trees in later passes​
TERRARIA.WIKI.GG
​
TERRARIA.WIKI.GG
.
Floating Islands: If not added yet, you can carve small floating land pieces in the sky with their own mini-biome (e.g., some grass or a rare resource on them).
Underground features: Place underground lakes by carving out some areas and filling with water (or mark them for water insertion in a later fluid simulation step), add mushroom patches or special biome pockets​
TERRARIA.WIKI.GG
.
All these steps can iterate over the world and adjust blocks. By structuring world gen in distinct passes, you ensure features overlay correctly (e.g., caves cut through terrain after terrain exists, trees sit on top of ground, etc.). Terraria uses dozens of passes for various features​
TERRARIA.WIKI.GG
​
TERRARIA.WIKI.GG
 – you can choose what your game needs.
Cave Generation (Tunnels and Caverns)
Caves make the underground interesting. We’ll implement two complementary techniques: Perlin Worms for long winding tunnels, and Cellular Automata for wide caverns​
TERRARIA.WIKI.GG
. Using a hybrid approach can produce natural-looking cave systems.
1. Perlin Worm Tunnels
Perlin worms are essentially random walkers that carve tunnels, with their direction influenced by Perlin noise for smooth turns. This creates long winding caves (much like snake-like tunnels). Unlike pure Perlin noise which can create continuous caves with no dead-ends, Perlin worms can branch and terminate, giving more variety.Algorithm:
Start a number of “worms” at various initial underground positions (e.g., random X on the map, Y around a certain depth).
For each worm, simulate a stepwise movement:
At each step, carve a empty space (turn the solid tiles to air) in a small radius (tunnel thickness).
Use a noise function to smoothly alter the worm’s direction as it moves. For example, sample dirNoise = perlinNoise2D(x * dScale, y * dScale) to get an angle offset.
Advance the worm by one tile in the current direction (which gradually turns based on noise).
Continue for a set length or until it goes out of bounds or hits an open area.
Optionally, randomize tunnel radius or create branches by occasionally spawning a new worm with a different direction.
Below is a simplified implementation of a single worm tunneling through the worldGrid:
cpp
Copy
#include <random>
std::default_random_engine rng(seed);
std::uniform_real_distribution<float> angleDist(-M_PI, M_PI);

// Carve a circular opening at (cx, cy) with given radius
auto carveCircle = [&](int cx, int cy, int radius) {
    for(int dx = -radius; dx <= radius; ++dx) {
        for(int dy = -radius; dy <= radius; ++dy) {
            if(dx*dx + dy*dy <= radius*radius) {
                int nx = cx + dx;
                int ny = cy + dy;
                if(nx >= 0 && nx < WORLD_WIDTH && ny >= 0 && ny < WORLD_HEIGHT) {
                    worldGrid[ny][nx].type = AIR;
                }
            }
        }
    }
};

// Perlin Worm parameters
int startX = WORLD_WIDTH / 2;
int startY = (int)(0.6 * WORLD_HEIGHT); // start somewhat underground
float angle = angleDist(rng);           // initial direction
float stepLength = 1.0f;
int tunnelRadius = 3;
int maxLength = 2000;                   // max steps for the worm
float turnFrequency = 0.1f;             // noise frequency for direction changes

// Worm carving loop
float x = startX;
float y = startY;
for(int i = 0; i < maxLength; ++i) {
    // Carve current position
    carveCircle((int)x, (int)y, tunnelRadius);
    // Update direction angle using Perlin noise
    float noiseVal = perlinNoise2D(x * turnFrequency, y * turnFrequency);
    angle += (noiseVal * 2 - 1) * 0.3f;  // small turn based on noise (scaled by 0.3 radians)
    // Move forward
    x += cos(angle) * stepLength;
    y += sin(angle) * stepLength;
    // If out of world bounds, stop
    if(x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT) break;
}
This code sends a worm starting at startX, startY and carves a tunnel. The direction gradually changes according to Perlin noise sampled at the worm’s position (so the path is semi-random but smooth). We carve a radius-3 tunnel; adjust tunnelRadius for thicker or thinner tunnels. You would run this for many worms:
cpp
Copy
for(int w = 0; w < 50; ++w) {
    // pick random start positions and call a similar loop to carve tunnels
}
Choose start positions in the dirt/rock layers (not too shallow, not too deep if you want open caves accessible to the surface). Also consider starting some worms at the sides of the map to simulate cave entrances from cliffs.Branching: You can occasionally spawn a new worm in a different direction to simulate branches. For example, every 100 steps, with some probability, spawn a new worm from the current location with angleDist(rng) direction. This creates a network of tunnels.
2. Cellular Automata Caverns
While worms create linear tunnels, cellular automata (CA) can create wide open caverns and irregular cave rooms​
TERRARIA.WIKI.GG
. The classic approach is:
Take a region of the map (or the whole underground) and fill it randomly with walls and empty spaces (e.g., 45% chance walls).
Apply an iterative rule to smooth it: in each iteration, for each cell, look at its 8 neighbors:
If a cell is wall and has 4 or more neighboring walls, it stays wall.
If a cell is empty and has 5 or more neighboring walls, it becomes wall.
Otherwise, it becomes empty​
ROGUEBASIN.COM
.
Perform ~4-6 iterations of this. The result will be blobs of walls and open spaces forming caves.
This rule (often called “4-5 rule”) causes areas to solidify or open up based on neighbor majority​
ROGUEBASIN.COM
. The outcome is cave-like: small random holes close up and large areas become smoother. The edges of caverns remain rough, giving a natural look.Implementation:
cpp
Copy
int width = WORLD_WIDTH;
int height = WORLD_HEIGHT / 2;  // apply CA to bottom half for caves, for example
std::vector<std::vector<int>> cellMap(height, std::vector<int>(width));

// 1. Initial random fill
float initialWallProbability = 0.45f;
std::uniform_real_distribution<float> dist(0.0f, 1.0f);
for(int y = 0; y < height; ++y) {
    for(int x = 0; x < width; ++x) {
        if(dist(rng) < initialWallProbability) cellMap[y][x] = 1; // wall
        else cellMap[y][x] = 0; // empty
    }
}

// 2. Iterative smoothing
auto countWallNeighbors = [&](int x, int y) {
    int count = 0;
    for(int ny = y-1; ny <= y+1; ++ny) {
        for(int nx = x-1; nx <= x+1; ++nx) {
            if(nx < 0 || nx >= width || ny < 0 || ny >= height) {
                count++; // treat out-of-bounds as wall
            } else if(!(nx == x && ny == y) && cellMap[ny][nx] == 1) {
                count++;
            }
        }
    }
    return count;
};

int iterations = 5;
for(int it = 0; it < iterations; ++it) {
    std::vector<std::vector<int>> newMap = cellMap;
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            int neighbors = countWallNeighbors(x, y);
            if(cellMap[y][x] == 1) {
                // Wall remains if 4 or more neighbors are walls
                newMap[y][x] = (neighbors >= 4 ? 1 : 0);
            } else {
                // Empty becomes wall if 5 or more neighbors are walls
                newMap[y][x] = (neighbors >= 5 ? 1 : 0);
            }
        }
    }
    cellMap.swap(newMap);
}
After this, cellMap holds a cave layout where 0 = cave (empty) and 1 = solid. We then apply it to our world:
cpp
Copy
// 3. Carve out caves in the worldGrid based on cellMap
int startY = WORLD_HEIGHT/2; // where we started the CA (e.g., mid-depth)
for(int y = 0; y < height; ++y) {
    for(int x = 0; x < width; ++x) {
        if(cellMap[y][x] == 0) {
            worldGrid[startY + y][x].type = AIR;
        }
    }
}
This will carve large caverns and irregular spaces in the underground. You might run CA in distinct layers (e.g., one for the dirt layer with smaller caves​
TERRARIA.WIKI.GG
 and one deeper for large caves​
TERRARIA.WIKI.GG
). In Terraria’s generation, there are passes for “small holes,” “small caves,” and “large caves” at different depths​
TERRARIA.WIKI.GG
. You can simulate that by varying the parameters or size of regions where you apply CA (for instance, shallower caves might start with lower wall probability to create more tunnels, deeper caves higher probability for huge caverns).
3. Hybrid Cave System and Final Touches
Combine worms and CA: run the cellular automata first to get big cave rooms, then run worm tunnels to ensure connectivity between them (the tunnels can break through walls to link isolated cave pockets). Alternatively, carve worms first and then CA, but CA might fill parts of tunnels. A good approach is:
Use CA to carve broad caverns.
Use Perlin worms as tunnels interconnecting those caverns and linking to the surface.
After carving, consider a connectivity check: Use a flood-fill on the empty cells to see if there are any isolated air pockets unreachable from others. If so, you can carve an extra tunnel to connect them to the main cave system (or to the surface). This ensures the cave network is navigable as one large system rather than disjoint rooms.You can also add vertical shafts or ravines by carving straight down tunnels in some places (for variety). And an underworld open area (Terraria creates a huge open lava layer at the bottom​
TERRARIA.WIKI.GG
) can be made by forcing a large open region in the deepest part of the map.
Water and Fluid Simulation
With the static world in place, we add dynamic elements: fluids like water, lava, oil, etc. Simulating fluids in a cellular environment involves moving liquid cells according to gravity and pressure rules. We aim for dynamic liquid behavior – water flows into caves, forms lakes, and different fluids interact (e.g., oil floating on water). We’ll use a cellular automaton approach for fluids, which is efficient in a pixel simulation (Noita uses a similar per-pixel simulation for liquids) and avoid complicated physics equations.Key concepts for fluid simulation:
Each fluid cell has a density relative to others. Heavier fluids sink, lighter fluids rise above heavier ones​
REDDIT.COM
​
REDDIT.COM
.
Fluids seek to move downwards (gravity) or spread out if constrained.
We simulate in discrete time steps, updating fluid positions.
We maintain a grid of tile types (worldGrid) which now includes fluid types (e.g., TileType::WATER, LAVA, etc.). A fluid cell moves to an adjacent cell if that cell is open (air) or contains a fluid of lower density (meaning this fluid can push the other out of the way by sinking below it).Density and fluid types: Assign each fluid a density value (e.g., Water = 1.0, Oil = 0.8 (lighter), Lava = 2.0 (heavier than water)). We’ll use these to decide movement priority. For example, oil will float above water because its density is lower​
REDDIT.COM
. We also assign behavior like viscosity (lava moves slower, honey could be very viscous, etc.) by controlling how frequently or how far it can move per update.We will implement the fluid simulation with a step-by-step update loop. A common approach is a bottom-up sweep for gravity-driven materials​
BLOG.MACUYIKO.COM
. However, when multiple fluids and gases are involved, a single pass order can cause bias (e.g. water flowing predominantly in one direction if always checked left-to-right first)​
BLOG.MACUYIKO.COM
. To avoid bias, we can randomize some decisions or use a checkerboard update pattern.For simplicity, we’ll demonstrate a straightforward approach and then note optimizations:
cpp
Copy
struct FluidProperties {
    float density;
    bool isLiquid;
    bool isGas;
    // You can add viscosity or other properties as needed
};
std::map<TileType, FluidProperties> fluids;
fluids[WATER] = { 1.0f, true, false };
fluids[OIL]   = { 0.8f, true, false };
fluids[LAVA]  = { 2.0f, true, false };
// (We treat air as a gas with density ~0, but it's just empty space in grid)

bool updated[WORLD_HEIGHT][WORLD_WIDTH] = {{false}}; // track if moved this step

// Utility lambda to swap two cells in the worldGrid
auto swapCells = [&](int x1, int y1, int x2, int y2) {
    std::swap(worldGrid[y1][x1].type, worldGrid[y2][x2].type);
};

// Single simulation tick for fluids
for(int y = WORLD_HEIGHT-1; y >= 0; --y) {        // iterate bottom-up
    for(int x = 0; x < WORLD_WIDTH; ++x) {
        updated[y][x] = false;
    }
}
for(int y = WORLD_HEIGHT-1; y >= 0; --y) {        // bottom-up for liquids (gravity)
    for(int x = 0; x < WORLD_WIDTH; ++x) {
        TileType t = worldGrid[y][x].type;
        if(fluids.count(t) == 0) continue;        // skip if not a fluid cell
        if(updated[y][x]) continue;              // already moved this tick
        FluidProperties prop = fluids[t];
        if(prop.isLiquid) {
            // Try to move down or diagonally down
            int ny = y + 1;
            if(ny < WORLD_HEIGHT) {
                // Check directly below
                if(worldGrid[ny][x].type == AIR) {
                    swapCells(x, y, x, ny);
                    updated[ny][x] = true;
                    continue; // moved, go to next cell
                }
                // If below is a lighter fluid (lower density), displace it (sink)
                TileType belowType = worldGrid[ny][x].type;
                if(fluids.count(belowType) && fluids[belowType].density < prop.density && !updated[ny][x]) {
                    // swap with the lighter fluid below
                    swapCells(x, y, x, ny);
                    updated[ny][x] = true;
                    continue;
                }
            }
            // If we reach here, down is blocked or out of bounds
            // Try down-left or down-right flow
            int nx1 = x - 1;
            int nx2 = x + 1;
            bool moved = false;
            if(ny < WORLD_HEIGHT && nx1 >= 0 && worldGrid[ny][nx1].type == AIR) {
                swapCells(x, y, nx1, ny);
                updated[ny][nx1] = true;
                moved = true;
            } else if(ny < WORLD_HEIGHT && nx2 < WORLD_WIDTH && worldGrid[ny][nx2].type == AIR) {
                swapCells(x, y, nx2, ny);
                updated[ny][nx2] = true;
                moved = true;
            } else {
                // Sideways spread on same level (if both below left and below right are filled)
                if(nx1 >= 0 && worldGrid[y][nx1].type == AIR) {
                    swapCells(x, y, nx1, y);
                    updated[y][nx1] = true;
                    moved = true;
                } else if(nx2 < WORLD_WIDTH && worldGrid[y][nx2].type == AIR) {
                    swapCells(x, y, nx2, y);
                    updated[y][nx2] = true;
                    moved = true;
                }
            }
            if(moved) continue;
        }
        // (Gases will be handled in a separate loop top-down)
    }
}
// Now handle gases in a top-down manner:
for(int y = 0; y < WORLD_HEIGHT; ++y) {
    for(int x = 0; x < WORLD_WIDTH; ++x) {
        TileType t = worldGrid[y][x].type;
        if(fluids.count(t) == 0) continue;
        if(updated[y][x]) continue;
        FluidProperties prop = fluids[t];
        if(prop.isGas) {
            // Gas rises
            int ny = y - 1;
            if(ny >= 0) {
                if(worldGrid[ny][x].type == AIR) {
                    swapCells(x, y, x, ny);
                    updated[ny][x] = true;
                    continue;
                }
                TileType aboveType = worldGrid[ny][x].type;
                if(fluids.count(aboveType) && fluids[aboveType].density > prop.density && !updated[ny][x]) {
                    // A heavier fluid above (meaning current gas is below a fluid) - swap to rise up
                    swapCells(x, y, x, ny);
                    updated[ny][x] = true;
                    continue;
                }
            }
            // Try up-left or up-right
            int nx1 = x - 1, nx2 = x + 1;
            bool moved = false;
            if(ny >= 0 && nx1 >= 0 && worldGrid[ny][nx1].type == AIR) {
                swapCells(x, y, nx1, ny);
                updated[ny][nx1] = true;
                moved = true;
            } else if(ny >= 0 && nx2 < WORLD_WIDTH && worldGrid[ny][nx2].type == AIR) {
                swapCells(x, y, nx2, ny);
                updated[ny][nx2] = true;
                moved = true;
            } else {
                // Lateral diffusion on same level for gas
                if(nx1 >= 0 && worldGrid[y][nx1].type == AIR) {
                    swapCells(x, y, nx1, y);
                    updated[y][nx1] = true;
                    moved = true;
                } else if(nx2 < WORLD_WIDTH && worldGrid[y][nx2].type == AIR) {
                    swapCells(x, y, nx2, y);
                    updated[y][nx2] = true;
                    moved = true;
                }
            }
            if(moved) continue;
        }
    }
}
This code covers basic liquid and gas movement in one tick:
Liquids (bottom-up loop): We iterate from bottom to top​
BLOG.MACUYIKO.COM
 so that a fluid doesn’t immediately fall multiple cells in one tick (since the one below will be processed later). A water cell tries to move down; if directly below is empty, it falls. If below is a lighter fluid (say water above oil), it swaps (water sinks, oil rises)​
REDDIT.COM
. If down is blocked, it tries down-left or down-right (flow around obstacles). If those are blocked too, it can move sideways on the same level (this simulates fluid spreading out on a flat surface). We mark cells in updated[][] when they move, to avoid moving them again in the same tick or being moved twice​
STACKOVERFLOW.COM
​
STACKOVERFLOW.COM
.
Gases (top-down loop): Gases (like smoke, steam) are treated similarly but with gravity inverted. We iterate top-down so a gas cell moves up without a newly moved gas above it immediately moving again (by analogy to bottom-up for liquids). A gas tries to go up; if above is occupied by a heavier fluid (essentially any non-gas), it will swap (so the gas rises through liquids or air)​
BLOG.MACUYIKO.COM
. If up is blocked, it tries up-left or up-right, otherwise spreads sideways.
We separated the loops for clarity. In an optimized implementation, you might integrate them, but you have to be careful with order. Another approach is to use a double buffer: compute the new grid from the old in one sweep so that movement decisions aren’t immediately affecting neighbors. Using a double buffer (or “ping-pong” buffers) inherently avoids bias and is thread-safe​
GAMEDEV.STACKEXCHANGE.COM
​
GAMEDEV.STACKEXCHANGE.COM
, but then you must resolve conflicts where two fluids try to flow into the same cell. A simple rule in conflict could be to prefer the heavier fluid to occupy the cell (or choose randomly one to move and leave the other).Distinct Fluid Behaviors:
Water: medium density (baseline), flows quickly.
Lava: high density, but also very viscous. We can simulate viscosity by limiting how often it moves. For example, update lava cells only every second or third tick, or require a random chance to move, to make it flow slowly.
Oil: low density (floats on water), and possibly slightly viscous. It will sit on top of water due to our density swap logic.
Other fluids: e.g., an acid could be like water in movement. Honey (like in Terraria) would be extremely viscous liquid (almost solid).
Interaction with solids: If water touches lava, you might form stone (solidify the lava) – this is a gameplay effect you can implement by checking neighbors of different types and changing types (not covered here, but possible to add as a rule after movement).
Fluid pressure: Our simulation doesn’t explicitly calculate pressure, but liquids will naturally equalize to some extent by flowing. Full real-life pressure (pushing up on the other side of a U-shaped container) is not simulated; Noita, for example, does not simulate fluid pressure except in enclosed tanks​
REDDIT.COM
. If you want more realism, you could incorporate a compressibility as in a known method where lower cells hold slightly more fluid (pressure) and push upwards if overfull​
W-SHADOW.COM
, but this adds complexity. The current approach is simpler and “good enough” for games – it produces visually believable behavior of liquids pooling and seeking levels.
Stability: The above algorithm can result in some jitter (fluids oscillating) if confined. To mitigate:
If a fluid has no place to go (surrounded by equal or heavier fluids/solids), it should stay put. Our rules naturally enforce that.
Randomize the order of checking left vs right when spreading to avoid bias to one side. In code, we always checked left then right; this will cause a slight left-bias. You can randomize per cell, or alternate the order each frame, or use a horizontal sweep in opposite directions on alternate ticks.
updated[][] ensures one move per cell per tick, preventing a fluid from falling multiple cells in one update pass (which could happen if not careful, leading to tunneling through floor).
You would call this fluid update logic every game tick (or maybe every few ticks, depending on desired simulation speed vs frame rate).After each fluid update, you should also handle settling: if a fluid has moved and now rests on something, you might mark it as “settled.” For optimization, settled fluids (like a lake that’s at rest) could be skipped in calculations until disturbed. Terraria’s world gen even has a "Settle Liquids" step after generation​
TERRARIA.WIKI.GG
, but in our dynamic simulation we settle at runtime instead of generation.
Gas Simulation (Air, Smoke, Steam)
Gas simulation is handled in the code above concurrently with fluids by marking fluid properties as isGas. We gave an example for smoke/steam. Gases have very low density, causing them to rise above all liquids. They also diffuse: since they have no strong downward pull, they spread out more evenly.Important considerations for gases:
Containment: If gas is in an enclosed space (e.g., smoke in a sealed room), it will rise to the top of that room and then accumulate. Our simulation will make the gas gather at the highest available cells. It won’t equalize pressure perfectly, but it will fill the top and then start filling downward once the top is full.
Dispersion: You might implement that gas slowly disappears or loses density (simulating dissipation) if you want smoke to clear over time. This can be done by converting some gas cells to air periodically (or having a lifetime on gas particles).
Air: We usually consider normal air as the baseline “empty” cell. You don’t explicitly simulate air, but you might simulate wind or pressure differences if needed by moving air around. That’s advanced; in most games, air is just the absence of other materials. If you want explosions to create pressure waves, you could push air (as a series of gas particles) outwards.
Our gas update logic in code was similar to liquids but inverted. Just ensure to run gas after liquids (or simultaneously with a robust method) so that the interactions (like water pushing air or air bubbling up through water) are handled. In our approach:
Heavy liquids sinking into gas was handled in the liquid loop by swapping with lighter fluid below​
BLOG.MACUYIKO.COM
.
Gas rising through liquids was handled in the gas loop by swapping with heavier fluid above.
This two-sided handling ensures, for example, an underwater pocket of air will rise up: the water above sees air (lighter) below and will drop into it, and the gas loop will then catch the air moving up into where the water was – effectively the air bubble goes up and water goes down to fill its place, which looks like bubbling.
Powder and Solid Particle Physics
Powders (sand, dirt particles, ash, etc.) behave like a solid that can still flow under gravity – essentially a granular fluid. In our simulation, we treat them separately from liquids for realism:
A sand pixel falls straight down if below is empty (air or gas).
If directly below is blocked (by solid or liquid), but below-left or below-right is empty, it will slide down diagonally​
BLOG.MACUYIKO.COM
.
If all downward directions are blocked, it comes to rest and stays there (forming a pile).
This is exactly the rule for “falling sand” in cellular automata, often implemented with a similar bottom-up scan​
BLOG.MACUYIKO.COM
. We can integrate powders into our update loop as a type of material with high density and zero fluidity sideways when on flat ground.In the code, we can handle powder in the same loop as liquids (since they are gravity-affected), but we treat them as non-spreading:
We would give them a FluidProperties entry with isLiquid=true (to use the gravity logic) but maybe treat sideways movement differently.
Alternatively, handle powders in their own section for clarity:
cpp
Copy
for(int y = WORLD_HEIGHT-1; y >= 0; --y) {
    for(int x = 0; x < WORLD_WIDTH; ++x) {
        if(worldGrid[y][x].type == SAND && !updated[y][x]) {
            int ny = y + 1;
            if(ny < WORLD_HEIGHT && worldGrid[ny][x].type == AIR) {
                swapCells(x, y, x, ny);
                updated[ny][x] = true;
            } else if(ny < WORLD_HEIGHT) {
                // Check diagonal down left/right for empty
                int nx1 = x - 1, nx2 = x + 1;
                if(nx1 >= 0 && worldGrid[ny][nx1].type == AIR) {
                    swapCells(x, y, nx1, ny);
                    updated[ny][nx1] = true;
                } else if(nx2 < WORLD_WIDTH && worldGrid[ny][nx2].type == AIR) {
                    swapCells(x, y, nx2, ny);
                    updated[ny][nx2] = true;
                }
            }
        }
    }
}
This will make sand fall straight down or diagonally, but unlike water, it doesn’t move sideways along a flat surface because we omitted that part for powder. So sand will pile up. For example, if sand falls on top of another sand, it may accumulate and form a slope at 45° due to the diagonal rule, which is a common behavior in such simulations.Interaction with liquids: If sand falls into water, ideally it should pass through and settle below the water (since sand is solid and heavier than water). Our logic will handle some of that: water sees sand above it as heavier? Actually in our code, we didn’t explicitly handle a solid above fluid. We could extend the liquid logic: if a liquid cell has a solid above that can fall, maybe do nothing special – instead handle it in the sand logic: if sand has water below, treat water as empty for falling purposes (and maybe push the water up). Implementing full two-way solid-liquid interaction is complex. A simpler hack: allow sand to replace water: if sand is above water, swap them (sand falls into water, water rises). This can simulate sediment falling through water. But one must be careful to avoid infinite oscillation (perhaps once swapped, water will flow away or sand will settle).Due to complexity, many games simply treat sand as not mixing with water except that it will displace water when it lands (water will flow out to sides). You can choose to implement detailed interactions or keep it straightforward.Other powders: You might have powders like salt or dust that behave similarly to sand. Just treat them as sand type in simulation terms (maybe different color or density if needed).Solids: Most world tiles are static solid blocks (stone, dirt when anchored, etc.) that do not move. We typically do not simulate rigid body physics for static terrain (that’s what makes it a voxel terrain game rather than a rigid body physics game). However, if you want destructible environment with physics (like chunks of terrain falling off), that’s a whole different approach (converting voxels to rigid bodies when they detach). Noita does something like turning a cluster of pixels into a single rigid body if they are grouped​
GAMEDEV.STACKEXCHANGE.COM
, but this is advanced. In our simulation, we assume terrain blocks stay fixed unless explicitly changed (mined or exploded by player, at which point they might become dynamic powder or just disappear).
Optimization Strategies
Simulating a large world with potentially millions of cells can be very CPU-intensive, especially with physics for fluids/gases. We need to optimize by updating only what’s necessary and using efficient data structures and algorithms.
1. Chunk-Based World and Active Regions
Chunk the world into regions (for example, 64x64 or 128x128 tiles per chunk). This serves multiple purposes:
Dynamic Loading: You don’t have to keep the entire world in memory or simulate it if the player is far away. You can load/generate chunks around the player and unload far ones (like Minecraft/Terraria do).
Localized Updates: Instead of looping over the entire world for simulation, loop over only the chunks (and within them, cells) that potentially need updates. If a chunk has no dynamic elements (no fluids moving, etc.), you can skip it.
Parallelism: Chunks provide a natural unit for multithreading.
Maintain a list or map of active chunks that currently contain dynamic behavior (e.g., fluids, falling sand, or maybe entities). Update only those each frame. If a fluid flows into a neighboring chunk, mark that neighbor active. If a chunk becomes completely static (all fluids settled, no particles moving), you could temporarily mark it inactive until something changes (like new fluid poured in).Additionally, track active cells or a bounding box of activity (“dirty rects”) within each chunk​
GAMEDEV.STACKEXCHANGE.COM
. For example, if you have one puddle sloshing in a corner of the chunk, you don't need to iterate over every cell in the chunk – just iterate over cells in and around the puddle. You can maintain a set of coordinates that need updating, though managing this can have overhead if there are many active cells. A compromise is to keep a bitmap of active cells or use spatial hashing to only update non-empty cells​
GAMEDEV.STACKEXCHANGE.COM
.Example optimization: In our fluid loop, instead of double nested loops over the whole map, have a container of coordinates of fluid cells and iterate that. However, since fluids move, this list changes every frame. One method is to swap between two sets: one for current positions and build the next set of positions as fluids move.
2. Multithreading the Simulation
Take advantage of multiple CPU cores by updating multiple chunks in parallel. Since each chunk’s update mostly involves its own cells, we can do this safely with some care:
Neighboring chunks can have interactions at the border (e.g., water flowing from one chunk to the next). If two adjacent chunks update at the same time, a cell at the edge might try to move into the neighbor which is simultaneously being updated. To avoid race conditions, one approach is updating in a checkerboard pattern​
GAMEDEV.STACKEXCHANGE.COM
: treat chunks like black or white squares of a chessboard and update all black chunks in one parallel step, then all white chunks in the next. Adjacent chunks are of opposite color, so they won’t be updated simultaneously, preventing conflicts. This is a technique reportedly used by Noita’s developers​
GAMEDEV.STACKEXCHANGE.COM
.
Alternatively, use fine-grained locks or atomic operations for cells, but that’s complex and can hurt performance. The checkerboard (or more generally, graph coloring of the update grid) is lock-free.
If chunk size is large or you don’t want to delay neighbors, you can subdivide further: e.g., split each chunk’s internal update into two passes (first half of columns then second half).
Using C++ threads or a thread pool, you can dispatch chunk updates. For instance:
cpp
Copy
// Pseudocode for multithreading chunk updates
std::vector<std::thread> workers;
for(Chunk* chunk : activeChunksBlack) {
    workers.emplace_back([&]() {
        chunk->updatePhysics(); // runs the fluid/powder update as described, but confined to chunk area
    });
}
// Join threads
for(auto &th : workers) th.join();
workers.clear();
// Next, do the same for activeChunksWhite
In updatePhysics() for a chunk, you must consider cells at the chunk boundary: a fluid at the edge might move into a neighboring chunk. One way is to allow it to move and mark the neighbor chunk active and perhaps handle that neighbor’s edge cell immediately. Or you can include a one-tile overlap where each chunk reads neighbor cells. A simpler approach: after all chunk updates, do a border resolution pass – check all chunk borders for fluids that should flow across and move them. This could be done sequentially after parallel chunk updates.Thread pool: In practice, spawning threads every frame for each chunk is expensive. Instead, use a fixed thread pool where each thread continually picks up chunk tasks. Libraries like OpenMP can also simplify parallel loops (e.g., #pragma omp parallel for over chunks).
3. Data Structure and Memory Optimizations
Use cache-friendly storage. Our worldGrid is a 2D vector of structs. This is fine, but ensure Tile is simple (in our case just an enum, which likely fits in an int). This keeps memory contiguous. If you had a complex object per tile, it would bloat memory. Aim for a compact representation (many games use a single byte or short per cell to indicate material). Noita’s engine uses a 1-byte material index per pixel for efficiency​
GAMEDEV.STACKEXCHANGE.COM
.
If you simulate water volumes (floats for water amount), that’s another array of floats – still fine if contiguous.
Avoid frequent allocation. Reuse buffers like the updated[][] array rather than allocating each frame.
Quadtrees or spatial hashing for active cells can help skip large empty regions​
GAMEDEV.STACKEXCHANGE.COM
, but if your chunking already divides space, that might suffice.
4. Skipping Idle Simulation
Not everything needs to be updated every tick. As mentioned, if a region has settled fluids or no moving parts, you can skip simulating it until something changes. This is the concept of dirty regions​
GAMEDEV.STACKEXCHANGE.COM
. For example:
A pool of water at rest doesn’t need to be recalculated until something disturbs it (like more water added or an opening created in its container).
We can have a flag for each fluid cell or each chunk indicating if it moved last tick or if any neighbor moved. If not, increment a counter – after, say, 60 frames of no movement, mark it as settled and remove from active list.
If a new event happens (like player digs a block and the water can flow), mark it active again.
The Noita tech talk summary suggests “don’t simulate if you can ignore it” – skipping anything outside player’s view or otherwise unimportant​
GAMEDEV.STACKEXCHANGE.COM
. In a single-player game, you might not need to simulate fluids that are far off-screen; you can pause them and only resume when the player comes near (the player might never know the difference). If continuity is needed (e.g., if you want an off-screen machine or flowing water to still operate), you could simulate at a lower frequency for far areas.
5. Optimizing the Update Algorithm
The algorithm we provided for fluids is simple and clear but not the most optimized:
It checks each cell even if it’s empty or a solid. We can change it to iterate only over fluid cells. Maintain lists of coordinates for each material type that needs updating.
Use bitwise operations or SIMD if operating on large chunks of data for things like neighbor counts (in CA) or for checking uniform areas.
GPU acceleration: Some falling sand simulations use shaders (OpenCL/CUDA) to update many cells in parallel on GPU​
GAMEDEV.STACKEXCHANGE.COM
. This can yield huge speedups, but it’s complex to integrate with game logic. For a large world, CPU multithreading might suffice.
6. Example: Dirty Region Update
To illustrate an optimization, suppose we keep a list activeFluids of coordinates of fluid cells that moved or might move. After each simulation tick, we compile a new list of active cells for next tick (any cell that just moved or its neighbor). We then iterate next frame only over those. If that list becomes empty (no fluid is moving), we can stop until triggered by some event (like adding new fluid).Pseudo-code:
cpp
Copy
std::vector<Point> newActive;
for(Point p : activeFluids) {
    // perform update logic for cell at p (similar to above rules)
    // if cell at p or any neighbor moves, add their coordinates to newActive
}
activeFluids = newActive;
This way, if fluids settle, activeFluids will shrink and eventually be empty.Be careful: if you remove a cell from the active list too early (while it’s still affected by e.g. neighboring pressure), you could miss a movement. Usually, check neighbors and maybe keep a fluid marked active for an extra frame or two after it last moved to be sure.
Interactivity and Real-Time Updates
One hallmark of games like Noita and Terraria is a modifiable, destructible world. Our system must handle changes on the fly:
Mining/Digging: Removing a solid tile turns it to air. Our physics should then naturally let any fluid or sand above fall down. For example, if a player digs a block at the bottom of a water pool, the water will flow into the new hole. Implementation: set worldGrid[y][x].type = AIR and mark that (and neighbors) as active for simulation. If the removed block was supporting others (like sand above), those become active due to gravity (sand will fall next tick).
Placing Blocks: If a player places a block (dirt, stone, etc.), it could displace fluid. In our simple simulation, a newly placed solid in a fluid will just occupy that cell, effectively deleting the fluid there. (You might spawn some fluid to spill out around it if you want more realism, or forbid placement if occupied by fluid.) Ensure to mark the area so that if fluid was around, it updates (fluids next to the new block might need to find new paths).
Explosions: An explosion can instantly remove (or convert) a group of tiles. This is essentially a batch of dig operations in a radius. For each destroyed tile, set to AIR. You might also spawn special particles (debris) or apply forces to nearby fluids (pressure wave). A straightforward approach is to clear the tiles and then let gravity and fluid simulation handle the rest. Explosion in liquid will create a cavity that surrounding liquid rushes into. Explosion in sand will cause a sand collapse. After an explosion, mark the entire affected chunk as active, since many changes happened at once.
Building structures: Similar to placing blocks, just ensure any dynamic elements are updated around the new placements.
Fire and gases: If you implement fire, burning objects might create gas (smoke) or consume oxygen (air). You would spawn smoke gas cells into the world, and those automatically rise. Likewise, an underwater explosion might create expanding gas (bubbles).
Player Interaction with Fluids: If the player character is in water, typically games apply buoyancy or swimming mechanics, but that’s on the game logic side. From the simulation perspective, the player is just an obstacle that water can’t occupy. You may represent the player as occupying some tiles (or continuously check the player’s area and remove fluid from there to avoid overlap). Noita, for instance, treats the player as a body that can be drenched in liquids but also displaces them.Real-time editing: Because our simulation runs step-by-step, any change you make will naturally be picked up in the next tick. Just ensure changes update the relevant data structures:
Removing a block – if it was marked as solid in any neighbor count or collision map, update those.
Possibly update lighting if you have a lighting engine (out of scope here).
Here's a small example of a function for removing a block (digging) and one for placing a block:
cpp
Copy
void removeBlock(int x, int y) {
    if(x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT) return;
    // Only remove if it's a destructible terrain (not air or fluid)
    TileType type = worldGrid[y][x].type;
    if(type != AIR && type != WATER && type != LAVA && type != OIL) {
        worldGrid[y][x].type = AIR;
        // Mark neighbors active
        for(int ny = y-1; ny <= y+1; ++ny) {
            for(int nx = x-1; nx <= x+1; ++nx) {
                if(nx >= 0 && nx < WORLD_WIDTH && ny >= 0 && ny < WORLD_HEIGHT) {
                    // If neighbor is fluid or sand, mark for update (e.g., add to activeFluids or set a flag)
                }
            }
        }
    }
}

void placeBlock(int x, int y, TileType newType) {
    if(x < 0 || x >= WORLD_WIDTH || y < 0 || y >= WORLD_HEIGHT) return;
    // If target is not solid (allow replacing air or fluid)
    TileType oldType = worldGrid[y][x].type;
    if(oldType == AIR || fluids.count(oldType)) {
        worldGrid[y][x].type = newType;
        // If replaced a fluid, maybe spawn that fluid to sides?
        // Mark neighbors active, since they might be fluid adjusting to new wall
        for(int ny = y-1; ny <= y+1; ++ny) {
            for(int nx = x-1; nx <= x+1; ++nx) {
                if(nx >= 0 && nx < WORLD_WIDTH && ny >= 0 && ny < WORLD_HEIGHT) {
                    // mark neighbor for update if needed
                }
            }
        }
    }
}
In a more advanced physics game, you might treat the removed block as a particle (debris) that then falls. For example, when an explosion breaks a stone block, you could create a temporary falling stone rubble entity that uses physics or the same cellular rules as a heavy powder. Noita does this extensively – pixels that break off become part of the simulation (either as powder, or grouped into rigid chunks)​
REDDIT.COM
. For our purposes, you can decide if that level of detail is needed or if blocks just disappear on destruction.Multithreading and Interactivity: If you remove or add blocks from a different thread (say the main game thread while the physics thread is running), make sure to synchronize (e.g., lock the world data or queue the edits to apply between simulation steps). It’s usually easiest to do world modifications in the same thread or pause simulation briefly to apply changes, since our simulation is essentially cellular (which doesn’t naturally support concurrent modifications while iterating).
Final Note on Performance and Accuracy
Balancing performance and realism is key:
We prioritized a simple cellular physics model. This is very fast for what it achieves and scales well with multithreading and active-area focus. It may not capture fluid pressure perfectly or allow complex buoyancy, but it gives visually convincing results for games​
REDDIT.COM
.
We used concrete sizes (WORLD_WIDTH, WORLD_HEIGHT) in examples. In an actual game, you might generate an “infinite” world by generating chunks on the fly. Our methods (noise for terrain, chunk-based updates) are compatible with infinite worlds—just generate noise with a seed for new chunks as needed.
Many values (thresholds, probabilities, densities) in the code can be tweaked to fine-tune behavior.
By following the above steps:
Terrain: you have a variety of biomes and landscapes in a single map, constructed via noise and layered passes​
TERRARIA.WIKI.GG
.
Caves: you carved out interesting tunnels and caverns using two complementary algorithms.
Fluids/Gas/Powders: your world now has dynamic elements that follow physical intuition: water flows and levels out, lava pools and cools slowly, smoke rises, and sand falls and stacks.
Optimization: you update only what’s needed, using chunks and threads to maintain performance even with thousands of active particles​
GAMEDEV.STACKEXCHANGE.COM
.
Interactivity: players can dig and cause cave-ins or floods, and the system responds in real-time.
All code snippets are ready to be integrated, but remember to adapt types and functions (e.g., noise functions) to your engine or libraries. Testing and tweaking will be necessary – procedural systems often need adjustment to get the right feel. Use debug visualizations (like drawing the noise map or cave map) to fine-tune generation, and observe the physics in action to catch any unstable behaviors (e.g., fluids teleporting or stuck oscillating, which you can fix with the techniques discussed).With these systems in place, you have a solid foundation for a 2D voxel world akin to Terraria’s expansive terrains combined with Noita’s mesmerizing physics-based simulation, all running efficiently for real-time gameplay. Enjoy building and experimenting with your procedurally generated, fully simulated world!Sources:
Terraria worldgen passes for terrain, biomes, and features​
TERRARIA.WIKI.GG
​
TERRARIA.WIKI.GG
 (demonstrating the multi-pass approach).
Cellular automata cave generation using the 4-5 neighbor rule​
ROGUEBASIN.COM
.
Noita-inspired falling sand simulation (bottom-up processing for gravity-driven materials)​
BLOG.MACUYIKO.COM
 and considerations for multiple material types (liquids vs gases, density-based movement)​
BLOG.MACUYIKO.COM
.
Noita developers’ insights on optimization: chunking the world and updating in a checkerboard for multithreading, and tracking dirty regions of particles​
GAMEDEV.STACKEXCHANGE.COM
.
Reddit discussion confirming each fluid’s distinct density in Noita​
REDDIT.COM
​
REDDIT.COM
 and that complex pressure physics are often omitted for performance​
REDDIT.COM
.
W. Shadow’s blog on simple water simulation highlighting using slight compressibility instead of full pressure calculations​
W-SHADOW.COM
 (in our context we stuck to simpler rules, but it’s a good reference for advanced fluid mechanics in CA).
