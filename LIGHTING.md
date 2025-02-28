Introduction


Implementing dynamic lighting in a 2D sidescroller (with pixel/voxel physics and destructible terrain) involves simulating realistic light behavior in real-time. This includes handling multiple light sources, shadows cast by terrain/objects, and even approximating global illumination (light bouncing) for more realism. We will use modern OpenGL 4.5 features (shaders, framebuffers, etc.) to achieve this. The guide below provides a step-by-step implementation in C++ and GLSL, covering the key components of the lighting system, code snippets for important steps, optimization tips, and notes on alternative rendering techniques (like Vulkan or ray tracing) for superior results.Overview of Approach: We will employ a deferred rendering pipeline to manage many lights efficiently​


LEARNOPENGL.COM


. The scene’s geometry (including color and normals) is first rendered into G-buffer textures. A second pass will compute lighting for each pixel using the G-buffer, combining contributions from all lights. Shadows are computed via shadow mapping for occluders, and an approximate global illumination pass will simulate light bounce (e.g., via a flood-fill or screen-space technique). Finally, we’ll composite everything and output the lit scene. Throughout, we use modern OpenGL best practices (VAOs, sampler objects, GLSL 4.5+, avoiding deprecated fixed-function calls).


Components of a 2D Dynamic Lighting System


Light Sources and Data Structures


Define the types of lights in your game and their properties: for a 2D game, common lights include:


Point Lights: Radiate light in all directions (within the 2D plane), e.g. a torch or explosion.


Directional Lights: A global light with a given direction, e.g. sunlight shining across the scene.


Ambient Light: A global minimal illumination present everywhere (we’ll refine this with global illumination).


Each light can be represented with a struct in C++ and a corresponding uniform in the shader. Typical properties are position, color (RGB), intensity, and possibly radius or angle (for spotlights). For example:


cpp


Copy


struct Light {


    int type;              // 0 = point, 1 = directional


    glm::vec3 position;    // For point lights (x, y, z)


    glm::vec3 direction;   // For directional lights (and spotlights)


    glm::vec3 color;       // Light color (RGB components)


    float intensity;       // Brightness multiplier


    float radius;          // Effective radius (for point/spot light falloff)


    float angle;           // Spot cone angle (if using spotlights)


};


std::vector<Light> lights;


For performance, upload an array of lights to the GPU. In OpenGL 4.5, you can use a Uniform Buffer Object (UBO) or Shader Storage Buffer (SSBO) to send the light array to shaders (especially if you have many lights or want to avoid uniform count limits). For instance:


cpp


Copy


GLuint lightUBO;


glGenBuffers(1, &lightUBO);


glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);


glBufferData(GL_UNIFORM_BUFFER, lights.size() * sizeof(Light), lights.data(), GL_DYNAMIC_DRAW);


// Bind the buffer to a binding point (e.g., binding index 0)


glBindBufferBase(GL_UNIFORM_BUFFER, 0, lightUBO);


In the fragment shader for lighting, bind this UBO (layout(std140, binding=0) uniform Light { ... } light[MAX_LIGHTS];) or use an SSBO for flexible sizing. Each light’s data will be accessible for lighting calculations.


Shaders (Geometry, Lighting, and Shadow)


We will use multiple shaders in our pipeline:


Geometry Pass Shader: Renders the game scene (terrain, objects, etc.) and outputs data to G-buffer textures (like normals, albedo, etc.). Vertex shader transforms vertices as usual. Fragment shader writes out multiple outputs: e.g. world position, surface normal, albedo color, and any specular/emissive info.


Lighting Pass Shader: A full-screen quad shader that reads G-buffer textures and computes lighting. For each pixel, it will loop through lights and accumulate contributions (diffuse, specular) using the stored normal and albedo. It also samples shadow maps to modulate light if an occluder blocks it.


Shadow Map Shader: Used to render the scene from a light’s perspective. For a directional light, this would be a depth-only shader (vertex outputs depth); for point lights, one common method is to render the scene 6 times (one for each face of a cubemap) to capture depths in all directions​


LEARNOPENGL.COM


. The shadow shaders don’t need color outputs, only depth.


Global Illumination (GI) Shader/Compute: (Optional) If implementing GI via a screen-space or voxel method, you might have a compute shader or an additional fragment shader pass to compute indirect lighting. For example, an SSAO (screen-space ambient occlusion) shader can approximate contact shadows (which contributes to indirect lighting), or a custom compute shader can propagate light through a grid for multi-bounce illumination. We’ll discuss this in the GI step.


Ensure all shaders are written in GLSL 4.50 (or compatible with your OpenGL context) and use layout qualifiers for inputs/outputs for clarity. For example, in the geometry fragment shader:


glsl


Copy


layout(location = 0) out vec3 gPosition;


layout(location = 1) out vec3 gNormal;


layout(location = 2) out vec4 gAlbedoSpec;


In the lighting shader, you’ll have sampler2D uniforms for each G-buffer texture and for shadow maps.


Framebuffers and Render Targets


We need several off-screen framebuffers (FBOs) for our multi-pass rendering:


G-buffer FBO: In the first pass, we render scene geometry into multiple textures (G-buffer). We’ll have, for instance: position (RGB16F), normal (RGB16F), and albedo+specular (RGBA). We also attach a depth buffer (renderbuffer or texture) to this FBO to capture scene depth. This ensures proper depth testing in the geometry pass.


Shadow Map FBO(s): For each shadow-casting light, create a depth-only FBO. For a directional light, attach a 2D depth texture (e.g., 2048×2048 or appropriate resolution) to capture the orthographic depth map. For a point light (if dynamic shadows needed), use a depth cubemap texture (OpenGL cubemap with 6 faces) and either render the scene 6 times or use a geometry shader to output to different faces. Each face’s projection is 90° FOV to cover all directions​


LEARNOPENGL.COM


. You’ll likely allocate one shadow map per significant light (too many dynamic shadow lights can be slow, so prioritize key lights).


Light Accumulation (Lighting pass) FBO: This can be a simple single render target (RGB16F texture) where you accumulate the lighting results. Alternatively, you can render lighting results directly to the default framebuffer (screen) if you apply lighting in one final pass. A common approach is to output the lit scene color in this pass. If you plan post-processing (bloom, gamma correction), rendering to an HDR off-screen texture is useful.


Setup example for G-buffer in C++:


cpp


Copy


GLuint gBuffer;


glGenFramebuffers(1, &gBuffer);


glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);





// Create G-buffer textures


GLuint gPosition, gNormal, gAlbedoSpec;


int SCR_WIDTH = 1280, SCR_HEIGHT = 720;


glGenTextures(1, &gPosition);


glBindTexture(GL_TEXTURE_2D, gPosition);


glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);





// Repeat for gNormal (RGB16F) and gAlbedoSpec (RGBA)


glGenTextures(1, &gNormal);


// ... (setup similar to gPosition)


glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);





glGenTextures(1, &gAlbedoSpec);


glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);


glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlbedoSpec, 0);





// Tell OpenGL which attachments we'll draw into in this FBO


GLuint attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };


glDrawBuffers(3, attachments);





// Create and attach depth buffer (renderbuffer)


GLuint rboDepth;


glGenRenderbuffers(1, &rboDepth);


glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);


glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SCR_WIDTH, SCR_HEIGHT);


glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);





// Check completeness


if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {


    std::cerr << "G-Buffer not complete!" << std::endl;


}


glBindFramebuffer(GL_FRAMEBUFFER, 0);


Similarly, set up a depth texture for a shadow map:


cpp


Copy


// For a directional light shadow map (2D texture)


GLuint depthMapFBO, depthMapTex;


glGenFramebuffers(1, &depthMapFBO);


glGenTextures(1, &depthMapTex);


glBindTexture(GL_TEXTURE_2D, depthMapTex);


glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 2048, 2048, 0,


             GL_DEPTH_COMPONENT, GL_FLOAT, NULL);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


// Configure depth comparison if using sampler2DShadow in shader:


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);


glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);


glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);


glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTex, 0);


glDrawBuffer(GL_NONE);


glReadBuffer(GL_NONE);


glBindFramebuffer(GL_FRAMEBUFFER, 0);


For a point light with shadow, you would use GL_TEXTURE_CUBE_MAP with 6 faces and attach it similarly (using glFramebufferTexture for each face during rendering, or a geometry shader to dispatch to cube faces). This setup allows us to render depth from the light’s view(s) and later sample it for shadow tests.


Normal Maps and Materials


In a 2D voxel or pixel-art game, objects and terrain are essentially flat, but we can simulate 3D surface details using normal maps. A normal map is a texture that encodes per-pixel normals (the direction the surface is facing) in RGB. Using normal maps allows the lighting shader to calculate light incidence angles and produce shading even on a flat geometry​


MATTGREER.DEV


. For each terrain tile or sprite, you can generate a normal map (manually authored or computed from the shape). Even the destructible terrain (if represented as a pixel grid) can have normals computed by sampling the neighboring solidity (essentially computing a gradient on the heightfield of terrain).Store material properties for your fragments in the G-buffer. Typically:


Albedo (base color) and possibly an emissive component for glowing parts.


Normal vector (from normal map or default facing). You’ll transform this to world space in the geometry shader if needed (for simplicity in 2D, normals might just be oriented in the plane).


Optionally specular intensity or roughness if doing more advanced lighting (could pack specular exponent or roughness into the alpha of the albedo texture, as done in many deferred renderers​


LEARNOPENGL.COM


).


When rendering the geometry pass, sample the normal map in the fragment shader and unpack the normal. Commonly, a normal map’s RGB is in [0,1] range; convert to [-1,1] by normal = normalize(texture(normMap, uv).rgb * 2.0 - 1.0). Ensure the normal is in world space or view space consistently with how light directions/positions are handled. For simplicity, you can calculate everything in view space (camera space), including lights (transform light position to view space each frame, and use view-space normals and positions in G-buffer). Or you can stick to world space for G-buffer and also use world-space light positions; just be consistent so that the dot products for lighting make sense.


Shadows and Occlusion


Shadow Mapping: We use shadow maps to determine if a fragment is in light or in shadow. The concept is: render the scene from the light’s perspective into a depth texture (the shadow map). Then, when rendering the lighting for each fragment, transform the fragment’s position into the light’s coordinate system and compare its depth to the stored depth map​


LEARNOPENGL.COM


. If the fragment is further away than the stored depth, it’s occluded (in shadow) relative to that light.


For a directional light (e.g., sun), treat the light like a camera with an orthographic projection covering the game area. Usually, you align this ortho projection to focus on the relevant region of the scene (cascaded shadow maps can improve quality over large areas, but that’s advanced). Render the solid parts of the scene (terrain, objects) into the depth texture. In the lighting shader, use the fragment’s world position, transform by light’s orthographic projection * light’s view matrix to get shadow map UV and depth, then do the depth compare. Add a bias to avoid acne (self-shadowing artifacts). Example in shader:


glsl


Copy


// fragment world position -> light clip space


vec4 fragPosLightSpace = lightProjection * lightView * vec4(fragWorldPos, 1.0);


// Perform perspective divide (for proj matrix)


vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;


// Transform to [0,1] UV space


projCoords = projCoords * 0.5 + 0.5;


float closestDepth = texture(shadowMap, projCoords.xy).r;  // sampled depth


float currentDepth = projCoords.z;


float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;


Then use shadow factor to reduce lighting: e.g. if in shadow, use only ambient, if lit, use full light. (For soft shadows, you’d sample the shadow map multiple times (PCF filtering) around projCoords.xy and average the result instead of a single compare.)


For point light shadows: With a depth cubemap, you do a similar process but using a samplerCube in the shader. You compute a vector from the light to the fragment, and sample the cubemap’s depth. GLSL's samplerCube with shadow comparison can do this if set up (GLSL allows samplerCubeShadow). Alternatively, sample manually each face’s depth. Because this is more complex, often 2D games avoid point light shadows or keep their radius small. If implementing, your shader might do:


glsl


Copy


// In fragment shader, if light is point:


vec3 lightToFrag = fragWorldPos - lightPos;


float currentDist = length(lightToFrag);


// Sample shadow cubemap with comparison (if available)


float depthOnMap = texture(shadowCube, vec4(lightToFrag, currentDist)).r;


float shadow = currentDist - bias > depthOnMap ? 1.0 : 0.0;


This requires the cubemap stores distance as the value in each face. Another approach is to render a distance field in a 2D radial grid around the light for 2D games, or to cast rays in CPU for each light to mark shadow regions (less GPU intense for low-res maps).


2D Occluders (Destructible Terrain): Since terrain can change, the shadow maps need updating when terrain changes. Each destructible voxel can be rendered as a small quad in the shadow pass (or use geometry shader to billboard a point). Because our world is essentially a top-down projection for the light, an optimization is to generate a 2D binary map of occlusion and use a flood-fill or ray-cast algorithm to compute shadows. For instance, some 2D engines perform a raymarch from the light through a bitmap of the world to determine shadow edges, but implementing that on GPU can be complex. Using traditional shadow mapping as above is straightforward if we treat each voxel as having a certain height (even if minimal).


Ambient Occlusion: To enhance depth perception and contact shadows, consider Screen-Space Ambient Occlusion (SSAO). SSAO computes an ambient shadow factor per pixel by sampling the depth buffer around that pixel. This can darken creases and enclosed areas, complementing global illumination. While SSAO is a 3D concept, in a side-scroller game with overlapping geometry it can still apply. SSAO would be an additional pass after geometry (using the depth/normal from G-buffer) and modulates the ambient light. This is optional but can improve visual quality.


Global Illumination (Indirect Lighting)


True real-time global illumination (GI) is costly, but we can approximate it. The goal is to simulate light bouncing off surfaces so that lit areas can illuminate nearby dark areas (e.g., a torch light bouncing off a wall to faintly light an area around a corner).Possible approaches for a 2D game:


Light Propagation / Flood-Fill (Grid-based): Since the world is voxel-based (grid of pixels), one approach is to propagate light through the grid iteratively. For each light source, you can perform a breadth-first search (BFS) or wave propagation through empty space, attenuating light as it travels. This can be done on the CPU for discrete blocks (like how Minecraft spreads light in a 3D grid), but for performance a GPU approach is better for per-pixel. A flood-fill algorithm on the GPU can take each illuminated cell and spread a portion of light to neighbors over multiple iterations. This essentially solves a 2D radiosity approximation. You might implement this by ping-ponging between two 2D textures representing light intensities, or use a compute shader that iteratively updates a light texture. After N iterations (bounces), you use that texture as an indirect light map, adding it to the scene. This method allows for multiple bounces and can respond to changes in geometry (as long as you re-run the propagation when the scene updates). It’s important to limit the propagation radius for performance.


Screen-Space Global Illumination (SSGI): Similar to SSAO, SSGI tries to approximate light bounce by sampling the already-lit frame. One could take bright pixels from the first lighting pass and sample their contribution to neighboring pixels (within some radius and respecting depth differences). This is complex to implement and often noisy, but there are research examples of SSGI. For a simpler approximation, you might blur the light buffer and use it as an additive ambient term, but that fails if walls should block the light. So depth-aware blurring (like bilateral blur that stops at occluders) is needed.


Voxel Cone Tracing: This is a more advanced 3D technique (voxelizing the scene and cone-tracing for indirect light). In a 2D game, a similar idea would voxelize a 2D slice of the world (which it already is) and shoot “rays” out. Likely overkill here, but it’s conceptually how one might get GI with few bounces​


REDDIT.COM


​


REDDIT.COM


. Due to performance and complexity (needs 3D data structures and lots of memory), this is not the first choice for a 2D game.


For our implementation, we will outline a simpler flood-fill GI approach using a 2D light grid:


We maintain a 2D array (texture) indirectLight[x][y], initialized to 0. For each light source, add its direct illumination (maybe the first bounce) into this grid where not blocked. Then iteratively diffuse the light: for each cell, average or gather light from neighbors and accumulate, decreasing intensity each step. This can be done in a compute shader or fragment shader multiple passes. Essentially, this simulates light scattering to adjacent cells.


Use the solid terrain as barriers: when propagating, do not send light through filled cells (or attenuate heavily if simulating partial transmission). After a number of iterations (or when light is too low), stop. The resulting indirectLight grid will have values for how much bounced light reaches each cell.


Finally, in the lighting shader, add an indirect term: sample the indirectLight texture at the fragment’s position, and add that to the pixel’s illumination (often as an ambient term). Because this GI was computed in world-space grid, you might need to convert fragment coords to grid indices (if 1:1 with pixels, that’s direct; if your world is larger, you might have a scaled down light grid for performance, e.g., 1 texel = 4 game pixels, then sample accordingly).


This GI approach will increase realism, but be mindful of performance – you can use a lower resolution for the light propagation texture or update it less frequently (e.g., once every few frames) to spread cost out. Also consider an upper bound on bounces or distance.


Step-by-Step Implementation Guide


Now we will walk through implementing the lighting system step by step in C++ (OpenGL 4.5) with GLSL shaders. This assumes you have a basic rendering loop and can create OpenGL contexts, compile shaders, etc. (for brevity, those setup details are omitted).Step 1: Initialize Lighting System Resources


Define light structures and shader interfaces: Create the Light struct in C++ and prepare a UBO as shown earlier. Ensure your lighting fragment shader has a matching struct definition and an array or UBO binding for the lights. Also define a maximum number of lights or use a dynamic loop with SSBO. Example GLSL snippet (in lighting shader):


glsl


Copy


#define MAX_LIGHTS 16  


layout(std140, binding = 0) uniform Lights {  


    Light {


        int type;


        vec3 position;


        vec3 direction;


        vec3 color;


        float intensity;


        float radius;


        float angle;


    } lights[MAX_LIGHTS];


    int numLights;


};


uniform vec3 ambientColor;


uniform float ambientIntensity;


uniform sampler2D gPosition;


uniform sampler2D gNormal;


uniform sampler2D gAlbedoSpec;


uniform sampler2DShadow dirShadowMap;  // for directional light shadow


uniform samplerCubeShadow pointShadowMap;  // for point lights (if used)


Update the UBO data whenever lights move or change (using glBufferSubData or mapping the buffer).


Create framebuffers and textures: Set up the G-buffer FBO with position, normal, albedo textures, and a depth buffer as described in Components. Also set up shadow map FBOs for any shadow-casting lights (likely one for sun, and perhaps a few for the largest point lights). For now, assume one directional light shadow map. Example creation was given above.


Load/compile shaders: You will need at least three GLSL programs: GeometryPass, LightingPass, and ShadowPass.


GeometryPass vertex shader transforms object vertices normally (for sprites/tiles, this might just position quads in world). Pass texture coordinates and normals (if using vertex normals; for 2D, you might compute normals in frag from a normal map).


GeometryPass fragment shader outputs to gPosition, gNormal, gAlbedoSpec. If using normal maps, sample them and output the transformed normal. Output position in world space (or view space consistently). The alpha of gAlbedoSpec can store a specular strength or 1 if not used. Example:


glsl


Copy


// Geometry fragment shader (simplified)


layout(location=0) out vec3 gPosition;


layout(location=1) out vec3 gNormal;


layout(location=2) out vec4 gAlbedoSpec;


uniform sampler2D diffuseMap;


uniform sampler2D normalMap;


void main() {


    vec3 albedo = texture(diffuseMap, TexCoords).rgb;


    vec3 normal = texture(normalMap, TexCoords).rgb * 2.0 - 1.0;


    // If normal map is in tangent space, transform to world space here (not shown for brevity)


    gPosition = vec3(gl_FragCoord.x, gl_FragCoord.y, worldZ); // or use actual world position if available


    gNormal = normalize(normal);


    gAlbedoSpec.rgb = albedo;


    gAlbedoSpec.a = 1.0; // specular strength or other info


}


Note: In a strictly 2D game, you might not have a readily available “world position z”. You can store the screen-space depth (gl_FragCoord.z) or a pseudo-depth (e.g., layer index) if needed. If everything is essentially on a 2D plane, the depth buffer already separates foreground/background; storing full position is optional. Many deferred renderers store depth in a texture and reconstruct position in the lighting pass using the screen-space coordinates and camera inverse projection. You can do that to save memory (skip gPosition texture). For clarity, we’ll assume gPosition is stored or we can reconstruct it.


ShadowPass vertex shader: For directional light, it will transform vertices by lightView and lightProjection matrices (set as uniforms). No fragment shader output needed except depth. For point lights with cubemap, you might use a geometry shader to emit to 6 layers or render each face separately with the appropriate projection matrix per face (with a uniform or using gl_Layer in geometry shader). Ensure to cull or clip as needed to avoid artifacts.


LightingPass vertex shader: This is usually a full-screen quad, so a trivial shader that just passes through a quad’s corners to the fragment shader.


LightingPass fragment shader: This is the core of lighting. It will:


Fetch G-buffer values: vec3 WorldPos = texture(gPosition, UV).rgb; vec3 Normal = normalize(texture(gNormal, UV).rgb); vec3 Albedo = texture(gAlbedoSpec, UV).rgb; float SpecStrength = texture(gAlbedoSpec, UV).a; (if you stored spec in alpha).


Initialize output color with ambient light. For example: vec3 lighting = ambientColor * ambientIntensity; – this could also incorporate GI if you have a separate texture for indirect light (sample it similar to Albedo and add).


For each light in the lights array (loop 0 to numLights-1): calculate lighting. Pseudocode inside the loop:


glsl


Copy


if(lights[i].type == 0) { // point light


    vec3 L = lights[i].position - WorldPos;


    float distance = length(L);


    if(distance > lights[i].radius) continue; // skip if outside light range


    L = normalize(L);


    // diffuse


    float diff = max(dot(Normal, L), 0.0);


    // attenuation (point light gets weaker with distance)


    float atten = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance*distance); // quadratic atten (tweak constants)


    // shadow


    float shadow = 0.0;


    if(lights[i] casts shadow) {


        // sample shadow cubemap (see above)


        shadow = ... ; // 1.0 if in shadow


    }


    vec3 diffuse = diff * lights[i].color * lights[i].intensity;


    // specular (Blinn-Phong)


    vec3 viewDir = normalize(camPos - WorldPos);


    vec3 halfDir = normalize(L + viewDir);


    float spec = pow(max(dot(Normal, halfDir), 0.0), 16.0); // 16 or use roughness for glossy


    vec3 specular = lights[i].color * spec * SpecStrength;


    // accumulate


    lighting += (1.0 - shadow) * atten * (diffuse * Albedo + specular);


} else if(lights[i].type == 1) { // directional light


    vec3 L = normalize(-lights[i].direction); // direction the light travels, assuming lights[i].direction is light pointing direction


    float diff = max(dot(Normal, L), 0.0);


    float shadow = 0.0;


    if(lights[i] casts shadow) {


        // Compute fragPos in light space and sample shadow map (as shown earlier)


        shadow = ...;


    }


    vec3 diffuse = diff * lights[i].color * lights[i].intensity;


    // Specular for directional


    vec3 viewDir = normalize(camPos - WorldPos);


    vec3 halfDir = normalize(L + viewDir);


    float spec = pow(max(dot(Normal, halfDir), 0.0), 16.0);


    vec3 specular = lights[i].color * spec * SpecStrength;


    lighting += (1.0 - shadow) * (diffuse * Albedo + specular);


}


This loop will accumulate the contribution of each light. Note we multiply the diffuse by the surface Albedo color (Lambertian diffuse). The specular is added on top (it can also be modulated by some material property if available). We also factor in shadow: if the point on the surface is in shadow for that light, we multiply that light’s contribution by (1 - shadow) (which will be 0 if fully shadowed). If implementing soft shadows, shadow could be a fractional value from 0 to 1 (percentage of light blocked).


After the loop, output the computed color: FragColor = vec4(lighting, 1.0); (assuming an HDR pipeline, lighting might be >1 for bright spots, which you tone-map later).


Compile and link these shaders, and get the uniform locations for things like matrices, samplers, etc., to set them in the rendering loop.


Step 2: Geometry Pass (Render Scene to G-buffer)


In your render loop (each frame):


Update dynamic objects (physics, etc.) and destructible terrain mesh if needed (e.g., if terrain changed, update the mesh or texture representing it). Ensure you have up-to-date vertex data or texture for terrain.


Bind G-buffer FBO: glBindFramebuffer(GL_FRAMEBUFFER, gBuffer); glViewport(0,0,width,height); and clear appropriate buffers. Typically clear the depth (glClear(GL_DEPTH_BUFFER_BIT)) and maybe color attachments to 0 (though color doesn’t strictly need clear if every pixel is covered; clearing to black is fine for albedo).


Use GeometryPass shader program. Bind the appropriate VAOs for your objects/terrain. For each object:


Set its model matrix (if any) as uniform. Also set any material textures (diffuse, normal map) to the shader samplers.


Draw the object (glDrawElements or glDrawArrays). For a tilemap terrain, you might batch draw many tiles. For voxels, if each voxel is a quad, you may want to instance them or draw a mesh representing the entire terrain (to avoid per-voxel draw overhead). Possibly you have the terrain as a single mesh generated from the voxel map (greedy meshing) – that would be efficient.


All geometry draws will output to the G-buffer textures. The depth buffer will be filled so that later only nearest fragments are lit.


After drawing all scene elements, unbind the FBO (or bind the default/framebuffer for next passes). At this point, you have captured the scene’s per-pixel information in the G-buffer. You can visualize G-buffer textures for debugging (positions, normals, etc.).


Step 3: Shadow Map Pass (Generate Shadow Depth Textures)


For each light that requires a shadow map (say, the sun and maybe a couple of important point lights), do:


Directional Light Shadow:


Bind the shadow FBO for the directional light (e.g., glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO)) and set viewport to shadow map resolution (e.g., 2048×2048).


Clear the depth buffer (glClear(GL_DEPTH_BUFFER_BIT); color is not used).


Use the shadow depth shader program. Set the light’s view-projection matrix uniforms. For a directional light, compute an orthographic projection that encompasses the visible scene. For example, an ortho box around the camera focus or the whole level (if small). The view matrix can be built from the light direction (e.g., like a camera at some pseudo position far in the light direction looking toward the scene).


Render the scene’s geometry again (terrain, objects). This time, you only care about depth, so the shader might be very simple: vertex transforms to light clip space and fragment writes depth. You can even use the same vertex shader as the geometry pass but a different fragment that does nothing (the depth from rasterization will be written). If you have a lot of small objects, consider frustum culling to the light’s frustum to skip things not casting shadows in range.


After rendering, check glBindFramebuffer(GL_FRAMEBUFFER, 0) (or if multiple shadow maps, you can keep binding different FBOs). Now you have depthMapTex filled.


Point Light Shadow (if any):


Bind the point light’s shadow cubemap FBO. For each of the 6 faces (or use a geometry shader trick), set the appropriate projection (90°) and view (center at light position, looking in +X, -X, +Y, -Y, +Z, -Z directions). You might loop through faces:


cpp


Copy


glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);


std::vector<glm::mat4> shadowViews = {/*6 view matrices for each direction*/};


for(int face = 0; face < 6; ++face) {


    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, cubeDepthTex, 0, face);


    glClear(GL_DEPTH_BUFFER_BIT);


    shadowShader.setMat4("lightViewProj", shadowProj * shadowViews[face]);


    // render scene


}


The vertex shader can take a model and these viewProj matrices to output depth. The fragment shader again just writes depth (or nothing, just enables depth test).


After this, the cubemap cubeDepthTex contains distances. Ensure the compare mode is set if using samplerCubeShadow so hardware filtering can be used for PCF across cube faces.


If the scene or lights don’t move often, you could update shadow maps only intermittently to save performance. In a fully dynamic setting (destructible terrain and moving lights), you likely update each frame or when changes occur.Step 4: Lighting Pass (Deferred Lighting with Shadows)


Now compute the lighting using the data from previous passes:


Bind the lighting accumulation target. This could be the default framebuffer (if you want to draw directly to screen) or an intermediate HDR FBO if you plan to do post-processing after. For example:


cpp


Copy


glBindFramebuffer(GL_FRAMEBUFFER, 0); // draw to screen


glClear(GL_COLOR_BUFFER_BIT);


glViewport(0,0, SCR_WIDTH, SCR_HEIGHT);


(If the G-buffer textures are the same size as screen, viewport should match.)


Activate the LightingPass shader program. Bind the G-buffer textures to the shader’s samplers (e.g., set texture unit 0 to gPosition, unit 1 to gNormal, etc. and call lightingShader.setInt("gPosition", 0) accordingly). Bind the shadow map textures as well (e.g., glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, depthMapTex) and set sampler uniform). Also update uniforms for camera position (if doing specular in world space), ambient light, and number of lights. The UBO with lights should already be bound to binding 0 (from initialization).


Render a full-screen quad (two triangles covering the viewport). You should have a VAO for a screen-filling quad ready (clip-space coords from (-1,-1) to (1,1)). When you draw this, the fragment shader will be invoked for each pixel, and perform the lighting calculations as described earlier. Each fragment will: sample G-buffer, loop lights, sample shadow maps, etc., and output the final lit color.


The result of this pass is the illuminated scene with all lights applied in one combined image. If you rendered to an HDR texture, you would next do tone mapping or gamma correction and then to the screen. If directly to screen, ensure you convert to proper gamma (e.g., the shader could output in linear space and you enable GL_FRAMEBUFFER_SRGB or do manual gamma correction).


Step 5: Global Illumination Pass (Indirect Lighting)


If implementing global illumination, there are a couple of ways to integrate it:


Integrate into Lighting Shader: If you computed an indirectLight texture (via a compute shader or earlier CPU computation), you can simply sample that in the lighting shader and add it to the ambient term. For example, have a sampler2D indirectLightMap where each texel stores the bounced light color for that cell. Then in the lighting shader do: vec3 bounced = texture(indirectLightMap, fragWorldXY).rgb; lighting += bounced * Albedo; (The * Albedo might be optional depending on if indirectLightMap already includes surface color in it or not. Usually GI light is colored by surfaces.) This way, GI is just another light contribution but coming from environment.


Separate Computation: Alternatively, you could compute GI after you have the initial lighting. One approach is to use the rendered image and blur it in a way that mimics light spreading. But a more physically based approach: after computing direct lighting, run a compute shader that does the flood-fill. For example, in the compute shader, for each cell (or pixel) that is not solid, set its light value = max(self, neighbors*some factor). Do this iteratively. Pseudocode in CPU terms:


cpp


Copy


for(int iter=0; iter<NUM_BOUNCES; ++iter) {


    for(x,y in world) {


        if(!solid[x][y]) {


           indirect[x][y] = 0;


           for(each neighbor (nx,ny) of (x,y)) {


              // if neighbor is within bounds and not solid


              indirect[x][y] += indirect_prev[nx][ny] * 0.25; // gather quarter from each (for 4-neighborhood)


           }


           // plus maybe add some of original light if first bounce


        } else {


           indirect[x][y] = 0;


        }


    }


    swap(indirect, indirect_prev);


}


The above is a very simple diffuse bounce (and not physically accurate, but spreads light). On GPU, you would do this in a shader by sampling a texture of the previous iteration’s light and writing to another. This is essentially a Jacobi iteration for radiosity. After enough iterations, the indirect texture holds an approximation of global illumination. Then use it as described (sample in lighting shader or composite by drawing a textured quad adding it).Make sure to include the influence of actual light sources on the first iteration: e.g., initialize indirect_prev with some fraction of direct lighting (or even the direct lighting render result converted to a low-res grid). This way bright spots start propagating outward.


Performance: Limit the GI grid size (maybe one texel per 4x4 game pixels or more) and the number of iterations based on how much bounce you need. You can also do a temporal GI: reuse last frame’s indirect light and only slowly equalize, which effectively accumulates bounces over frames (as seen in some Godot GI experiments where previous frame is re-used for infinite bounces​


REDDIT.COM


). This can smooth the cost over time.


For clarity, if we proceed without an explicit GI computation step, the result will still have dynamic lighting and shadows. GI can be considered an enhancement that you add once the basic system works. In summary, ensure that if you have GI computed, you incorporate it into the final lighting either directly in the lighting shader or as an additional full-screen additive pass.Step 6: Final Composition and Post-Processing


At this stage, we have the lit scene. If the lighting pass was drawn to an off-screen texture (for HDR or other reasons), you now composite it to the screen. Common final steps:


Tone Mapping and Gamma: If using HDR lighting (high intensities), apply tone mapping (e.g. ACES or simple Reinhard) in a shader to clamp bright values naturally, then gamma-correct to sRGB for display. If you rendered directly in low dynamic range and kept values 0-1, you might simply need to apply gamma correction (or if GL_FRAMEBUFFER_SRGB was enabled and you output in linear, OpenGL could do it automatically).


Add UI or Unlit Elements: If you have any UI or HUD, render it on top of the lit scene now. Also, if some objects were meant to be fully bright (emissive) regardless of lighting, you could render them after lighting pass (or include their emissive color in the G-buffer so it gets added in lighting).


Post-processing: Effects like bloom (glow) can greatly enhance the feel of bright lights. To implement bloom: extract bright regions from the lit texture (threshold), blur them, and add back. This makes torches and such bleed light. Be careful to exclude shadowed areas (bloom should be added after shadows applied, so doing it now on the final image is correct). Also consider if you want lens flares, color grading, etc., as needed for your game’s aesthetic.


Finally, swap buffers/present the image to the screen as usual. At this point, you should see your scene with dynamic lights, shadows being cast by terrain and objects, and possibly softer indirect lighting if GI is enabled.


Performance Considerations and Optimizations


Deferred vs Forward Rendering: We chose a deferred approach because it handles many lights efficiently​


LEARNOPENGL.COM


. In deferred shading, lighting cost is proportional to number of pixels * lights, rather than (pixels * lights * objects) as in forward. This allows hundreds of lights in a voxel game​


REDDIT.COM


. However, deferred rendering consumes more memory (G-buffer) and doesn’t directly support transparent objects (you must render transparencies in a forward pass after, without deferred benefits). If your game has few lights or lots of alpha-blended particles, you might opt for a forward rendering with light culling instead, or a hybrid (deferred for terrain, forward for particles).


Light Culling: Even in deferred, iterating over all lights for every pixel is wasteful if many lights have limited range. Implement scissor test or stencil mask for each light volume to restrict lighting calculations to only the affected pixels. For example, for a point light, you can render a bounding quad or sphere in the lighting pass shader (using stencil to only allow fragments within light radius to be computed). Alternatively, partition screen into tiles (16x16) and compute which lights affect which tile (tile/clustered shading). These optimizations can drastically cut down shader work when many lights are present.


Shadow Map Performance: Shadow rendering can be the most expensive part if many objects and lights cast shadows. Consider:


Only the largest/most important lights cast shadows. Minor light sources might be non-shadowed (or have simpler shadows like a small radius blur).


Cache static terrain shadows: If terrain doesn’t move, you can render its shadow map once and reuse it until changed. Only dynamic objects need updated shadows (could draw them to shadow map separately or use multiple layers).


Use PCF (Percentage Closer Filtering) or variance shadow maps for smoother shadows, but note these add sampling cost. You can choose lower resolution shadow maps for less important lights or for performance scaling.


Shadow map size and projection: fit the frustum tightly to the area of interest to maximize resolution usage. For a side-scroller level, you can move the shadow map region along with the camera (for sun shadows) – this is analogous to cascaded shadow maps (split the view range into multiple sections each with its own shadow map for better detail). For simplicity, you might not implement full cascades, but be aware of quality vs size tradeoff.


Normal Map and Material Detail: Using normal maps and per-pixel lighting will be costlier than simpler shading. If performance is an issue on lower-end hardware, you could consider per-vertex lighting for large simple surfaces or baking some lighting. However, modern GPUs should handle a 2D game with these shaders easily, especially if you keep resolution reasonable and optimize shader loops.


Global Illumination Cost: Indirect lighting computations (like our flood-fill) can be expensive if the world is large. To optimize:


Compute GI on a coarser grid than full resolution (e.g., 1/4 or 1/8 resolution of screen), since human vision is less sensitive to low-frequency light changes.


Limit update frequency – e.g., only recompute GI when a significant change happens (light moved significantly or big explosion occurred). Slight GI lag is often not noticeable. Or update one quadrant of the map per frame (spreading the load).


Use temporal accumulation: each frame, blend between old GI and new calculations a bit; this smooths out noise if you do random sampling for GI and also amortizes work.


Profile and identify if the GI shader is a bottleneck. If so, consider simplifying bounce calculations or reducing iterations.


Batching Draws: Ensure in geometry and shadow passes that drawing many voxels/tiles is done efficiently. Use instanced rendering for tiles or merge them into meshes to avoid thousands of draw calls. Fewer draw calls will give the CPU more headroom to potentially do GI calculations or physics.


Memory considerations: The G-buffer with multiple 16-bit float textures at 1080p can be a few hundred MB/s in bandwidth each frame. If running on lower VRAM, consider packing data: e.g., store depth in one channel and normal in two channels (with third reconstructed), pack specular and albedo’s alpha together, etc. Also free any unused buffers and use texture formats that are no larger than needed (e.g., if you don’t need high precision normals, a RGBA8 could suffice with careful packing).


Use of OpenGL 4.5 features: Take advantage of features like Direct State Access (DSA) for cleaner object setup (since OpenGL 4.5 allows glCreateTextures, glNamedFramebufferTexture, etc., which simplify code and possibly driver efficiency). Use glBindTextureUnit instead of glActiveTexture+glBindTexture if available. These make the code more robust and possibly a bit faster by reducing binding calls overhead. Also, consider using glMultiDrawIndirect if you can batch multiple object draws in one call, especially for drawing many lights as proxy geometry or many scene elements in one go.


Profiling and Scaling: Test your game on target hardware. If performance is low, you can scale down: reduce shadow resolution, number of lights, or even provide a toggle to disable global illumination for low-end machines. Conversely, on high-end, you could enable more advanced effects (like real-time ray traced reflections if using different API, etc. – see below).


Alternative Techniques and APIs for Improved Lighting


While OpenGL 4.5 is capable of advanced lighting, there are newer technologies that could simplify or enhance the implementation:


Vulkan or DirectX 12: These modern graphics APIs offer lower-level control, better multi-threading, and support for GPU compute and ray tracing. The lighting system outlined can be implemented in Vulkan similarly, with potential performance gains due to better CPU utilization and explicit control of memory. More importantly, Vulkan/DX12 support hardware ray tracing (DXR) via extensions, whereas OpenGL does not​


REDDIT.COM


. With ray tracing, you could implement shadows and global illumination with physically accurate ray queries. For example, instead of shadow maps, you could trace rays from each pixel to lights to determine occlusion (for perfect hard shadows, or multiple samples for soft shadows). Global illumination could be achieved through ray tracing (path tracing or tracing a few diffuse rays for indirect light) – though real-time GI this way is extremely heavy, techniques like ReSTIR GI or hardware-accelerated sparse voxel tracing are emerging. If you consider moving to Vulkan, you could integrate ray tracing for effects like reflections or more accurate GI as optional features. Keep in mind, using Vulkan/DX12 has a steep learning curve and a lot of boilerplate, so weigh the benefits (more fine-tuned performance, access to latest features) against the development effort.


Physically Based Rendering (PBR): Upgrading the lighting model to a PBR model (with metalness, roughness, energy-conserving BRDFs) can improve realism. OpenGL 4.5 can handle PBR in shaders – you’d add extra G-buffer textures for material properties and use a more complex lighting equation (e.g., Cook-Torrance BRDF). If your art style is pixelated, full PBR might not be necessary, but if you want advanced look, consider it. Vulkan/DX12 make no difference for PBR vs Blinn-Phong – it’s all shader logic. However, modern engines (Unity, Unreal) have PBR by default, so if top visual fidelity is a goal, you might borrow techniques from them.


Game Engines / Libraries: If writing everything from scratch becomes overwhelming, note that engines like Godot and Unity have 2D lighting systems. Unity’s Universal Render Pipeline (URP) includes a 2D renderer with lights and shadows, though not full GI. Godot 4 introduced a 2D GI probe (experimental) which does bounce lighting. These can serve as references or even alternatives if you decide not to reinvent the wheel. However, using them means less low-level control and possibly not fitting your voxel physics requirements exactly. Since the question is about implementation in OpenGL/C++, we assume you’re building a custom engine, but it’s worth knowing what’s out there.


Compute Shaders for Parallelism: OpenGL 4.5 supports compute shaders, which we already considered for GI. Compute shaders can also be used for tiled lighting (computing light lists for screen tiles) or for generating data like a cluster grid for clustered shading. This can speed up how you apply many lights by doing culling on GPU. Another advanced use: compute shader can rasterize light volumes or do manual ray marching for shadows (which might be helpful in 2D if you implement something like a signed distance field for the scene and ray-march for shadow edges). These are more esoteric, but OpenGL compute gives flexibility beyond the traditional rendering pipeline.


Future-facing Tech (if hardware allows): Techniques like Realtime Global Illumination (GI) systems e.g., Unreal’s Lumen (software ray tracing and probe volumes) or Ray Traced GI could yield better results. If you find that your GI approximation isn’t sufficient, and performance budget allows on target hardware (like requiring an RTX GPU), you could integrate a ray trace pass using extensions or switch to an API that supports it. For instance, using Vulkan Ray Tracing, you could build an acceleration structure of your 2D scene (essentially triangles of your geometry extruded a bit) and then trace diffuse rays for indirect lighting. This would give very realistic results (soft global illumination, color bounce, etc.), but again, significant development effort.


Comparison of APIs: To summarize, OpenGL 4.5 can achieve the goal with careful optimization and is simpler to set up initially (given its fixed-function helpers and widespread support). Vulkan/DirectX12 can give more headroom and features at the cost of complexity. Since OpenGL is no longer evolving (no support for new features like hardware RT), if you foresee needing those, it might be worth the switch. If sticking with OpenGL, you will rely on shader tricks and possibly vendor-specific extensions for things like bindless textures or GPU-driven rendering to push it to the max. If using alternative APIs, you might get better driver support for multithreading and modern GPU features out-of-the-box, which could be beneficial for a complex simulation game.


Conclusion


By following the above guide, you can implement a robust dynamic lighting system in a 2D side-scroller game using modern OpenGL 4.5. We covered setting up deferred rendering with G-buffers, real-time shadow mapping for both directional and point lights, and even approaches to approximate global illumination. The provided code snippets and steps should serve as a strong starting point – you can expand on them (adding more lights, handling transparency, etc.) as needed. Always keep performance in mind by limiting unnecessary calculations and using the GPU efficiently. With the system in place, your game’s destructible environments, objects, and fluids will come to life with realistic lighting – torches will cast moving shadows, explosions will light up dark caves, and caves will not be pitch black due to indirect light bouncing around corners. Good luck with your implementation, and happy coding!References: Dynamic lighting and shadow mapping techniques were informed by common graphics knowledge and resources like LearnOpenGL​


LEARNOPENGL.COM


 for deferred shading and shadow mapping​


LEARNOPENGL.COM


. Strategies for voxel and 2D lighting (deferred lighting in voxel engines​


REDDIT.COM


 and using normal maps in 2D​


MATTGREER.DEV


) guided our approach. The flood-fill global illumination idea is inspired by Lionel Pigou’s write-up on 2D GI. When considering advanced techniques or API choices, insights from real-world implementations of voxel GI​


REDDIT.COM


​


REDDIT.COM


 and discussions on OpenGL vs Vulkan​


REDDIT.COM


 were used to provide context. These sources and industry practices reinforce the recommendations and best practices described.
