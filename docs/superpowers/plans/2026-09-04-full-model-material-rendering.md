# Full Model Material Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make imported OBJ/MTL assets visibly render their complete supported material data, including native TGA textures, normal/bump/displacement, legacy and PBR material terms, alpha/transparency/transmission, reflections, and imported-triangle shadow/Lumen participation.

**Architecture:** Keep the public `Models::load/spawn/loadInto` API unchanged. Add a self-contained TGA/texture layer under `Sources/Models/`, a renderer-facing material registry, material-aware mesh/tangent preprocessing, material-keyed GPU batches, an expanded opaque/masked GBuffer path, a forward transparent/transmissive path, and imported-triangle BLAS/TLAS acceleration shared by shadows and Lumen. Preserve the existing procedural scalar-material fast path and RendererCheck fixtures.

**Tech Stack:** C++17, C-BuildSystem, lwcgl v2.9.3, OpenGL 4.3 compatibility profile, GLSL 4.30, existing ECS/Renderer/Lumen code, no third-party asset/image library.

**Spec:** `docs/superpowers/specs/2026-09-04-full-model-material-rendering-design.md`

## Global Constraints

- Remain self-contained: no Assimp, tinyobjloader, stb_image, or other third-party asset/image dependency.
- Keep `Models::load`, `Models::spawn`, `Models::loadInto`, and `Models::clearCache` as the normal public model API.
- Support native TGA thoroughly; arbitrary PNG/JPG support is not part of this phase.
- Keep named RendererCheck procedural fixtures deterministic and unchanged unless a dedicated new material test explicitly requires another fixture.
- OpenGL target remains portable GL 4.3; do not require bindless textures.
- Existing procedural `Ecs::MaterialComponent` scalar use must remain valid.
- Imported material support only counts when the value visibly affects rendering; parsing-only support does not satisfy the spec.
- The Sponza submodule remains the first imported visual acceptance asset.
- Preserve persistent GPU mesh/instance resources; do not re-upload static mesh data every frame.
- Imported geometry must participate in GPU shadow/Lumen ray visibility rather than being misrepresented as analytic cubes/planes.
- Work on `main`; each completed task gets its own reviewable commit.

---

## File Structure

### New files

- `Sources/Models/Tga.hpp` / `Tga.cpp` — native TGA decoding only.
- `Sources/Models/Texture.hpp` / `Texture.cpp` — normalized-path CPU image cache and semantic helpers.
- `Sources/Renderer/Material/Material.hpp` / `Material.cpp` — renderer-facing material/texture registry and resolved render-class data.
- `Sources/Renderer/Mesh/Tangent.hpp` / `Tangent.cpp` — tangent generation and displacement preprocessing helpers.
- `Sources/Renderer/Gpu/MaterialGpu.hpp` / `MaterialGpu.cpp` — GL texture upload/cache and material binding.
- `Sources/Renderer/Gpu/TransparentGpu.hpp` / `TransparentGpu.cpp` — sorted forward transparent/transmissive pass.
- `Sources/Renderer/Gpu/TriangleScene.hpp` / `TriangleScene.cpp` — imported triangle BLAS/TLAS data and CPU build/refit logic.
- `tests/tga_contract.cpp`
- `tests/material_resolver_contract.cpp`
- `tests/tangent_displacement_contract.cpp`
- `tests/gpu_material_contract.cpp`
- `tests/triangle_scene_contract.cpp`
- `tests/sponza_material_contract.cpp`

### Existing files modified

- `Sources/Models/Material.hpp` — preserve raw MTL data and resolved texture semantics metadata.
- `Sources/Models/Obj.cpp` / `Obj.hpp` — feed document-level material compatibility information and preserve texture directives.
- `Sources/Models/Models.cpp` — resolve/register imported renderer materials and preprocess imported meshes.
- `Sources/Ecs/Ecs.hpp` — add renderer material handle without breaking scalar aggregate initialization.
- `Sources/Renderer/Mesh/Mesh.hpp` / `Mesh.cpp` — add tangent data and material-safe loaded-mesh access.
- `Sources/Renderer/Gpu/GBufferGpu.hpp` / `GBufferGpu.cpp` — material-keyed opaque/masked batches, UV/tangent shader inputs, expanded GBuffer.
- `Sources/Renderer/Gpu/DirectLightingGpu.hpp` / `DirectLightingGpu.cpp` — advanced BRDF and imported triangle shadows.
- `Sources/Renderer/Gpu/LumenGpu.hpp` / `LumenGpu.cpp` and `BvhShadersV2.hpp` — imported triangle tracing and material-aware response.
- `Sources/Renderer/Render.hpp` / `Render.cpp` — initialize/dispatch transparent pass and shared imported triangle scene.
- `Sources/Renderer/Lumen/SceneChanges.cpp` — compare renderer material handles and relevant material state.
- `build.c` — no new explicit subsystem globs; existing generic depth globs must continue to cover all new `.cpp` files.

---

### Task 1: Native TGA Decoder and CPU Texture Cache

**Files:**
- Create: `Sources/Models/Tga.hpp`
- Create: `Sources/Models/Tga.cpp`
- Create: `Sources/Models/Texture.hpp`
- Create: `Sources/Models/Texture.cpp`
- Create: `tests/tga_contract.cpp`

**Interfaces:**
- Produces:

```cpp
namespace Models::Tga {
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
    bool meaningful_alpha = false;
};
bool load(const std::string& path, Image *image, std::string *error = nullptr);
}

namespace Models {
using TextureHandle = std::uint32_t;
constexpr TextureHandle INVALID_TEXTURE = UINT32_MAX;
struct TextureAsset {
    std::string path;
    Tga::Image image;
};
TextureHandle loadTexture(const std::string& path, std::string *error = nullptr);
const TextureAsset *texture(TextureHandle handle);
void clearTextureCache();
}
```

- [ ] **Step 1: Write failing TGA contracts**

Generate tiny fixture files inside the test process and require true-color, grayscale, RLE, color-map, alpha and orientation behavior:

```cpp
Models::Tga::Image image;
std::string error;
require(Models::Tga::load("/tmp/tga-rle-top-right.tga", &image, &error));
require(image.width == 2 && image.height == 2);
require(image.rgba == expected_rgba_top_left_origin);
require(image.meaningful_alpha);
```

Also require malformed/truncated RLE and invalid dimensions to fail with a non-empty error.

- [ ] **Step 2: Run the failing contract**

Run:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/tga_contract.cpp Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  -o /tmp/tga_contract && /tmp/tga_contract
```

Expected: compile/link failure because `Tga`/`Texture` interfaces do not exist.

- [ ] **Step 3: Implement header parsing and normalized RGBA8 decoding**

Implement explicit little-endian reads and bounds checks. Normalize all decoded output to top-left RGBA8 order:

```cpp
bool load(const std::string& path, Image *image, std::string *error)
{
    std::vector<std::uint8_t> bytes = readWholeFile(path, error);
    if (bytes.size() < 18u) return fail(error, "truncated TGA header");
    Header header = parseHeader(bytes.data());
    if (!validate(header, bytes.size(), error)) return false;
    return decodePixels(bytes, header, image, error);
}
```

Cover TGA image types 1/2/3 and RLE 9/10/11, 15/16/24/32-bit true-color, valid 8-bit gray/index data, palette decode, descriptor alpha bits, and both horizontal/vertical origin flags.

- [ ] **Step 4: Implement normalized-path CPU cache**

Use `std::filesystem::absolute(...).lexically_normal()` as the cache key. `loadTexture()` decodes once and returns a stable numeric handle; `clearTextureCache()` clears only CPU texture state.

- [ ] **Step 5: Run strict decoder/cache tests**

Run the command from Step 2. Expected: PASS with all fixture assertions, including duplicate normalized-path reuse.

- [ ] **Step 6: Commit**

```bash
git add Sources/Models/Tga.* Sources/Models/Texture.* tests/tga_contract.cpp
git commit -m "Models: add native TGA texture decoding"
```

---

### Task 2: Renderer Material Registry and Exact MTL Resolution

**Files:**
- Create: `Sources/Renderer/Material/Material.hpp`
- Create: `Sources/Renderer/Material/Material.cpp`
- Modify: `Sources/Models/Material.hpp`
- Modify: `Sources/Models/Obj.hpp`
- Modify: `Sources/Models/Obj.cpp`
- Modify: `Sources/Models/Models.cpp`
- Modify: `Sources/Ecs/Ecs.hpp`
- Create: `tests/material_resolver_contract.cpp`

**Interfaces:**
- Consumes: `Models::TextureHandle`, decoded texture pixels and existing `Models::MaterialData`.
- Produces:

```cpp
namespace Renderer::Material {
using MaterialHandle = std::uint32_t;
constexpr MaterialHandle INVALID_MATERIAL = UINT32_MAX;

enum class RenderClass { Opaque, Masked, Transparent, Transmissive };
enum class ColorSpace { Linear, Srgb };

enum class Slot : std::uint8_t {
    BaseColor, Ambient, Specular, Emissive, Metallic, Roughness,
    Shininess, Opacity, Normal, Bump, Displacement, Reflection,
    Transmission, Clearcoat, ClearcoatRoughness, Sheen, Anisotropy,
    Count
};

struct TextureBinding {
    Models::TextureHandle texture = Models::INVALID_TEXTURE;
    Math::Vec3 offset = {0,0,0};
    Math::Vec3 scale = {1,1,1};
    Math::Vec3 turbulence = {0,0,0};
    float multiplier = 1.0f;
    char channel = '\0';
    bool clamp = false;
    ColorSpace color_space = ColorSpace::Linear;
};

struct Resource {
    Math::Vec3 base_color, ambient, specular, emissive, transmission_color;
    float metallic, roughness, specular_strength, shininess, ior;
    float opacity, transmission, reflectivity, clearcoat;
    float clearcoat_roughness, sheen, anisotropy;
    int illumination_model;
    RenderClass render_class;
    float alpha_cutoff;
    std::array<TextureBinding, static_cast<std::size_t>(Slot::Count)> textures;
};

MaterialHandle registerMaterial(Resource resource);
const Resource *get(MaterialHandle handle);
void clear();
}
```

Add to `Ecs::MaterialComponent` at the end so current aggregate initialization remains source-compatible:

```cpp
std::uint32_t renderer_material = Ecs::INVALID_ASSET_HANDLE;
```

- [ ] **Step 1: Write failing resolver tests**

Require exact precedence and classifications:

```cpp
require(resolveNsRoughness(7.843137f) == approx(std::sqrt(2.0f / 9.843137f)));
require(iorToF0(1.5f) == approx(0.04f));
require(resource.render_class == Renderer::Material::RenderClass::Masked);
require(resource.textures[slot(Opacity)].texture != Models::INVALID_TEXTURE);
```

Construct a 25-material Sponza-like document where at least 80% of textured non-alpha/non-transmissive materials use `d == 0`, all `map_d` materials use `d == 1`, and no competing `Tr`/`Pt` semantics exist. Require ordinary `d == 0` materials to resolve opaque only in that detected document pattern.

For opacity-map classification, use exact normalized byte thresholds: values `<= 5/255` count as zero and `>= 250/255` count as one; any selected-channel pixel between those limits makes the material `Transparent` rather than `Masked`.

- [ ] **Step 2: Run resolver tests and confirm failure**

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/material_resolver_contract.cpp \
  Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Math/Math.cpp \
  -o /tmp/material_resolver_contract && /tmp/material_resolver_contract
```

Expected: failure because registry/resolver APIs are absent.

- [ ] **Step 3: Implement material registry and legacy/PBR conversions**

Use:

```cpp
float nsToRoughness(float ns) {
    return Math::clamp(std::sqrt(2.0f / (std::max(0.0f, ns) + 2.0f)), 0.04f, 1.0f);
}

float iorToF0(float ior) {
    const float n = std::max(1.0001f, ior);
    const float r = (n - 1.0f) / (n + 1.0f);
    return r * r;
}
```

Resolve explicit roughness texture/scalar before `Ns`, explicit metallic before default 0, `Ks`/specular texture before dielectric-IOR fallback, `Pt`/transmission texture before zero, and preserve raw values in `Models::MaterialData` for diagnostics.

- [ ] **Step 4: Implement Sponza dissolve compatibility at document scope**

In `Obj::Document`, carry a boolean such as `legacy_zero_d_is_opaque`. Compute it after all MTL materials are parsed using the exact 80% pattern above. Do not globally invert `d`; apply the compatibility only to ordinary non-alpha/non-transmissive materials in that document.

- [ ] **Step 5: Register imported materials during `Models::load()`**

Change model parts to store both mesh handle and renderer material handle. `Models::spawn()` copies scalar fallback fields as today and sets `renderer_material` to the resolved registry handle.

- [ ] **Step 6: Run resolver and existing model contracts**

Run the resolver command plus:

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/models_contract.cpp Sources/Models/Obj.cpp Sources/Models/Models.cpp \
  Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Mesh/Mesh.cpp \
  Sources/Renderer/Math/Math.cpp Sources/Ecs/Ecs.cpp \
  -o /tmp/models_contract && /tmp/models_contract
```

Expected: all existing model behavior remains green and renderer material handles are valid.

- [ ] **Step 7: Commit**

```bash
git add Sources/Renderer/Material Sources/Models Sources/Ecs/Ecs.hpp tests/material_resolver_contract.cpp
git commit -m "Models: resolve complete renderer materials"
```

---

### Task 3: Tangent Generation and Static Displacement Preprocessing

**Files:**
- Create: `Sources/Renderer/Mesh/Tangent.hpp`
- Create: `Sources/Renderer/Mesh/Tangent.cpp`
- Modify: `Sources/Renderer/Mesh/Mesh.hpp`
- Modify: `Sources/Models/Models.cpp`
- Create: `tests/tangent_displacement_contract.cpp`

**Interfaces:**
- Modify vertex layout:

```cpp
struct Vertex {
    Math::Vec3 position;
    Math::Vec3 normal;
    Math::Vec4 tangent; // xyz direction, w handedness
    Math::Vec2 uv;
};
```

- Produce:

```cpp
void generateTangents(MeshData *mesh);
bool applyDisplacement(MeshData *mesh,
                       const Renderer::Material::TextureBinding& binding,
                       float strength,
                       std::string *error = nullptr);
```

- [ ] **Step 1: Write failing tangent/displacement tests**

Require a UV-mapped quad to generate tangent approximately `(1,0,0,+1)`. Require a degenerate-UV triangle to produce finite normalized tangent values. Create a 2x2 height TGA and require displacement to move vertices along normals by selected-channel height times strength.

- [ ] **Step 2: Run and confirm failure**

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/tangent_displacement_contract.cpp \
  Sources/Renderer/Mesh/Tangent.cpp Sources/Renderer/Math/Math.cpp \
  Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  -o /tmp/tangent_contract && /tmp/tangent_contract
```

- [ ] **Step 3: Implement tangent accumulation/orthogonalization**

For each indexed triangle compute UV determinant, accumulate tangent/bitangent, Gram-Schmidt tangent against final normal, derive handedness from `dot(cross(n,t), b)`. For degenerate UVs, choose a deterministic axis least aligned with the normal and cross it with the normal.

- [ ] **Step 4: Implement `_ddn` reclassification and real height displacement**

Before displacement, treat `map_Disp` paths containing `_ddn` immediately before the extension, case-insensitively, as normal/detail-normal input. For true displacement, sample transformed/clamped/repeated UVs using the selected channel, offset along current normal, recompute normals, then regenerate tangents.

- [ ] **Step 5: Update procedural meshes for the new vertex layout**

Ensure `createCube()` and `createPlane()` initialize valid tangent data so existing procedural rendering remains compatible.

- [ ] **Step 6: Run strict tests**

Run Task 3 command and existing mesh/model contracts. Expected: all pass with no NaN/Inf values.

- [ ] **Step 7: Commit**

```bash
git add Sources/Renderer/Mesh Sources/Models/Models.cpp tests/tangent_displacement_contract.cpp
git commit -m "Renderer: add tangent and displacement preprocessing"
```

---

### Task 4: GPU Texture Cache and Material Binding

**Files:**
- Create: `Sources/Renderer/Gpu/MaterialGpu.hpp`
- Create: `Sources/Renderer/Gpu/MaterialGpu.cpp`
- Create: `tests/gpu_material_contract.cpp`

**Interfaces:**

```cpp
class MaterialGpu {
public:
    bool init(std::string *error = nullptr);
    bool ensure(Renderer::Material::MaterialHandle material, std::string *error = nullptr);
    bool bind(Renderer::Material::MaterialHandle material, GLuint first_unit,
              std::string *error = nullptr);
    void shutdown();
};
```

Internally cache GL texture objects by `(Models::TextureHandle, ColorSpace)` because the same decoded pixels may need sRGB vs linear interpretation in different semantic slots.

- [ ] **Step 1: Write a source/behavior contract**

Require material upload records to distinguish sRGB base/emissive/specular color slots from linear data slots; require repeated `ensure()` for one material not to allocate new GL texture objects. Use fake lwcgl function tables as existing GPU contracts do.

- [ ] **Step 2: Run and verify RED**

Compile `tests/gpu_material_contract.cpp` with the new file paths; expect missing `MaterialGpu` failure.

- [ ] **Step 3: Implement GL upload policy**

Use `GL_SRGB8_ALPHA8` for sRGB RGBA8 color textures, `GL_RGBA8` for linear textures, semantic wrap from `TextureBinding::clamp`, linear mipmapped minification and linear magnification. Generate CPU-renormalized normal-map mip chains before upload; other slots may use `glGenerateMipmap` if available in lwcgl GL4.3 tables.

- [ ] **Step 4: Bind a fixed slot layout**

Reserve consecutive units in the GBuffer/transparent shaders with a stable order matching `Renderer::Material::Slot`. Missing textures bind texture 0 and a `uTextureMask` bitfield tells the shader to use scalar fallbacks.

- [ ] **Step 5: Run strict fake-GL contract**

Expected: PASS and allocation count remains constant across repeated `ensure()`/`bind()` calls.

- [ ] **Step 6: Commit**

```bash
git add Sources/Renderer/Gpu/MaterialGpu.* tests/gpu_material_contract.cpp
git commit -m "Renderer: add GPU material texture cache"
```

---

### Task 5: Material-Aware Opaque/Masked GBuffer

**Files:**
- Modify: `Sources/Renderer/Gpu/GBufferGpu.hpp`
- Modify: `Sources/Renderer/Gpu/GBufferGpu.cpp`
- Modify: `Sources/Renderer/Gpu/SurfaceFormats.hpp`
- Modify: `Sources/Renderer/Lumen/SceneChanges.cpp`
- Create: `tests/gpu_gbuffer_material_contract.cpp`

**Interfaces:**
- `GBufferGpu` owns/uses `MaterialGpu` for opaque/masked batches.
- Batch key becomes:

```cpp
struct BatchKey {
    std::uint32_t mesh;
    std::uint32_t material;
    Renderer::Material::RenderClass render_class;
};
```

- [ ] **Step 1: Write failing shader/source contract**

Require the committed GBuffer shader source to carry `aUv`, `aTangent`, material texture samplers, alpha discard, and resolved outputs. Require loaded entities with the same mesh but different renderer materials to form different batches.

- [ ] **Step 2: Run RED contract**

Compile the contract against current `GBufferGpu.cpp`; expected failure because UV/tangent/material sampling is absent.

- [ ] **Step 3: Extend vertex upload and shader inputs**

Add attribute 3 for tangent and forward UV/tangent/world-normal into the fragment stage. Reconstruct TBN in GLSL and apply normal map, then bump finite differences if a bump map is present.

- [ ] **Step 4: Resolve all opaque/masked material channels in the fragment shader**

Sample/apply base color, ambient, specular, emissive, metallic, roughness/shininess fallback, opacity, normal, bump, reflection scalar/color, transmission metadata, clearcoat, clearcoat roughness, sheen and anisotropy. Apply per-slot `-o`, `-s`, `-t`, clamp/repeat, `-bm` and selected channel.

- [ ] **Step 5: Implement expanded GBuffer attachments**

Use up to eight GL4.3 draw buffers with the spec semantics. Keep exact accessors in `GBufferGpu.hpp` for downstream passes:

```cpp
GLuint specularIorTexture() const;
GLuint advancedMaterialTexture() const;
GLuint transmissionTexture() const;
GLuint tangentAnisotropyTexture() const;
```

- [ ] **Step 6: Implement masked alpha behavior**

For `RenderClass::Masked`, compute resolved opacity and `discard` below `resource.alpha_cutoff` (default 0.5). Opaque never enters blend mode. Transparent/transmissive entities are skipped by this pass and collected later by `TransparentGpu`.

- [ ] **Step 7: Update change tracking**

Treat a changed `renderer_material` handle as material change, while retaining comparison of existing scalar fields.

- [ ] **Step 8: Run fake-GL/source contracts and procedural contracts**

Expected: GBuffer material contract passes; existing RendererCheck procedural source contracts continue to compile.

- [ ] **Step 9: Commit**

```bash
git add Sources/Renderer/Gpu/GBufferGpu.* Sources/Renderer/Gpu/SurfaceFormats.hpp \
  Sources/Renderer/Lumen/SceneChanges.cpp tests/gpu_gbuffer_material_contract.cpp
git commit -m "Renderer: shade textured imported materials in GBuffer"
```

---

### Task 6: Advanced Direct PBR Material Evaluation

**Files:**
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.cpp`
- Modify: `Sources/Renderer/Gpu/BvhShadersV2.hpp`
- Create: `tests/direct_material_brdf_contract.cpp`

**Interfaces:**
- Consume the expanded GBuffer channels from Task 5.
- Keep existing `dispatch(const GBufferGpu&, const Math::Vec3&, std::string*)` public signature.

- [ ] **Step 1: Write failing BRDF source/math contracts**

Require shader functions/paths for dielectric IOR F0, explicit specular, clearcoat GGX, sheen and anisotropic tangent-space roughness. Require no hardcoded-only `vec3(0.04)` path when explicit specular/IOR data exists.

- [ ] **Step 2: Run and confirm RED**

Compile the source contract; expect missing advanced channels/functions.

- [ ] **Step 3: Implement energy-aware base BRDF**

Compute dielectric F0 from resolved specular/IOR, metallic colored F0 from albedo, GGX NDF/Smith visibility, diffuse energy scaled by `(1-F)*(1-metallic)`.

- [ ] **Step 4: Add clearcoat, sheen and anisotropy**

Use tangent/bitangent from GBuffer for anisotropic distribution. Add a separate clearcoat Fresnel/GGX lobe and reduce base-layer energy by clearcoat Fresnel. Add sheen at grazing angles without adding it to metallic materials unless the material explicitly requests it.

- [ ] **Step 5: Preserve emissive and material reflectivity outputs**

Direct lighting starts from emissive as today; reflection strength is not baked into direct diffuse but remains available for the Lumen/reflection composite.

- [ ] **Step 6: Run contracts**

Expected: strict BRDF source/math contract passes and existing light-type contracts remain green.

- [ ] **Step 7: Commit**

```bash
git add Sources/Renderer/Gpu/DirectLightingGpu.* Sources/Renderer/Gpu/BvhShadersV2.hpp \
  tests/direct_material_brdf_contract.cpp
git commit -m "Renderer: evaluate complete material BRDF"
```

---

### Task 7: Forward Transparent and Transmissive Pass

**Files:**
- Create: `Sources/Renderer/Gpu/TransparentGpu.hpp`
- Create: `Sources/Renderer/Gpu/TransparentGpu.cpp`
- Modify: `Sources/Renderer/Render.hpp`
- Modify: `Sources/Renderer/Render.cpp`
- Create: `tests/transparent_material_contract.cpp`

**Interfaces:**

```cpp
class TransparentGpu {
public:
    bool init(std::string *error = nullptr);
    bool resize(int width, int height, std::string *error = nullptr);
    bool updateScene(const Ecs::World& world, std::string *error = nullptr);
    bool render(const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                GLuint opaque_color,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::string *error = nullptr);
    GLuint finalTexture() const;
    void shutdown();
};
```

- [ ] **Step 1: Write failing classification/order contract**

Create three transparent submeshes at different camera distances and require back-to-front ordering. Require masked materials not to enter this list and transmissive materials to carry IOR/Tf/transmission.

- [ ] **Step 2: Run RED contract**

Expected: missing `TransparentGpu`.

- [ ] **Step 3: Implement persistent transparent batches and sorting**

Reuse loaded mesh VAO/VBO/EBO data or share a mesh GPU resource interface rather than duplicating uploads. Sort transparent instances by camera-space depth each frame; only order records change.

- [ ] **Step 4: Implement forward shader**

Sample all relevant material textures, evaluate direct BRDF, sample opaque final color for screen-space refraction using IOR-derived offset, apply transmission color/amount, Fresnel reflection, emissive and fractional alpha. Use premultiplied-alpha blending and read-only opaque depth.

- [ ] **Step 5: Integrate render order**

In normal GPU rendering:

```text
GBuffer opaque/masked -> DirectLighting -> Lumen trace/composite -> TransparentGpu -> Presenter
```

Presenter receives `TransparentGpu::finalTexture()` when transparent geometry exists; otherwise keep the existing Lumen final texture fast path.

- [ ] **Step 6: Run contracts**

Expected: sorting/classification/source contract passes; no RendererCheck path initializes this subsystem.

- [ ] **Step 7: Commit**

```bash
git add Sources/Renderer/Gpu/TransparentGpu.* Sources/Renderer/Render.* tests/transparent_material_contract.cpp
git commit -m "Renderer: add transparent and transmissive materials"
```

---

### Task 8: Imported Triangle BLAS/TLAS Scene

**Files:**
- Create: `Sources/Renderer/Gpu/TriangleScene.hpp`
- Create: `Sources/Renderer/Gpu/TriangleScene.cpp`
- Modify: `Sources/Renderer/Mesh/Mesh.hpp`
- Create: `tests/triangle_scene_contract.cpp`

**Interfaces:**

```cpp
struct TriangleGpu {
    float p0[4], p1[4], p2[4];
    float uv0_uv1[4];
    float uv2_material[4];
    float n0[4], n1[4], n2[4];
};

class TriangleScene {
public:
    bool update(const Ecs::World& world, std::string *error = nullptr);
    GLuint triangleBuffer() const;
    GLuint blasNodeBuffer() const;
    GLuint instanceBuffer() const;
    GLuint tlasNodeBuffer() const;
    std::size_t triangleCount() const;
    void shutdown();
};
```

- [ ] **Step 1: Write failing BLAS/TLAS tests**

Build a two-triangle loaded mesh with two instances. Require one mesh-local BLAS to be reused by both instances, TLAS leaf bounds to reflect transforms, and transform-only scene updates to refit/rebuild TLAS without recreating triangle records.

- [ ] **Step 2: Run RED contract**

Compile `tests/triangle_scene_contract.cpp` with `TriangleScene.cpp`; expected missing implementation.

- [ ] **Step 3: Implement mesh-local triangle extraction and BLAS**

Build one immutable triangle array and BVH per loaded mesh handle. Store local positions, normals, UVs and renderer material handle. Reuse existing `Bvh` build/refit primitives where layouts permit; do not duplicate analytic cube/plane records.

- [ ] **Step 4: Implement instance records and TLAS**

Each imported renderable entity references mesh BLAS root, model/inverse transforms and material. TLAS bounds are transformed BLAS bounds. Geometry/topology change rebuilds affected BLAS/TLAS; transform-only change refits TLAS.

- [ ] **Step 5: Upload persistent SSBOs with dirty ranges**

Use capacity growth and `forEachDirtyRange` patterns already used by the renderer. No full re-upload on unchanged frames.

- [ ] **Step 6: Run tests**

Expected: BLAS reuse, TLAS transform update and stable triangle storage tests all pass.

- [ ] **Step 7: Commit**

```bash
git add Sources/Renderer/Gpu/TriangleScene.* Sources/Renderer/Mesh/Mesh.hpp tests/triangle_scene_contract.cpp
git commit -m "Renderer: add imported triangle acceleration"
```

---

### Task 9: Imported Triangle Shadows with Alpha Awareness

**Files:**
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.cpp`
- Modify: `Sources/Renderer/Gpu/BvhShadersV2.hpp`
- Modify: `Sources/Renderer/Render.hpp`
- Modify: `Sources/Renderer/Render.cpp`
- Create: `tests/imported_shadow_contract.cpp`

**Interfaces:**
- `DirectLightingGpu` receives/binds shared `TriangleScene` buffers during dispatch without changing the public scene-facing ECS API.

- [ ] **Step 1: Write failing shadow-source contract**

Require the GPU shadow function to test both analytic primitives and imported triangle TLAS/BLAS. Require masked intersections to sample opacity coverage and continue the ray when the sampled alpha is below cutoff.

- [ ] **Step 2: Run RED**

Expected: current shader only intersects analytic cube/plane primitives.

- [ ] **Step 3: Add TLAS/BLAS traversal and triangle intersection to direct-light shader**

Use Möller-Trumbore or equivalent watertight-enough triangle intersection, transforming rays into instance local space before BLAS traversal. Return hit distance, barycentrics and material/UV information.

- [ ] **Step 4: Add alpha-aware shadow continuation**

Resolve material opacity at hit UV. Masked holes do not terminate shadow rays. Opaque hits terminate. Transparent/transmissive hits attenuate shadow energy according to resolved opacity/transmission rather than becoming binary blockers.

- [ ] **Step 5: Share `TriangleScene` lifetime in renderer**

Update it only when geometry/material/transform changes require it and bind its buffers for direct lighting.

- [ ] **Step 6: Run contracts**

Expected: imported shadow source contract and existing analytic BVH contracts both pass.

- [ ] **Step 7: Commit**

```bash
git add Sources/Renderer/Gpu/DirectLightingGpu.* Sources/Renderer/Gpu/BvhShadersV2.hpp \
  Sources/Renderer/Render.* tests/imported_shadow_contract.cpp
git commit -m "Renderer: trace imported mesh shadows"
```

---

### Task 10: Material-Aware Lumen and Imported Triangle Ray Hits

**Files:**
- Modify: `Sources/Renderer/Gpu/LumenGpu.hpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.cpp`
- Modify: `Sources/Renderer/Gpu/BvhShadersV2.hpp`
- Modify: `Sources/Renderer/Render.cpp`
- Create: `tests/lumen_imported_material_contract.cpp`

**Interfaces:**
- `LumenGpu::traceShared()` additionally consumes the renderer's shared `TriangleScene` through a const reference:

```cpp
bool traceShared(const GBufferGpu& gbuffer,
                 const DirectLightingGpu& direct,
                 const TriangleScene& triangles,
                 const Math::Mat4& view,
                 const Math::Mat4& projection,
                 const Math::Vec3& camera_position,
                 std::uint64_t frame_index,
                 std::string *error = nullptr);
```

- [ ] **Step 1: Write failing Lumen source contract**

Require shared trace shader code to traverse imported TLAS/BLAS after/alongside screen and analytic scene tests, return imported hit UV/material, and evaluate textured base/emissive/roughness/metallic/reflectivity at the hit.

- [ ] **Step 2: Run RED**

Expected: current Lumen path only has analytic primitive buffer and screen/GBuffer data.

- [ ] **Step 3: Add imported ray traversal to Lumen shader**

Bind TriangleScene buffers and choose the nearest valid analytic/imported hit. Preserve existing BVH benchmark path for procedural primitives.

- [ ] **Step 4: Add GL4.3 trace texture access without bindless textures**

Build semantic texture-array/atlas pages from imported material textures for ray shading. Store per-material page/layer plus UV scale/bias in a material SSBO. Repeat textures tile in material UV space before atlas remap; clamp textures clamp before remap. Use separate sRGB-decoded color sampling behavior and linear data pages.

- [ ] **Step 5: Evaluate imported hit material**

At off-screen hits use textured base color, mapped/geometry normal as available, metallic, roughness, emissive, specular/IOR, reflectivity and clearcoat-relevant response. Masked holes continue tracing; transmissive hits use a bounded continuation approximation rather than becoming opaque.

- [ ] **Step 6: Update reflection composite weighting**

Use GBuffer reflectivity/specular/roughness/clearcoat channels to control reflection contribution rather than applying one generic reflection strength.

- [ ] **Step 7: Run contracts**

Expected: imported Lumen material contract and existing GPU Lumen scheduling/hot-path contracts pass.

- [ ] **Step 8: Commit**

```bash
git add Sources/Renderer/Gpu/LumenGpu.* Sources/Renderer/Gpu/BvhShadersV2.hpp \
  Sources/Renderer/Render.cpp tests/lumen_imported_material_contract.cpp
git commit -m "Lumen: trace textured imported geometry"
```

---

### Task 11: Sponza End-to-End Material Acceptance

**Files:**
- Create: `tests/sponza_material_contract.cpp`
- Modify only if required by verified failure: `Sources/Models/Models.cpp`, `Sources/Models/Obj.cpp`, renderer material/GPU files from prior tasks.

**Interfaces:**
- Uses `Assets/Sponza/sponza.obj` and `Assets/Sponza/sponza.mtl` as the first real imported asset.

- [ ] **Step 1: Write Sponza asset contract**

Load the real submodule asset and assert:

```cpp
require(model != Models::INVALID_MODEL);
require(countResolvedBaseColorTextures() >= 20u);
require(countResolvedNormalOrDdnTextures() >= 15u);
require(countResolvedOpacityTextures() >= 3u);
require(allReferencedSponzaTgaFilesDecode());
require(noOrdinarySponzaMaterialResolvedFullyTransparent());
```

Also assert `leaf`, `chain`, and `Material__57` become masked/alpha-aware and ordinary arch/floor/column/fabric materials resolve opaque.

- [ ] **Step 2: Run CPU Sponza contract**

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/sponza_material_contract.cpp \
  Sources/Models/Obj.cpp Sources/Models/Models.cpp Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Mesh/Mesh.cpp Sources/Renderer/Mesh/Tangent.cpp \
  Sources/Renderer/Math/Math.cpp Sources/Ecs/Ecs.cpp \
  -o /tmp/sponza_material_contract && /tmp/sponza_material_contract
```

Expected: PASS.

- [ ] **Step 3: Run full project build**

```bash
git submodule update --init --recursive
c build
```

Expected: exit 0 with all production `.cpp` files compiled by the existing generic source globs.

- [ ] **Step 4: Launch interactive Sponza**

```bash
c build run
```

Visual acceptance requires:

- colored diffuse textures are visible instead of gray scalar-only shading;
- columns/floor/brick/curtains show normal/detail-map response;
- leaves/chains/plants show cutout holes rather than solid cards;
- specular/roughness response varies materially under the scene light;
- imported Sponza geometry casts/receives GPU shadows;
- indirect/reflection response respects imported geometry/materials;
- no widespread disappearance from the Sponza `d 0` exporter convention.

Capture a screenshot for review but do not add it to the repository unless explicitly requested.

- [ ] **Step 5: Run RendererCheck regression suite once at phase end**

Do not trigger per-commit workflows. Run the local/offline RendererCheck suite or the repository's existing final validation command once after all tasks are complete. Named fixtures must remain unchanged unless a new dedicated test was intentionally added.

- [ ] **Step 6: Commit any acceptance-only fixes**

If Step 4 reveals a verified asset-specific bug, fix only the root cause, rerun Steps 2-5, then commit:

```bash
git add <verified-fix-files>
git commit -m "Models: finish Sponza material rendering"
```

If no fixes are needed, do not create an empty commit.

---

### Task 12: Cleanup, Lifetime, Performance, and Final Verification

**Files:**
- Modify as required by measured leaks/redundant work: material/texture/triangle GPU lifecycle files.
- Test existing performance/hot-path contracts plus any new lifecycle contract needed.

**Interfaces:**
- `Models::clearCache()` must clear model CPU state and renderer material registrations only when caller explicitly requests teardown.
- `renderer.shutdown()` owns GL resource deletion for GPU material textures, triangle buffers and transparent targets.

- [ ] **Step 1: Add lifecycle contract**

Require repeated model/material lookup not to grow CPU/GPU registries, repeated unchanged frames not to upload static texture/mesh/triangle data, and shutdown to delete every allocated GL object exactly once under fake GL counters.

- [ ] **Step 2: Run RED if lifecycle gaps are found**

Use existing fake-GL patterns; expect the test to fail on the exact leak/redundant upload before changing production code.

- [ ] **Step 3: Fix only measured lifecycle/hot-path issues**

Keep normalized path caches, material registry stability, persistent buffers, dirty-range uploads, and no per-frame TGA decoding. Do not introduce speculative streaming/async systems in this phase.

- [ ] **Step 4: Run strict focused contracts**

Run all new contracts:

```bash
for test in \
  tga_contract \
  material_resolver_contract \
  tangent_displacement_contract \
  gpu_material_contract \
  gpu_gbuffer_material_contract \
  direct_material_brdf_contract \
  transparent_material_contract \
  triangle_scene_contract \
  imported_shadow_contract \
  lumen_imported_material_contract \
  sponza_material_contract
  do
    ./build/tests/$test
  done
```

If the project does not generate standalone test binaries automatically, compile each with the exact per-task command above and require exit 0.

- [ ] **Step 5: Run final build and runtime verification**

```bash
c build
c build run
```

Require build exit 0 and visually verify the Task 11 criteria.

- [ ] **Step 6: Verify branch diff and no dependency drift**

```bash
git status --short
git diff --stat <plan-start-commit>..HEAD
grep -RniE 'stb_image|tinyobj|assimp' Sources build.c || true
```

Expected: clean worktree; only intended model/material/renderer/tests/docs changes; no third-party image/model dependency.

- [ ] **Step 7: Commit final lifecycle/performance fixes if any**

```bash
git add Sources tests
git commit -m "Renderer: finalize imported material pipeline"
```

Skip this commit if Step 3 required no code changes.
