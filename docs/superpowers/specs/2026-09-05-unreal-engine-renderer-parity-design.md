# Unreal Engine Renderer Parity Design

## Scope

Implement every renderer feature documented in `docs/superpowers/unreal_engine_repo_deep_analysis.txt` inside `xt9y/CrapGame`, preserving the current CrapGame source architecture, class boundaries, naming, file organization, public frontend, coding style, and performance-first design.

The target is behavioral and architectural reproduction of the inspected Unreal Engine 5.8.2 renderer systems without copying Unreal source code verbatim.

The implementation remains strictly on the existing `lwcgl` stack and the `xt9y/lwcgl` `v2.9.3` branch. `lwcgl` may only expose functions, classes, constants, overloads, or extension entry points that actually existed in LWJGL 2.9.3. No CrapGame-specific renderer API may be added to `lwcgl`.

Where Unreal uses DXR or other hardware-ray-tracing APIs that cannot exist in the LWJGL 2.9.3 stack, CrapGame reproduces the same renderer role using its software BVH, mesh-distance-field, global-distance-field, screen-space, cache, and compute paths.

The authoritative completeness checklist is:

`docs/superpowers/unreal_engine_repo_deep_analysis.txt`

No feature documented there may be silently omitted.

## Permanent source-style constraints

The existing repository style is authoritative.

- `Renderer::Rendering` remains the only renderer frontend.
- `main.cpp` remains thin.
- ECS remains world-description data only.
- Renderer internals remain behind `Renderer::Rendering`.
- GPU renderer files remain under `Sources/Renderer/Gpu/`.
- CPU/reference Lumen files remain under `Sources/Renderer/Lumen/`.
- CPU/reference shadow files remain under `Sources/Renderer/Shadows/`.
- PascalCase directories/files are preserved.
- Function names remain camelCase.
- Variables remain snake_case.
- Existing brace placement, spacing, wrapping, aligned declarations, grouped declarations, and include ordering are preserved exactly.
- Existing classes are extended rather than replaced by a new monolithic renderer.
- New public frontend classes are not introduced.
- Existing minimal `init`, `resize`, `render`, `shutdown`, `ready`, texture/buffer access patterns remain the model for new GPU classes.
- Shader source remains embedded in the same project style unless an existing local pattern requires otherwise.

## Permanent platform constraints

Only `lwcgl` `v2.9.3` is permitted for rendering/window/input access.

Allowed graphics functionality is limited to functionality exposed by actual LWJGL 2.9.3-era bindings, including the existing GL11/GL15/GL20/GL30/GL31/GL32/GL33/GL42/GL43 surfaces and verified LWJGL 2.9.3 extension surfaces when required.

Before adding any missing function to `lwcgl`:

1. Verify that the corresponding function/class/extension existed in LWJGL 2.9.3.
2. Add only a source-shaped or native-equivalent binding.
3. Keep the binding generic and renderer-independent.
4. Add compatibility coverage in `lwcgl`.
5. Use the new binding from CrapGame only after that compatibility work exists.

No Vulkan, Direct3D, DXR, Metal, CUDA, OptiX, newer LWJGL API, or custom pseudo-hardware-RT abstraction is allowed.

## Feature status model

Every item from the deep analysis is assigned one of these implementation statuses during development:

### Exact behavioral reproduction

Used when LWJGL 2.9.3/OpenGL can represent the required architecture directly.

Examples:

- virtual shadow address spaces
- sparse physical page pools
- page tables and mips
- directional clipmaps
- receiver-driven page allocation
- page caching and invalidation
- SMRT sampling logic
- GGX/Smith/Fresnel BRDFs
- Surface Cache
- Radiance Cache
- Screen Probe Gather
- radiosity
- short-range AO
- temporal reconstruction
- reflection filtering
- ReSTIR reservoir logic

### Software equivalent

Used when Unreal performs the role with hardware RT but the LWJGL 2.9.3 stack cannot.

Examples:

- hardware reflection fallback becomes compacted BVH/SDF fallback
- hit lighting becomes software hit lighting from triangle/material/light data
- hardware short-range AO becomes software BVH/SDF AO for unresolved rays
- hardware far-field reflection rays become software far-field BVH/SDF rays

The software implementation must preserve the same stage ordering, fallback semantics, filtering inputs, cache interaction, and bounded-work behavior where practical.

### Platform-inapplicable primitive

Used only for an API primitive that fundamentally cannot exist on the stack. This does not permit dropping the renderer feature that depended on it. The feature still receives an exact or software-equivalent implementation.

## Existing renderer boundaries

### `Renderer::Rendering`

Remains the frame-level orchestration boundary.

It continues to own and schedule:

- `Gpu::GBufferGpu`
- `Gpu::DirectLightingGpu`
- `Gpu::LumenGpu`
- `Gpu::LumenSchedule`
- convergence state
- revision state
- profiler state
- presentation
- CPU/reference renderer state

Its public interface remains unchanged unless an existing renderer requirement already forces a compatible addition.

### `Gpu::DirectLightingGpu`

Remains the direct-light orchestrator.

It will own or coordinate the UE-style direct-light visibility stack rather than being replaced.

The current single-map/static-PCF and binary-any-hit paths are retained only while migration stages need them and are removed from the active final path once VSM/SMRT equivalents are validated.

### `Gpu::LumenGpu`

Remains the GPU GI/reflection orchestrator.

It will coordinate expanded cache, tracing, probe, radiosity, AO, reflection, ReSTIR, reprojection, and composition systems rather than being replaced by a new renderer class.

### CPU/reference renderer

The CPU path remains deterministic and becomes the correctness/reference counterpart for major algorithms where practical. RendererCheck continues to validate actual renderer behavior rather than a disconnected fake implementation.

## Direct shadow architecture

The final active direct-shadow system replaces the current scene-bounds-fitted conventional directional shadow map with UE-style virtualized shadow representations.

### Virtual address space

Implement:

- 128 x 128 physical shadow pages
- level-0 128 x 128 page table
- 16384 x 16384 nominal virtual maximum resolution
- mip hierarchy
- virtual-page identifiers
- physical-page metadata
- physical-page allocation/free lists
- page requested-this-frame state
- page age
- cached/static/dynamic flags
- dirty/invalidation flags
- page-pool pressure tracking

The default physical pool follows the inspected UE reference value of approximately 2048 pages, with project-local tuning permitted only through CrapGame-owned policy constants/configuration.

### Receiver-driven allocation

Visible receivers mark virtual pages based on screen demand instead of rendering the entire light volume.

Implement:

- receiver masks
- receiver page marking
- mip selection from receiver footprint
- page-table fallback
- coarse-page marking
- local-light page requests
- directional clipmap page requests
- invalid-page fallback behavior
- physical-page allocation under pressure

Receiver mask granularity follows the inspected 8 x 8 reference behavior.

### Directional clipmaps

Directional lights use view-centered clipmaps rather than one world-bounds orthographic map.

Implement:

- first/last clipmap levels matching inspected UE defaults as starting references
- first/last coarse levels
- view-centered clipmap origin
- snapped clipmap origin for cache stability
- coverage doubling per level
- projection/viewport-based level selection
- clipmap Z range handling
- per-level page tables
- cache reuse as the camera moves
- localized invalidation

### Local lights

Point and spot lights receive virtualized page allocation rather than fixed monolithic maps where applicable.

The implementation preserves the light-specific projection model while sharing physical page management, cache metadata, receiver demand, invalidation, and filtering infrastructure.

### Page caching and invalidation

Implement:

- persistent physical-page reuse
- geometry-change invalidation
- material alpha/masked-state invalidation where relevant
- light transform/property invalidation
- page-age eviction
- static/dynamic page separation
- cache hit/miss statistics
- partial invalidation rather than global clear
- page reuse across camera movement
- pressure-aware eviction

### Distant/coarse representation

Implement coarse/far coverage equivalent to the documented Unreal behavior, including prefiltered/filterable distant representation where it contributes to stable distant shadows.

## SMRT soft-shadow projection

Shadow softness comes from finite emitter size and blocker geometry, not fixed PCF blur.

Implement:

- directional-light finite angular source extent
- local-light source radius/extent
- multi-ray shadow visibility
- multiple shadow-map samples along each ray
- blocker-depth interpretation
- penumbra growth from blocker/receiver separation
- contact-hardening behavior
- directional and local parameter sets
- adaptive ray counts
- early-out for fully lit/fully shadowed regions
- screen-space smart receiver bias
- normal receiver bias
- slope handling
- extrapolation behind blockers
- texel dithering/jitter
- temporal sample rotation
- bounded source-angle/ray-length behavior

The inspected UE defaults such as approximately seven rays and eight samples per ray are starting behavioral references, not hardcoded immutable requirements. Adaptive work reduction is part of the algorithm.

Fixed PCF remains only as a fallback/debug comparison path and is not the final normal shadow solution.

## Direct material lighting

The direct-light shader evolves to UE-like energy-consistent physically based shading.

Implement:

- Lambert-compatible diffuse base
- GGX/Trowbridge-Reitz normal distribution
- Smith joint visibility
- Schlick Fresnel
- metallic workflow
- explicit specular/IOR handling
- roughness handling
- anisotropic GGX
- tangent basis handling
- clear-coat layering
- clear-coat refraction/transmittance behavior where represented by material data
- sheen/cloth behavior where represented
- dual-specular/subsurface-related lobes where represented
- finite-source specular broadening
- rect-light LTC approximation
- energy preservation
- microfacet multiple-scattering energy compensation
- transmission/refraction paths
- translucent direct lighting

The renderer must avoid additive lobe combinations that create or lose energy compared with the intended layer model.

## Lumen Scene and Surface Cache

### Mesh Cards

Implement UE-style card representation using CrapGame-owned data structures.

Include:

- card generation/orientation
- per-mesh card sets
- card bounds
- card visibility
- card lookup
- card resolution levels
- low-resolution diffuse representation
- high-resolution specular representation
- card merge behavior where beneficial
- minimum-card-size policy
- card culling
- dirty-card tracking

### Surface Cache pages

Implement:

- virtual/paged Surface Cache storage
- material capture
- albedo
- normal
- emissive
- depth
- roughness/material properties needed for hit shading
- direct lighting
- radiosity/indirect lighting
- final cached radiance
- high/low-resolution lookup
- page allocation
- page reuse
- dirty tracking
- update priority
- update budgets
- age and residency
- partial recapture after geometry/material changes

### Lumen Scene direct lighting

Direct lighting for off-screen cached surfaces is computed separately from visible-screen direct shadowing.

Surface Cache direct lighting uses the same light/material/shadow semantics but stores cached scene radiance for GI/reflection hits.

Visible direct shadow signal, Surface Cache shadow signal, GI occlusion, and short-range AO remain separate until composition.

## Tracing architecture

All secondary-lighting systems follow the cheap-first, compact-misses, expensive-fallback strategy.

### Screen/HZB tracing

Implement:

- hierarchical depth representation
- screen ray traversal
- configurable iteration limit
- relative depth thickness
- hit validation
- normal/depth compatibility
- history-depth compatibility where required
- distant screen traces where useful
- screen-hit SceneColor sampling for valid near-screen hits

### Miss compaction

Unresolved traces are compacted before software geometry fallback.

Implement:

- trace masks
- compacted ray records
- indirect dispatch where available through LWJGL 2.9.3/OpenGL
- coherent grouping
- work counters
- early abort when unresolved occupancy becomes too low to justify expensive continuation where equivalent

### Mesh SDF

Preserve and extend existing mesh-distance-field infrastructure to support:

- signed distance storage
- object transforms
- thin-surface handling policy
- sphere tracing
- mesh-specific fallback
- distance/bias robustness
- dirty rebuild/update behavior

### Global SDF clipmaps

Implement/extend:

- world-space clipmaps
- multiple coverage levels
- camera-centered updates
- local invalidation
- merge of mesh fields into global field
- coarse far tracing
- bounded per-frame updates

### Software BVH fallback

Use the existing GPU BVH/triangle infrastructure for unresolved traces requiring exact triangle intersection.

Implement:

- near-field fallback
- far-field fallback
- shadow fallback
- reflection fallback
- hit-lighting fallback
- translucent/refraction fallback
- material lookup at hit
- coherent grouping/compaction

## Radiance Cache

Implement the documented world-space radiance cache behavior.

Include:

- approximately four clipmaps as the default reference
- first clipmap world extent following the inspected reference scale
- geometric clipmap distribution
- grid resolution equivalent to the documented reference behavior
- directional probe radiance
- probe atlas allocation
- per-probe state
- probe age
- probe priority
- probe dirty state
- probe tracing budget
- reprojection
- validity tests
- stochastic interpolation
- sky visibility where required
- history rejection
- scrolling/reuse as camera moves
- radiance lookup for diffuse GI
- radiance lookup for rough reflections where enabled

The cache uses a bounded probe-update budget rather than retracing all probes every frame.

## Screen Probe Gather

Replace scalar RGB-per-probe behavior with directional probe representation.

Implement:

- uniform screen probe grid
- adaptive probes
- representative surface selection
- directional octahedral storage
- structured direction sequence
- approximately 8 x 8 directional tracing resolution as the reference starting point
- importance sampling
- screen traces first
- compacted fallback traces
- Surface Cache hit radiance
- Radiance Cache miss/far fallback
- directional integration to irradiance
- spatial reconstruction
- depth/normal/material-aware interpolation
- temporal reprojection
- history rejection
- temporal accumulation
- spatial filtering
- disocclusion handling
- sample rotation/jitter
- bounded adaptive work

The current hard-coded scalar energy multipliers are removed from the final path and replaced by physically meaningful integration/normalization.

## Radiosity and multiple-bounce GI

Implement cached radiosity feedback through the Surface Cache and probe systems.

Include:

- Surface Cache radiosity state
- bounded page updates
- irradiance/radiance feedback
- multiple-bounce propagation
- temporal accumulation
- dirty invalidation after lighting/material/geometry changes
- stable emissive contribution
- color bleed
- energy-bounded feedback

Radiosity remains a cached secondary representation rather than a full per-pixel multi-bounce path tracer.

## Short-range AO and bent normals

Implement the full-resolution high-frequency occlusion/detail pass used to restore contact information lost by spatially filtered GI.

Include:

- short screen traces first
- software BVH/SDF fallback where required
- bent-normal output
- AO output
- configurable ray count
- normal bias
- hair/transparent handling where represented by CrapGame geometry/materials
- temporal stability
- separate composition from direct shadows

## Reflections

### Trace generation

Implement:

- roughness-aware ray generation
- screen/HZB first
- valid SceneColor sampling at screen hits
- miss compaction
- software BVH/SDF fallback
- near field
- far field
- hit radiance from Surface Cache
- software hit lighting for invalid/missing cache hits
- roughness-based trace distance
- optional Radiance Cache fallback for rough reflections
- recursive reflection/refraction where supported by material configuration and bounded budgets

### Reconstruction and denoising

Implement:

- BRDF-aware spatial reconstruction
- neighboring-ray reconstruction
- temporal reprojection
- history validation
- neighborhood history clamp
- max accumulated-frame policy
- disocclusion rejection
- bilateral filtering
- depth weighting
- normal weighting
- roughness-aware filtering
- firefly/ray-intensity clamps
- tonemap-domain filtering where required

Mirror-like surfaces preserve detail while rough surfaces use lower-frequency reconstruction and shorter/fewer expensive traces.

## ReSTIR GI gather

Implement the documented ReSTIR gather path as a separate selectable/usable GI gathering strategy integrated with the same caches and tracing stack.

Include:

- downsampled reservoir grid
- candidate generation
- reservoir weighting
- temporal reservoir reuse
- temporal validation/retrace option
- spatial resampling
- multiple spatial passes
- neighbor normal-angle compatibility
- depth compatibility
- occlusion screen validation
- bounded kernel radius
- ray-intensity clamp
- upsampling
- spiral/jittered resolve equivalents
- temporal filter
- history rejection
- variance-aware bilateral filter
- short-range AO supplementation

Reservoir state remains renderer-internal and never enters ECS.

## Transparency, transmission, and refraction

Preserve the existing transparent pass architecture and extend it so advanced material behavior participates consistently in:

- direct lighting
- shadows/transmittance where supported
- reflection rays
- refraction rays
- recursive bounded refraction
- Surface Cache/hit-lighting rules where appropriate

Opaque fast paths remain separate so transparency does not impose its cost on ordinary geometry.

## Temporal architecture

Temporal reuse is part of every noisy/cached algorithm rather than a final generic blur.

Implement per-system history validation using the data each system needs:

- depth
- normal
- entity/material identity where appropriate
- world position
- motion vectors
- roughness
- cache generation
- lighting generation
- camera changes
- disocclusion

Histories include:

- TAA/presentation history
- GI history
- Screen Probe history
- Radiance Cache reprojection
- reflection history
- ReSTIR reservoir history
- Surface Cache age/state
- VSM page cache state

## Scheduling and bounded work

`Gpu::FrameWork` and `Gpu::LumenSchedule` remain the scheduling model and are expanded instead of replaced.

The final renderer must avoid unconditional full-scene work.

Implement scheduling for:

- VSM page marking/rendering
- shadow invalidation
- Surface Cache recapture
- Surface Cache lighting updates
- radiosity updates
- Radiance Cache probe updates
- Screen Probe trace work
- reflection fallback traces
- ReSTIR updates
- SDF/global-SDF updates
- transparent work

Use:

- revision tracking
- dirty ranges
- dirty tiles
- cache generations
- per-frame budgets
- compacted dispatches
- indirect dispatch where available
- early-outs
- adaptive sampling
- converged-frame reuse

The current performance characteristic is a hard constraint. Visual improvements must not regress into brute-force full-resolution path tracing.

## Debug and observability

Add renderer debug outputs for every major subsystem.

### VSM

- virtual page
- physical page
- page mip/clipmap level
- cached page
- dirty page
- receiver mask
- requested page
- page-pool occupancy
- eviction/overflow state

### SMRT

- rays per pixel
- samples per ray
- early-out state
- blocker depth/distance
- source direction sample
- penumbra estimate
- normal/slope bias
- screen smart-bias trace

### Lumen Scene

- Mesh Card ID
- card bounds/orientation
- Surface Cache page
- Surface Cache albedo
- Surface Cache normal
- Surface Cache emissive
- Surface Cache depth
- Surface Cache direct lighting
- Surface Cache radiosity
- Surface Cache final radiance
- page age/dirty/priority

### GI

- Radiance Cache clipmap/probe
- probe validity/age/priority
- Screen Probe placement
- adaptive Screen Probes
- directional octahedron
- screen-trace hits
- fallback-trace hits
- radiosity state
- short-range AO
- bent normal

### Reflections

- screen hit
- software fallback hit
- Surface Cache vs hit-lighting source
- roughness trace budget
- history weight
- disocclusion state

### Performance

- VSM pages requested/rendered/reused/evicted
- shadow cache hit ratio
- average SMRT rays/pixel
- average SMRT samples/ray
- screen-trace success percentage
- expensive fallback percentage
- Surface Cache texels/pages updated
- Radiance Cache probes updated
- Screen Probe rays
- history accept/reject counts
- ReSTIR reservoir statistics
- GPU time per subsystem

## RendererCheck validation

The existing RendererCheck structure is preserved and expanded.

Every new system receives isolated correctness tests and performance contracts.

Required additions include at least:

### VSM/SMRT

- contact-hard directional shadow
- distance-softened directional penumbra
- local-light soft shadow
- clipmap transition continuity
- camera movement cache reuse
- moving caster local invalidation
- moving light invalidation
- page-pool pressure
- thin occluder
- grazing receiver bias

### GI

- white-room bounce
- colored-wall bleed
- emissive bounce
- doorway transfer
- off-screen indirect contribution
- thin-wall leak
- moving emissive history rejection
- Surface Cache invalidation
- Radiance Cache scrolling/reprojection
- Screen Probe adaptive placement

### Reflections

- mirror
- roughness sweep
- off-screen reflection
- screen-to-software-fallback continuity
- far-field reflection
- disocclusion rejection
- firefly clamp
- refraction where enabled

### Materials

- dielectric roughness sweep
- metallic roughness sweep
- anisotropy rotation
- clear coat
- energy preservation
- finite-source highlight
- rect-light LTC
- transmission/refraction

### ReSTIR

- temporal reservoir reuse
- spatial reservoir reuse
- depth rejection
- normal rejection
- disocclusion
- moving-light response
- short-range-AO supplementation

### Performance

Each visual test that exercises an expensive subsystem receives counters/budgets for the corresponding work.

No stage is declared complete solely because an image looks plausible.

## Migration order

Implementation proceeds in dependency order and the old path remains usable only as long as required to validate replacement stages.

1. VSM data model and page pool.
2. Receiver marking and directional clipmaps.
3. VSM page rendering/caching/invalidation.
4. SMRT projection and adaptive sampling.
5. Local-light virtual shadows.
6. Direct BRDF energy/material parity.
7. Surface Cache/Mesh Card expansion.
8. Lumen Scene direct-light cache.
9. HZB/screen tracing and miss compaction.
10. Mesh/global SDF fallback refinement.
11. Software BVH near/far/hit-lighting fallback.
12. Radiance Cache parity.
13. Directional Screen Probe Gather.
14. Radiosity/multi-bounce parity.
15. Short-range AO/bent normals.
16. Reflection tracing parity.
17. Reflection reconstruction/temporal filtering.
18. ReSTIR gather.
19. Transparency/refraction integration.
20. Scheduler/budget/convergence refinement.
21. Full debug/performance instrumentation.
22. RendererCheck expansion and final parity tuning.
23. Remove obsolete active shadow/GI approximations once replacement tests cover them.

## Error handling

Each GPU subsystem follows current CrapGame patterns:

- `init`/`ensure` return `bool`
- optional `std::string *error`
- clear error on success
- release partial resources on initialization failure
- do not leave half-valid objects
- recoverable secondary-pass failure may skip that pass and report once
- core GBuffer/direct/composition initialization failure aborts renderer initialization cleanly
- shader compile/link errors expose complete diagnostics

Missing `lwcgl` functions are treated as capability failures, not silently ignored.

## Success criteria

The implementation is complete only when all of the following are true:

1. Every feature documented in `unreal_engine_repo_deep_analysis.txt` has an implemented exact or software-equivalent renderer path, or a documented platform-inapplicable primitive whose renderer role is still reproduced.
2. Normal direct shadows use VSM/SMRT behavior rather than the old fixed-PCF architecture.
3. Directional shadows retain sharp contacts and widen penumbrae with blocker/receiver separation.
4. Directional shadow detail no longer depends on fitting one map to total scene bounds.
5. VSM pages persist and invalidate locally.
6. Direct BRDF response is energy-consistent across supported material lobes.
7. Surface Cache stores complete hit-lighting information required by GI/reflections.
8. Screen Probes retain directional radiance until irradiance integration.
9. Radiance Cache, radiosity, and Screen Probe history converge without the old arbitrary energy-crushing factors.
10. Short-range AO is a separate high-frequency indirect-occlusion signal.
11. Reflections use screen-first tracing, compacted fallback, cache/hit lighting, roughness-aware budgets, and temporal/spatial reconstruction.
12. ReSTIR temporal/spatial reservoir reuse is implemented and validated.
13. Hardware-RT-only Unreal roles have software BVH/SDF equivalents without introducing non-LWJGL-2.9.3 APIs.
14. `Renderer::Rendering` remains the minimal external interface.
15. ECS remains renderer-resource-free.
16. Source style remains consistent with current CrapGame code.
17. `lwcgl` contains no invented renderer-specific API.
18. Every added `lwcgl` binding is verified against actual LWJGL 2.9.3 capability.
19. RendererCheck validates isolated correctness, fallback behavior, temporal behavior, and performance budgets.
20. Performance remains bounded by sparse allocation, caches, budgets, compaction, adaptive sampling, and convergence reuse rather than brute-force work.

## Non-goals

- Do not copy Unreal source code verbatim.
- Do not introduce Unreal's object framework, RHI, RDG, Nanite implementation, editor systems, or engine-wide abstractions.
- Do not redesign CrapGame into Unreal Engine.
- Do not replace the CrapGame ECS.
- Do not introduce a second public renderer frontend.
- Do not create APIs in `lwcgl` that never existed in LWJGL 2.9.3.
- Do not solve visual artifacts by hiding them with uncontrolled blur or arbitrary brightness multipliers.
