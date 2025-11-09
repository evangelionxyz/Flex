# Flex Engine Documentation

## Project Overview

**Flex Engine** is an educational game engine written in **C++23** for learning modern graphics programming, engine architecture, and real-time rendering techniques. Built on **OpenGL 4.6**, it serves as a playground for experimenting with physically-based rendering (PBR), post-processing effects, physics simulation, and scene management.

### Purpose
- **Educational**: Understand GPU pipelines, rendering algorithms, and game engine subsystems
- **Experimental**: Rapid prototyping of rendering techniques and visual effects
- **Approachable**: Clean C++23 codebase with modern practices (RAII, smart pointers, ECS)

### Core Technologies
- **Graphics API**: OpenGL 4.6 (via GLAD loader)
- **Windowing**: SDL3 for cross-platform window management and input
- **Mathematics**: GLM for vector/matrix operations
- **Physics**: Jolt Physics for rigid body dynamics
- **UI**: ImGui (docking branch) for editor interface
- **Scene Graph**: EnTT entity-component-system (ECS)
- **Asset Loading**: tinygltf for glTF 2.0 models, stb_image for textures
- **Text Rendering**: FreeType + msdfgen for multi-channel signed distance field fonts

## Architecture Overview

Flex Engine is organized into modular subsystems that separate concerns and promote maintainability:

### Core Systems (`Flex/Source/Core`)

#### `App` Class
The heart of the engine. Responsibilities include:
- **Main Loop**: Drives the game loop with fixed/variable timestep
- **Subsystem Initialization**: Boots renderer, physics, UI systems
- **Scene Management**: Handles play/stop modes, scene switching, serialization
- **Event Routing**: Dispatches SDL3 input events to camera, UI, and scene systems
- **Viewport Management**: Manages ImGui viewport for in-editor rendering
- **File Dialogs**: Integrates native file pickers for asset loading and scene save/load

Key Methods:
- `App::Run()`: Main game loop (event polling, update, render, swap buffers)
- `App::OnScenePlay()` / `App::OnSceneStop()`: Runtime vs editor mode switching
- `App::UIViewport()`: Renders scene output to an ImGui window
- `App::SaveScene()` / `App::OpenScene()`: Scene serialization workflows

#### `Window` Class
Wraps SDL3 windowing and OpenGL context:
- **Context Creation**: Initializes SDL3 window with OpenGL 4.6 core profile
- **Input State Tracking**: Maintains key/mouse button states and modifier keys
- **Event Callbacks**: Exposes callbacks for keyboard, mouse motion, scroll, and resize
- **Platform Integration**: Sets native window properties (Windows dark mode, borders)
- **Display Modes**: Fullscreen toggle, maximize, minimize, restore

#### `Camera` Struct
Orbital camera with spherical coordinates:
- **Projection Types**: Perspective and orthographic modes
- **Orbit Controls**: Yaw/pitch/distance with smooth inertia and damping
- **Pan & Zoom**: Mouse-driven camera manipulation
- **Lens Parameters**: Focal length, f-stop, focal distance for depth-of-field
- **Post-Processing Settings**: Embedded `PostProcessing` struct for bloom, vignette, chromatic aberration, SSAO

Key Fields:
- `glm::vec3 position, target, up`: Camera transform
- `float yaw, pitch, distance`: Spherical coordinates for orbit camera
- `glm::mat4 view, projection`: Cached matrices updated per frame
- `CameraLens lens`: Depth-of-field parameters
- `PostProcessing postProcessing`: Effect toggles and parameters
- `Controls controls`: Sensitivity, inertia, and debug settings

#### `ImGuiContext` Class
Manages ImGui integration:
- **Docking Layout**: Configures multi-window editor layout
- **Font Loading**: Loads custom fonts and icon fonts
- **Style Customization**: Applies custom color schemes and UI styling
- **Event Handling**: Forwards SDL3 events to ImGui backend

#### `UUID` Class
128-bit universally unique identifiers for entities:
- Used as primary entity identifier in ECS and serialization
- Enables stable entity references across scene save/load cycles

### Renderer Systems (`Flex/Source/Renderer`)

#### `Renderer` Namespace
Core OpenGL abstraction layer:
- `Renderer::Init()` / `Renderer::Shutdown()`: GL state setup
- `Renderer::CreateShaderFromFile()`: Compiles and links GLSL programs
- `Renderer::DrawIndexed()`: Issues indexed draw calls

#### `VertexArray`, `VertexBuffer`, `IndexBuffer` Classes
Modern OpenGL buffer abstractions:
- **VAO Management**: `VertexArray` binds vertex attributes to buffers
- **Buffer Uploads**: `VertexBuffer` and `IndexBuffer` wrap `glBufferData`
- **Attribute Configuration**: `SetAttributes()` defines layout (position, normal, tangent, UV, etc.)

#### `Shader` Class
GLSL program management:
- **Compilation**: Compiles vertex/fragment/compute shaders with error reporting
- **Uniform Setting**: Type-safe uniform setters (`SetUniform<T>`)
- **Hot-Reload**: Supports runtime recompilation for rapid iteration

#### `Texture2D` Class
Texture loading and binding:
- **Format Support**: RGB8, RGBA8, RGBA16F, RGB32F, depth/stencil
- **Filtering**: Nearest, Linear, Mipmaps
- **Wrap Modes**: Repeat, Clamp, Mirror
- **HDR Loading**: Uses stb_image for LDR, custom loader for HDR environment maps

#### `Framebuffer` Class
Offscreen render targets:
- **Attachments**: Color (multiple render targets), depth, stencil
- **Resize**: Dynamic resizing for viewport changes
- **Binding**: Activates framebuffer and sets viewport in one call

#### `Material` Class
PBR material parameters and textures:
- **Albedo/Base Color**: RGB texture + tint factor
- **Metallic-Roughness**: Packed texture (R=occlusion, G=roughness, B=metallic)
- **Normal Maps**: Tangent-space normal maps
- **Emissive**: Self-illumination texture + intensity factor
- **Occlusion**: Ambient occlusion texture
- **Uniform Buffer**: Uploads material parameters to GPU via UBO (binding point 2)

#### `Mesh` and `MeshInstance` Classes
Geometry representation:
- **Vertex Format**: Position, normal, tangent, bitangent, color, UV
- **Mesh Caching**: `MeshLoader` caches meshes by vertex/index count to avoid duplication
- **Scene Graph**: `MeshNode` and `MeshScene` preserve glTF node hierarchy and local transforms
- **Fallback Geometry**: Quad and skybox cube generators for testing

#### `Bloom` Class
High-quality bloom post-processing:
- **Mip Chain**: Generates multiple downsampled levels
- **Separable Blur**: Horizontal then vertical Gaussian blur per level
- **Upsample & Combine**: Blends blur levels for soft, natural bloom
- **Settings**: Threshold, intensity, knee, radius, iteration count

Structure:
- `std::vector<Level> m_Levels`: Each level has three framebuffers (downsample, blur H, blur V)
- `BloomSettings settings`: Runtime-adjustable parameters
- `Build(uint32_t sourceTex)`: Generates bloom from HDR input

#### `SSAO` Class
Screen-space ambient occlusion:
- **Kernel Generation**: Random hemisphere samples for occlusion tests
- **Noise Texture**: 4x4 rotation vectors to break up sampling patterns
- **Depth-Based**: Reconstructs world position from depth buffer
- **Blur Pass**: Bilateral blur to reduce noise
- **Parameters**: Radius, bias, power for artistic control

#### `CascadedShadowMap` Class
Directional light shadow mapping:
- **Cascades**: 4 split frustums for varying shadow detail by distance
- **Depth Array Texture**: Single 2D array texture for all cascades
- **Dynamic Splits**: Adjusts split distances based on camera near/far planes
- **Quality Presets**: Low (512x512), Medium (1024x1024), High (2048x2048)
- **PCF Filtering**: Percentage-closer filtering for soft shadow edges

Key Methods:
- `Update()`: Recomputes light view-projection matrices per cascade
- `BeginCascade(int index)`: Binds cascade layer for depth rendering
- `BindTexture(int unit)`: Binds shadow depth array for sampling in PBR shader

#### `Renderer2D` Class
Immediate-mode 2D line rendering:
- **Batching**: Accumulates line primitives and flushes in one draw call
- **Debug Visualization**: Used for physics collider debug draw and gizmos
- `DrawLine()`: Submits a line segment with color
- `SetLineWidth()`: Adjusts line thickness

#### `Font` and `TextRenderer` Classes
Multi-channel signed distance field (MSDF) text rendering:
- **Font Loading**: Uses FreeType + msdfgen to generate MSDF atlases
- **High-Quality Rendering**: Sharp text at any scale without blurring
- **Batched Rendering**: Builds vertex buffer of quads for text strings
- **Kerning & Spacing**: Configurable line spacing and character kerning

### Scene Systems (`Flex/Source/Scene`)

#### `Scene` Class
Entity-component-system container:
- **EnTT Registry**: `entt::registry* registry` manages entities and components
- **Entity Management**: Create, destroy, duplicate entities with stable UUIDs
- **Model Loading**: `LoadModel()` parses glTF files into entities with `TransformComponent` and `MeshComponent`
- **Play/Stop**: Starts/stops physics simulation, swaps between editor and runtime scenes
- **Rendering**: `Render()` iterates entities with mesh components and draws them
- **Serialization**: Saves scene graph to JSON via `SceneSerializer`

Key Methods:
- `CreateEntity(name, uuid)`: Spawns entity with `TagComponent` and `TransformComponent`
- `AddComponent<T>(entity)` / `RemoveComponent<T>(entity)`: Component manipulation
- `Clone()`: Deep copies the scene for runtime snapshot (play mode)

#### ECS Components (`Scene/Components.h`)

**`TagComponent`**
- `std::string name`: Entity name for editor display
- `UUID uuid`: Stable entity identifier
- `UUID parent`: Parent entity for hierarchy
- `std::set<UUID> children`: Child entities for scene graph

**`TransformComponent`**
- `glm::vec3 position, rotation, scale`: Local transform
- Rotation stored as Euler angles in degrees

**`MeshComponent`**
- `std::string meshPath`: Asset path for mesh
- `Ref<MeshInstance> meshInstance`: Loaded mesh data
- `int meshIndex`: Index into glTF mesh array

**`RigidbodyComponent`**
- `float mass, friction, restitution`: Physics material properties
- `bool useGravity, isStatic`: Physics behavior flags
- `JPH::BodyID bodyID`: Handle to Jolt Physics body
- `EMotionQuality`: Discrete or continuous collision detection

**`BoxColliderComponent`**
- `glm::vec3 scale, offset`: Collider dimensions and center offset
- `float friction, restitution, density`: Material properties

#### `SceneSerializer` Class
JSON-based scene persistence:
- **Serialization**: Traverses `entt::registry` and writes components to JSON
- **Deserialization**: Parses JSON and reconstructs entities/components
- **Component Handlers**: Custom serialization for `TransformComponent`, `MeshComponent`, `RigidbodyComponent`, `BoxColliderComponent`
- Uses **nlohmann/json** for JSON parsing

### Physics Systems (`Flex/Source/Physics`)

#### `JoltPhysics` Namespace
Global Jolt initialization:
- **Singleton**: `JoltPhysics::Get()` provides global access
- **Job System**: Multi-threaded physics update with configurable thread count
- **Broad Phase Layers**: Separates static and dynamic objects for efficiency
- **Collision Filters**: `ObjectLayerPairFilter` determines which layers collide

#### `JoltPhysicsScene` Class
Per-scene physics world:
- **Body Creation**: Converts ECS components to Jolt bodies
- **Simulation**: `Simulate(deltaTime)` steps physics world
- **Transform Sync**: Updates `TransformComponent` from Jolt body positions/rotations
- **Force/Impulse API**: Exposes methods to apply forces, torques, impulses
- **Collision Shapes**: Supports box and sphere colliders (extensible to capsule, convex hull, mesh)

Key Methods:
- `InstantiateEntity(entity)`: Creates Jolt body from `RigidbodyComponent` + collider components
- `SimulationStart()` / `SimulationStop()`: Activates/deactivates physics
- `AddForce()`, `AddImpulse()`, `SetLinearVelocity()`: Physics manipulation
- `CreateBoxCollider()`, `CreateSphereCollider()`: Collider instantiation

Helper Functions:
- `GlmToJoltVec3()` / `JoltToGlmVec3()`: Convert between GLM and Jolt vector types
- `GlmToJoltQuat()` / `JoltToGlmQuat()`: Convert rotations

### Mathematics (`Flex/Source/Math`)
Utility functions built on GLM:
- **Transform Composition**: Converts position/rotation/scale to matrices
- **Interpolation**: Lerp, slerp for smooth transitions
- **Random Sampling**: Hemisphere sampling for SSAO kernel generation
- **Quaternion Helpers**: Euler angle conversions, rotation utilities

## Runtime Flow
1. **Bootstrap**: `flex::App` initializes SDL3, creates an OpenGL context via `Window`, loads GL function pointers with GLAD, and prepares ImGui/ImGuizmo for editor tooling.
2. **Subsystem Wiring**: Framebuffers (`Renderer/Framebuffer`), post-process passes (`Bloom`, `SSAO`), cascaded shadow maps, and screen-space compositing shaders are constructed. A default scene is loaded or an empty scene is created.
3. **Main Loop**: SDL3 events are pumped (`Window::PollEvents`), camera and gizmo interactions are processed, then scene simulation (`Scene::Update`) and rendering (`Scene::Render`, `Scene::RenderDepth`) run each frame.
4. **Post-Processing & UI**: The HDR framebuffer is resolved through bloom, SSAO, vignette, chromatic aberration, depth-of-field, tone mapping, and exposure controls within `Screen::Render`. ImGui windows expose viewport output, scene hierarchy, property inspectors, and renderer parameters.
5. **Scene Management**: `SceneSerializer` persists entities/components to JSON, while runtime/editor scenes (`m_ActiveScene`, `m_EditorScene`) allow toggling between play mode and authoring workflows.

## Rendering Systems
- **Physically Based Rendering**: Materials pull albedo, normal, metallic-roughness, emissive, and occlusion textures; fallback meshes or glTF models are supported. Lighting integrates environment maps and cascaded directional shadows.
- **Post-Processing Stack**: `Renderer/Bloom.cpp` implements a downsample/blur/upsample chain, `Renderer/SSAO.cpp` generates ambient occlusion with configurable radius/bias/power, and the compositing shader manages depth-of-field, vignette, chromatic aberration, exposure, and gamma correction.
- **Shader Management**: `Renderer::CreateShaderFromFile` compiles shader programs, while hot-reload (via keybinding) recompiles PBR and screen shaders during runtime for rapid iteration.
- **Screen & Viewports**: `Screen::Render` binds the scene color/depth targets, uploads camera lens/post-processing uniforms, and draws a full-screen quad to present the final image in the ImGui viewport and the native window.

## Tooling & Editor Features
- **ImGui Docking UI**: `ImGuiContext` configures docking layouts, font atlases, and styling. Panels include viewport, statistics, renderer controls, scene hierarchy, and property inspectors.
- **ImGuizmo Integration**: Provides translate/rotate/scale gizmos for selected entities, synced with `Scene` transforms.
- **Asset Browsing & Dialogs**: File dialog callbacks in `App` handle importing meshes, saving/loading scenes, and tracking pending file operations safely across threads.
- **Shader & Scene Hot-Reload**: Quick iteration on GLSL shaders and scene data is encouraged; `Ctrl + R` recompiles primary render shaders, and scene serialization enables quick snapshotting of editor state.

## GitHub Actions CI Pipeline
Use GitHub Actions to guarantee that every push and pull request builds across platforms and runs the (future) unit tests.

### Goals
- Validate that the engine configures and compiles on Windows and Linux.
- Run automated tests (Google Test) once implemented.
- Produce build artifacts (optional) for quick smoke tests.
- Provide fast feedback with caching and matrix builds.

#### Notes
- The workflow uses Ninja for consistent multi-platform builds; remove the generator or adjust installation if you prefer Visual Studio on Windows.
- `submodules: recursive` is required because third-party dependencies (e.g., SDL3, Jolt) live under `thirdparty/`.
- OpenGL development headers are pre-installed on Windows images; Linux jobs add Mesa development packages for the GL loader to link successfully.
- When Google Test is wired in, `ctest` will discover and run `FlexTest`; until then, keep the step to catch regressions once the harness is ready.
- Consider adding a nightly job for Release builds with asset packaging or static analysis (clang-tidy, sanitizers) as the project matures.

## Third-Party Libraries

Flex Engine leverages well-established open-source libraries to accelerate development and focus on learning core engine concepts:

### Graphics & Windowing

**SDL3 (`thirdparty/sdl3`)**
- **Purpose**: Cross-platform windowing, input handling, and OpenGL context creation
- **Features Used**: Window management, keyboard/mouse input, event system, file dialogs
- **Why**: Industry-standard, well-documented, supports Windows, Linux, macOS

**GLAD (`thirdparty/glad`)**
- **Purpose**: OpenGL function loader
- **Features Used**: Loads OpenGL 4.6 core profile functions at runtime
- **Why**: Header-only, lightweight, supports multiple GL versions

**GLM (`thirdparty/glm`)**
- **Purpose**: Mathematics library for graphics
- **Features Used**: Vectors, matrices, quaternions, transformations, projections
- **Why**: GLSL-compatible syntax, header-only, fast compile times

### UI & Editor

**ImGui (`thirdparty/imgui`)**
- **Purpose**: Immediate-mode GUI library for editor tooling
- **Features Used**: Docking windows, property editors, viewport, statistics panels
- **Why**: Fast prototyping, no XML/designers needed, docking branch for multi-window layouts

**ImGuizmo (`thirdparty/imguizmo`)**
- **Purpose**: 3D gizmos for object manipulation
- **Features Used**: Translate, rotate, scale gizmos in viewport
- **Why**: Seamless ImGui integration, visual transform editing

### Asset Loading

**tinygltf (`thirdparty/tinygltf`)**
- **Purpose**: glTF 2.0 model loader
- **Features Used**: Mesh, material, texture, scene graph parsing
- **Why**: Header-only, supports embedded/external textures, preserves node hierarchy

**stb (`thirdparty/stb`)**
- **Purpose**: Single-file image loading/writing
- **Features Used**: `stb_image.h` for PNG/JPG/HDR loading, `stb_image_write.h` for screenshots
- **Why**: No dependencies, battle-tested, supports HDR

### Text Rendering

**FreeType (`thirdparty/freetype`)**
- **Purpose**: TrueType/OpenType font rasterization
- **Features Used**: Loads font glyphs, provides outline data to msdfgen
- **Why**: Industry-standard font engine, high-quality text

**msdfgen & msdf-atlas-gen (`thirdparty/msdfgen`, `thirdparty/msdfatlasgen`)**
- **Purpose**: Multi-channel signed distance field (MSDF) generation
- **Features Used**: Generates MSDF texture atlases for sharp text rendering at any scale
- **Why**: Superior quality vs bitmap fonts, no blurring at large sizes

### Physics

**Jolt Physics (`thirdparty/jolt`)**
- **Purpose**: High-performance 3D rigid body physics engine
- **Features Used**: Bodies, shapes (box, sphere), collision detection, constraints
- **Configuration**: Built as static library with debug renderer enabled
- **Why**: Modern C++, faster than Bullet/PhysX, excellent documentation

### Scene Management

**EnTT (`thirdparty/entt`)**
- **Purpose**: Fast and reliable entity-component-system (ECS)
- **Features Used**: Entity registry, component storage, views for iteration
- **Why**: Header-only, cache-friendly, zero-cost abstractions

**nlohmann/json (`thirdparty/json`)**
- **Purpose**: Modern C++ JSON library
- **Features Used**: Scene serialization/deserialization
- **Why**: Intuitive API, header-only, comprehensive type support

### Testing (Planned)

**Google Test (`thirdparty/googletest`)**
- **Purpose**: C++ testing framework
- **Status**: Integrated but not yet fully utilized (tests placeholder in `Tests/Source/Main.cpp`)
- **Planned Use**: Unit tests for math utilities, serialization, renderer helpers
- **Why**: Industry-standard, rich assertion macros, integrates with CTest

## Testing With Google Test
Google Test will underpin the automated verification of math helpers, renderer utilities, and scene serialization. It enables expressive unit tests with fixtures, typed tests, and parameterized scenarios while integrating cleanly with CTest.

### Integration Plan
1. **Fetch the framework**: either add it as a submodule or use CMake's `FetchContent` in `Tests/CMakeLists.txt`.
2. **Link against `gtest_main`**: replace the placeholder `main()` with `gtest_main` and register test suites spread across the `Tests/Source` tree.
3. **Expose tests to CTest**: call `enable_testing()` and `add_test` so `ctest` and the CI pipeline can execute them.

### Sample CMake Snippet
```cmake
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(FlexTest ${FLEX_TEST_SOURCES})
target_link_libraries(FlexTest PRIVATE gtest_main)
add_test(NAME FlexTests COMMAND FlexTest)
```

### Writing Tests
- Use `TEST`/`TEST_F` macros to cover math utilities (e.g., matrix conversions in `Math`), resource managers, and serialization (`SceneSerializer`).
- Mock or stub OpenGL-dependent code by isolating pure logic (SSAO kernel generation, camera parameter math) so tests run headless in CI.
- Keep tests colocated under `Tests/Source` with matching folder structure to the runtime code for discoverability.

### Current Status
- `Tests/Source/Main.cpp` currently prints "Hello World". Replace it with Google Test cases as part of the planned integration.
- Once tests exist, the CI workflow above will execute them automatically via `ctest`.

## Local Development Tips
- Configure and build locally with `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` followed by `cmake --build build`.
- Run the editor from the build output (e.g., `build/Flex/FlexEngine.exe` on Windows) to access the ImGui-driven tooling suite.
- Keep third-party libraries up to date by syncing submodules: `git submodule update --init --recursive`.
- Use the ImGui stats panel to profile FPS, delta time, and post-processing costs while tuning scenes.

## Next Steps
- Flesh out Google Test coverage for math utilities, serialization, and renderer subsystems.
- Extend CI with static analysis (clang-tidy) and packaged nightly builds for QA.
- Document asset authoring guidelines (texture formats, glTF conventions) as the content library grows.
