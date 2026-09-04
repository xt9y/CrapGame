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
- `tests/gpu_gbuffer_material_contract.cpp`
- `tests/direct_material_brdf_contract.cpp`
- `tests/transparent_material_contract.cpp`
- `tests/triangle_scene_contract.cpp`
- `tests/imported_shadow_contract.cpp`
- `tests/lumen_imported_material_contract.cpp`
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

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/tga_contract.cpp Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  -o /tmp/tga_contract && /tmp/tga_contract
```

Expected: compile/link failure because `Tga`/`Texture` interfaces do not exist.

- [ ] **Step 3: Implement header parsing and normalized RGBA8 decoding**

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

Cover TGA image types 1/2/3 and RLE 9/10/11, 15/16/24/32-bit true-color, valid 8-bit gray/index data, palette decode, descriptor alpha bits, and both horizontal/vertical origin flags. Normalize all output to top-left RGBA8.

- [ ] **Step 4: Implement normalized-path CPU cache**

Use `std::filesystem::absolute(...).lexically_normal()` as the cache key. `loadTexture()` decodes once and returns a stable numeric handle; `clearTextureCache()` clears only CPU texture state.

- [ ] **Step 5: Run strict decoder/cache tests**

Run Step 2. Expected: PASS including duplicate normalized-path reuse.

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

Append to `Ecs::MaterialComponent` so existing aggregate initializers remain valid:

```cpp
std::uint32_t renderer_material = Ecs::INVALID_ASSET_HANDLE;
```

- [ ] **Step 1: Write failing resolver tests**

```cpp
require(resolveNsRoughness(7.843137f) == approx(std::sqrt(2.0f / 9.843137f)));
require(iorToF0(1.5f) == approx(0.04f));
require(resource.render_class == Renderer::Material::RenderClass::Masked);
require(resource.textures[slot(Opacity)].texture != Models::INVALID_TEXTURE);
```

Construct a 25-material Sponza-like document where at least 80% of textured non-alpha/non-transmissive materials use `d == 0`, all `map_d` materials use `d == 1`, and no competing `Tr`/`Pt` semantics exist. Require ordinary `d == 0` materials to resolve opaque only in that detected document pattern.

For opacity-map classification, normalized byte values `<= 5/255` count as zero and `>= 250/255` count as one; any selected-channel pixel between those limits makes the material `Transparent` instead of `Masked`.

- [ ] **Step 2: Run resolver tests and confirm failure**

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/material_resolver_contract.cpp \
  Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Math/Math.cpp \
  -o /tmp/material_resolver_contract && /tmp/material_resolver_contract
```

- [ ] **Step 3: Implement exact legacy/PBR conversions**

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

Resolve explicit roughness texture/scalar before `Ns`, explicit metallic before default 0, `Ks`/specular texture before dielectric-IOR fallback, `Pt`/transmission texture before zero, and preserve raw values in `Models::MaterialData`.

- [ ] **Step 4: Implement document-scope Sponza dissolve compatibility**

Add `bool legacy_zero_d_is_opaque = false;` to `Obj::Document`. Set it only when the exact 80% pattern from Step 1 is satisfied. Do not globally invert `d`.

- [ ] **Step 5: Register imported renderer materials during `Models::load()`**

Each model part stores mesh handle plus `Renderer::Material::MaterialHandle`. `Models::spawn()` copies existing scalar fallback fields and assigns `renderer_material`.

- [ ] **Step 6: Run resolver and existing model contracts**

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/models_contract.cpp Sources/Models/Obj.cpp Sources/Models/Models.cpp \
  Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Mesh/Mesh.cpp \
  Sources/Renderer/Math/Math.cpp Sources/Ecs/Ecs.cpp \
  -o /tmp/models_contract && /tmp/models_contract
```

Expected: existing model contract plus renderer-material assertions pass.

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
- Modify: `Sources/Renderer/Mesh/Mesh.cpp`
- Modify: `Sources/Models/Models.cpp`
- Create: `tests/tangent_displacement_contract.cpp`

**Interfaces:**

```cpp
struct Vertex {
    Math::Vec3 position;
    Math::Vec3 normal;
    Math::Vec4 tangent;
    Math::Vec2 uv;
};

void generateTangents(MeshData *mesh);
bool applyDisplacement(MeshData *mesh,
                       const Renderer::Material::TextureBinding& binding,
                       float strength,
                       std::string *error = nullptr);
```

- [ ] **Step 1: Write failing tangent/displacement tests**

Require a UV-mapped quad to generate tangent approximately `(1,0,0,+1)`, a degenerate-UV triangle to produce finite normalized tangent values, and a 2x2 height texture to move vertices along normals by sampled selected-channel height times strength.

- [ ] **Step 2: Run and confirm failure**

```bash
c++ -std=c++17 -Wall -Wextra -Werror -I Sources \
  tests/tangent_displacement_contract.cpp \
  Sources/Renderer/Mesh/Tangent.cpp Sources/Renderer/Math/Math.cpp \
  Sources/Models/Tga.cpp Sources/Models/Texture.cpp \
  -o /tmp/tangent_contract && /tmp/tangent_contract
```

- [ ] **Step 3: Implement tangent accumulation and fallback**

For each indexed triangle compute UV determinant, accumulate tangent/bitangent, Gram-Schmidt tangent against final normal, and derive handedness from `dot(cross(n,t), b)`. Degenerate UVs choose the axis least aligned with the normal and cross it with the normal.

- [ ] **Step 4: Implement `_ddn` reclassification and real height displacement**

`map_Disp` whose filename stem ends in `_ddn`, case-insensitively, becomes normal/detail-normal input. Other displacement samples transformed/clamped/repeated UV, applies selected channel and strength along the current normal, recomputes normals, then regenerates tangents.

- [ ] **Step 5: Update procedural mesh vertex initialization**

Make `createCube()` and `createPlane()` initialize valid tangent data.

- [ ] **Step 6: Run Task 3 and existing mesh/model contracts**

Expected: all pass and no NaN/Inf tangent values exist.

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

Cache GL textures by `(Models::TextureHandle, Renderer::Material::ColorSpace)`.

- [ ] **Step 1: Write failing fake-GL contract**

Require sRGB base/emissive/specular-color upload, linear data upload, wrap policy, and no extra GL allocation on repeated `ensure()` for the same material.

- [ ] **Step 2: Run RED contract**

Compile `tests/gpu_material_contract.cpp` against `MaterialGpu.cpp`. Expected: missing implementation.

- [ ] **Step 3: Implement upload policy**

Use `GL_SRGB8_ALPHA8` for sRGB RGBA8, `GL_RGBA8` for linear RGBA8, `TextureBinding::clamp` for wrapping, mipmapped linear minification and linear magnification. Generate CPU-renormalized normal-map mip chains; other slots may use GL mip generation.

- [ ] **Step 4: Implement stable slot binding**

Bind slots in `Renderer::Material::Slot` order. Missing textures bind texture 0 and a bitfield uniform selects scalar fallback.

- [ ] **Step 5: Run fake-GL contract**

Expected: PASS with stable allocation counts.

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

```cpp
struct BatchKey {
    std::uint32_t mesh;
    std::uint32_t material;
    Renderer::Material::RenderClass render_class;
};
```

`GBufferGpu` owns or receives one `MaterialGpu` cache and only emits Opaque/Masked batches.

- [ ] **Step 1: Write failing shader/source contract**

Require `aUv`, `aTangent`, material samplers, alpha discard, and separate batches for one mesh with different renderer materials.

- [ ] **Step 2: Run RED contract**

Expected: current GBuffer lacks material texture sampling.

- [ ] **Step 3: Extend vertex upload and shader varyings**

Add tangent attribute location 3; forward UV, transformed tangent and world normal. Build TBN in the fragment stage and apply normal then bump perturbation.

- [ ] **Step 4: Resolve every opaque/masked channel**

Sample/apply base color, ambient, specular, emissive, metallic, roughness/shininess, opacity, normal, bump, reflection, transmission metadata, clearcoat, clearcoat roughness, sheen and anisotropy. Apply `-o`, `-s`, `-t`, clamp/repeat, `-bm` and selected channel.

- [ ] **Step 5: Add expanded GBuffer attachments**

Provide:

```cpp
GLuint specularIorTexture() const;
GLuint advancedMaterialTexture() const;
GLuint transmissionTexture() const;
GLuint tangentAnisotropyTexture() const;
```

Keep position/depth, normal/roughness, albedo/metallic and emissive/opacity attachments from the spec.

- [ ] **Step 6: Add masked alpha discard**

Default cutoff is `0.5f`. Opaque never blends. Transparent/Transmissive entities are excluded from GBuffer.

- [ ] **Step 7: Update scene-change tracking**

Compare `renderer_material` in addition to existing scalar material state.

- [ ] **Step 8: Run fake-GL/source and procedural contracts**

Expected: new GBuffer contract passes and procedural RendererCheck contracts still compile.

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
- Keep `dispatch(const GBufferGpu&, const Math::Vec3&, std::string*)` unchanged.
- Consume Task 5 GBuffer attachments.

- [ ] **Step 1: Write failing BRDF source/math contract**

Require dielectric IOR F0, explicit specular, clearcoat GGX, sheen and anisotropic tangent-space roughness; reject a hardcoded-only `vec3(0.04)` material path.

- [ ] **Step 2: Run RED contract**

Expected: advanced channels/functions absent.

- [ ] **Step 3: Implement energy-aware base BRDF**

Use resolved specular/IOR for dielectric F0, metallic colored F0 from albedo, GGX NDF/Smith visibility, and diffuse weight `(1-F)*(1-metallic)`.

- [ ] **Step 4: Add clearcoat, sheen and anisotropy**

Use tangent/bitangent for anisotropic distribution, add a second clearcoat Fresnel/GGX lobe and reduce base-layer energy by clearcoat Fresnel, then add sheen at grazing angles.

- [ ] **Step 5: Preserve emissive and reflection controls**

Direct starts from emissive; reflectivity remains available to Lumen/reflection composite instead of being folded into diffuse.

- [ ] **Step 6: Run BRDF and existing lighting contracts**

Expected: all pass.

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

Require three transparent submeshes to sort back-to-front, Masked materials to stay out of this pass, and Transmissive materials to preserve IOR/Tf/transmission.

- [ ] **Step 2: Run RED contract**

Expected: `TransparentGpu` absent.

- [ ] **Step 3: Implement persistent batches and camera-depth sorting**

Share loaded mesh GPU resources rather than re-uploading geometry. Sort only instance/order records each frame.

- [ ] **Step 4: Implement forward material shader**

Sample complete material textures, evaluate direct BRDF, sample opaque color for screen-space refraction, apply IOR, transmission color/amount, Fresnel reflection, emissive and fractional alpha. Use premultiplied-alpha blending and read-only opaque depth.

- [ ] **Step 5: Integrate render order**

```text
GBuffer opaque/masked -> DirectLighting -> Lumen trace/composite -> TransparentGpu -> Presenter
```

Use the existing Lumen final texture directly when no transparent geometry exists.

- [ ] **Step 6: Run contracts**

Expected: transparent contract passes and RendererCheck never initializes this pass.

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

A two-triangle loaded mesh with two instances must reuse one mesh-local BLAS, create transformed TLAS bounds per instance, and update transforms without recreating triangle records.

- [ ] **Step 2: Run RED contract**

Expected: `TriangleScene` absent.

- [ ] **Step 3: Implement immutable local triangle records and one BLAS per loaded mesh**

Store local positions, normals, UVs and material handle. Reuse existing BVH build/refit helpers where layouts permit.

- [ ] **Step 4: Implement instance records and TLAS**

Instance records contain BLAS root, model/inverse transform and material. Geometry/topology changes rebuild affected BLAS/TLAS; transform-only changes refit TLAS.

- [ ] **Step 5: Upload persistent SSBOs with dirty ranges**

Use existing capacity-growth and `forEachDirtyRange` patterns.

- [ ] **Step 6: Run triangle-scene contract**

Expected: BLAS reuse, TLAS update and stable triangle storage pass.

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
- `DirectLightingGpu` binds shared `TriangleScene` buffers; ECS API remains unchanged.

- [ ] **Step 1: Write failing shadow-source contract**

Require both analytic and imported triangle traversal and require masked intersections below cutoff to continue the ray.

- [ ] **Step 2: Run RED contract**

Expected: current shader only intersects analytic cubes/planes.

- [ ] **Step 3: Add TLAS/BLAS traversal and triangle intersection**

Transform rays into instance-local space, traverse BLAS and return nearest distance, barycentrics, UV and material handle.

- [ ] **Step 4: Add alpha-aware shadow continuation**

Masked holes continue; opaque hits terminate; transparent/transmissive hits attenuate according to resolved opacity/transmission.

- [ ] **Step 5: Integrate `TriangleScene` lifetime**

Update shared triangle state only on geometry/material/transform changes that require it and bind it for direct lighting.

- [ ] **Step 6: Run imported-shadow and analytic-BVH contracts**

Expected: both pass.

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

Require imported TLAS/BLAS traversal, nearest imported hit UV/material, textured base/emissive/roughness/metallic/reflectivity evaluation, and alpha-aware continuation.

- [ ] **Step 2: Run RED contract**

Expected: current Lumen trace only has procedural analytic primitives plus screen/GBuffer data.

- [ ] **Step 3: Add imported traversal to Lumen shader**

Bind TriangleScene buffers and choose the nearest valid analytic/imported hit while preserving existing procedural BVH benchmark behavior.

- [ ] **Step 4: Add GL4.3 trace texture pages**

Build semantic texture arrays/atlas pages for ray shading. Store per-material page/layer and UV scale/bias in a material SSBO. Repeat in material UV space before page remap; clamp before remap. Separate color and linear-data pages.

- [ ] **Step 5: Evaluate imported hit material**

Use textured base color, normal, metallic, roughness, emissive, specular/IOR, reflectivity and clearcoat-relevant response. Masked holes continue tracing; transmissive hits use bounded continuation instead of becoming opaque.

- [ ] **Step 6: Weight reflection composite by material**

Use reflectivity/specular/roughness/clearcoat channels instead of one generic reflection strength.

- [ ] **Step 7: Run imported-Lumen and existing Lumen hot-path contracts**

Expected: all pass.

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
- Candidate verified-fix files only after a failing acceptance check: `Sources/Models/Models.cpp`, `Sources/Models/Obj.cpp`, `Sources/Renderer/Material/Material.cpp`, `Sources/Renderer/Gpu/GBufferGpu.cpp`, `Sources/Renderer/Gpu/MaterialGpu.cpp`, `Sources/Renderer/Gpu/DirectLightingGpu.cpp`, `Sources/Renderer/Gpu/LumenGpu.cpp`, `Sources/Renderer/Gpu/TransparentGpu.cpp`, `Sources/Renderer/Gpu/TriangleScene.cpp`.

**Interfaces:**
- Uses `Assets/Sponza/sponza.obj` and `Assets/Sponza/sponza.mtl`.

- [ ] **Step 1: Write Sponza asset contract**

```cpp
require(model != Models::INVALID_MODEL);
require(countResolvedBaseColorTextures() >= 20u);
require(countResolvedNormalOrDdnTextures() >= 15u);
require(countResolvedOpacityTextures() >= 3u);
require(allReferencedSponzaTgaFilesDecode());
require(noOrdinarySponzaMaterialResolvedFullyTransparent());
```

Also require `leaf`, `chain`, and `Material__57` to be alpha-aware and ordinary arch/floor/column/fabric materials to resolve opaque.

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

Expected: exit 0.

- [ ] **Step 4: Launch interactive Sponza**

```bash
c build run
```

Visual acceptance requires colored diffuse textures, normal/detail response on architecture/floor/curtains, cutout foliage/chain holes, material-varying specular/roughness, imported geometry shadows, imported GI/reflection participation, and no widespread disappearance from `d 0`.

- [ ] **Step 5: Run RendererCheck regression suite once at phase end**

Use the repository's existing local/offline RendererCheck validation once after all tasks; do not trigger a workflow per commit.

- [ ] **Step 6: Fix only a reproduced acceptance failure and commit**

After reproducing a failure, modify only the relevant candidate file(s), rerun Steps 2-5, then stage exactly the changed candidate paths:

```bash
git diff --name-only -- \
  Sources/Models/Models.cpp Sources/Models/Obj.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Gpu \
  | xargs -r git add --
git commit -m "Models: finish Sponza material rendering"
```

Skip the commit when no acceptance fix is required.

---

### Task 12: Cleanup, Lifetime, Performance, and Final Verification

**Files:**
- Potentially modify after a failing lifecycle contract: `Sources/Models/Texture.cpp`, `Sources/Models/Models.cpp`, `Sources/Renderer/Material/Material.cpp`, `Sources/Renderer/Gpu/MaterialGpu.cpp`, `Sources/Renderer/Gpu/TriangleScene.cpp`, `Sources/Renderer/Gpu/TransparentGpu.cpp`, `Sources/Renderer/Render.cpp`.
- Create: `tests/imported_material_lifecycle_contract.cpp`.

**Interfaces:**
- `Models::clearCache()` clears model CPU state and renderer material registrations only on explicit teardown.
- `renderer.shutdown()` deletes GPU material textures, triangle buffers and transparent targets.

- [ ] **Step 1: Write lifecycle contract**

Require repeated model/material lookup not to grow registries, unchanged frames not to upload static texture/mesh/triangle data, and shutdown to delete each fake-GL object exactly once.

- [ ] **Step 2: Run lifecycle contract before production changes**

If it passes, no lifecycle production change is needed. If it fails, record the exact counter/assertion and proceed to Step 3.

- [ ] **Step 3: Fix only the failing lifetime/hot-path assertion**

Preserve normalized path caches, stable material handles, persistent buffers, dirty-range uploads, and zero per-frame TGA decoding. Do not add streaming or asynchronous loading in this phase.

- [ ] **Step 4: Run all focused contracts**

Run the exact per-task compile commands above for:

```text
tga_contract
material_resolver_contract
tangent_displacement_contract
gpu_material_contract
gpu_gbuffer_material_contract
direct_material_brdf_contract
transparent_material_contract
triangle_scene_contract
imported_shadow_contract
lumen_imported_material_contract
sponza_material_contract
imported_material_lifecycle_contract
```

Require exit 0 for every contract.

- [ ] **Step 5: Run final build and runtime verification**

```bash
c build
c build run
```

Require build exit 0 and visually re-check Task 11.

- [ ] **Step 6: Verify branch diff and dependency constraints**

```bash
git status --short
git diff --stat b6aefac9339a9aab7522b31184722bb269abdd07..HEAD
grep -RniE 'stb_image|tinyobj|assimp' Sources build.c || true
```

Expected: clean worktree, intended files only, no prohibited dependency.

- [ ] **Step 7: Commit lifecycle/performance fixes only if Step 3 changed code**

```bash
git add Sources/Models/Texture.cpp Sources/Models/Models.cpp \
  Sources/Renderer/Material/Material.cpp Sources/Renderer/Gpu/MaterialGpu.cpp \
  Sources/Renderer/Gpu/TriangleScene.cpp Sources/Renderer/Gpu/TransparentGpu.cpp \
  Sources/Renderer/Render.cpp tests/imported_material_lifecycle_contract.cpp
git commit -m "Renderer: finalize imported material pipeline"
```

If Step 2 was already green, add only `tests/imported_material_lifecycle_contract.cpp` to the most recent task commit rather than creating a production-code cleanup commit.
