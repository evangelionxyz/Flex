# Flex Engine Documentation

## Engine At A Glance
- **Flex** is a modern OpenGL renderer and sandbox focused on experimenting with physically based shading, real-time post-processing, and lightweight engine architecture.
- Core tech stack: C++23, CMake, SDL3 windowing, GLAD, GLM, ImGui (docking), ImGuizmo, tinygltf, FreeType + msdfgen, Jolt Physics, and multiple in-house renderer utilities under `Flex/Source`.
- The runtime exposes extensive tweakability: shader hot-reload (`Ctrl + R`), ImGui-powered inspectors, and runtime gizmos for manipulating scene entities.
- Target use-cases: rendering R&D, prototyping game-engine subsystems, and educational exploration of GPU pipelines.

## Architecture Overview
- **Core (`Flex/Source/Core`)**: `App` drives the main loop, SDL3 event processing, frame timing, and orchestrates subsystems. `Window` wraps SDL3 + OpenGL context creation, input state tracking, and fullscreen/maximize toggles. Utility classes such as `Camera`, `Buffer`, `UUID`, and `ImGuiContext` live here to provide platform-agnostic tools.
- **Renderer (`Flex/Source/Renderer`)**: Encapsulates all GPU abstractions (vertex/index buffers, uniform buffers, framebuffer management) and mid-level features such as `Shader`, `Material`, `Bloom`, `SSAO`, `CascadedShadowMap`, and `Renderer2D`. `Screen` in `App` handles fullscreen compositing and post-processing parameter binding.
- **Scene (`Flex/Source/Scene`)**: Uses the `entt` ECS to manage entities and components, loads assets via tinygltf, and serializes/deserializes scene graphs through `SceneSerializer` (nlohmann::json). A scene can be cloned, played, or stopped to swap between editor/runtime states.
- **Physics (`Flex/Source/Physics`)**: Wraps Jolt Physics integration, exposing `JoltPhysicsScene` for runtime simulation and debug draw hooks.
- **Math (`Flex/Source/Math`)**: Houses GLM extensions and math helpers (transforms, interpolation, random sampling) consumed by camera, renderer, and SSAO kernel generation.
- **Resources (`Flex/Resources`)**: Contains bundled assets—GLTF models, HDR environment maps, PBR textures, shaders, fonts, and screenshots for reference.
- **Tests (`Tests/`)**: Placeholder CMake target `FlexTest` for unit tests; this will host Google Test suites once the testing harness is connected (see below).

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

### Recommended Workflow (`.github/workflows/ci.yml`)
```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        os: [windows-latest, ubuntu-24.04]
        build-type: [Debug, Release]
    runs-on: ${{ matrix.os }}

    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Install dependencies (Linux)
        if: contains(matrix.os, 'ubuntu')
        run: sudo apt-get update && sudo apt-get install -y mesa-common-dev libgl1-mesa-dev libxinerama-dev libxcursor-dev libxi-dev libxrandr-dev ninja-build

      - name: Install dependencies (Windows)
        if: contains(matrix.os, 'windows')
        run: choco install ninja --no-progress

      - name: Configure CMake
        run: cmake -S . -B build/${{ matrix.os }}-${{ matrix.build-type }} -G Ninja -DCMAKE_BUILD_TYPE=${{ matrix.build-type }}

      - name: Build
        run: cmake --build build/${{ matrix.os }}-${{ matrix.build-type }} --config ${{ matrix.build-type }}

      - name: Run unit tests
        run: ctest --test-dir build/${{ matrix.os }}-${{ matrix.build-type }} --output-on-failure

      - name: Upload build artifacts (optional)
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: Flex-${{ matrix.os }}-${{ matrix.build-type }}
          path: build/${{ matrix.os }}-${{ matrix.build-type }}
```

#### Notes
- The workflow uses Ninja for consistent multi-platform builds; remove the generator or adjust installation if you prefer Visual Studio on Windows.
- `submodules: recursive` is required because third-party dependencies (e.g., SDL3, Jolt) live under `thirdparty/`.
- OpenGL development headers are pre-installed on Windows images; Linux jobs add Mesa development packages for the GL loader to link successfully.
- When Google Test is wired in, `ctest` will discover and run `FlexTest`; until then, keep the step to catch regressions once the harness is ready.
- Consider adding a nightly job for Release builds with asset packaging or static analysis (clang-tidy, sanitizers) as the project matures.

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
