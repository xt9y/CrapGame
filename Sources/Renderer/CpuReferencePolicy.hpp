#ifndef CRAPGAME_RENDERER_CPU_REFERENCE_POLICY_HPP
#define CRAPGAME_RENDERER_CPU_REFERENCE_POLICY_HPP

namespace Renderer
{

enum class CpuReferenceMode
{
    Final,
    Albedo,
    Normal,
    Depth,
    Material,
    Direct,
    Shadow,
    Motion,
    MeshSdf,
    GlobalSdf,
    Trace,
    SurfaceCache,
    SurfaceLighting,
    RadianceCache,
    ScreenProbes,
    Indirect,
    Ao,
    Reflection,
};

struct CpuReferencePassPlan
{
    bool geometry = true;
    bool motion = false;
    bool tracer = false;
    bool cards = false;
    bool shadows = false;
    bool surface_cache = false;
    bool scene_lighting = false;
    bool radiosity = false;
    bool radiance_cache = false;
    bool direct = false;
    bool screen_probes = false;
    bool gi_taa = false;
    bool reflections = false;
    bool final_taa = false;
};

/* The visual reference renderer keeps one deterministic output mode per test,
 * but it does not need to execute passes whose results that mode never reads. */
inline CpuReferencePassPlan cpuReferencePlan (CpuReferenceMode mode)
{
    CpuReferencePassPlan plan;

    switch (mode)
    {
        case CpuReferenceMode::Albedo:
        case CpuReferenceMode::Normal:
        case CpuReferenceMode::Depth:
        case CpuReferenceMode::Material:
            return plan;

        case CpuReferenceMode::Direct:
            plan.shadows = true;
            plan.direct = true;
            return plan;

        case CpuReferenceMode::Shadow:
            plan.shadows = true;
            return plan;

        case CpuReferenceMode::Motion:
            plan.motion = true;
            return plan;

        case CpuReferenceMode::MeshSdf:
        case CpuReferenceMode::GlobalSdf:
        case CpuReferenceMode::Trace:
        case CpuReferenceMode::Ao:
            plan.tracer = true;
            return plan;

        case CpuReferenceMode::SurfaceCache:
            plan.cards = true;
            plan.surface_cache = true;
            return plan;

        case CpuReferenceMode::SurfaceLighting:
            plan.cards = true;
            plan.shadows = true;
            plan.surface_cache = true;
            plan.scene_lighting = true;
            return plan;

        case CpuReferenceMode::RadianceCache:
            plan.tracer = true;
            plan.cards = true;
            plan.shadows = true;
            plan.surface_cache = true;
            plan.scene_lighting = true;
            plan.radiosity = true;
            plan.radiance_cache = true;
            return plan;

        case CpuReferenceMode::ScreenProbes:
            plan.tracer = true;
            plan.cards = true;
            plan.shadows = true;
            plan.surface_cache = true;
            plan.scene_lighting = true;
            plan.radiosity = true;
            plan.radiance_cache = true;
            plan.screen_probes = true;
            return plan;

        case CpuReferenceMode::Indirect:
            plan.motion = true;
            plan.tracer = true;
            plan.cards = true;
            plan.shadows = true;
            plan.surface_cache = true;
            plan.scene_lighting = true;
            plan.radiosity = true;
            plan.radiance_cache = true;
            plan.screen_probes = true;
            plan.gi_taa = true;
            return plan;

        case CpuReferenceMode::Reflection:
            plan.motion = true;
            plan.tracer = true;
            plan.cards = true;
            plan.shadows = true;
            plan.surface_cache = true;
            plan.scene_lighting = true;
            plan.radiosity = true;
            plan.radiance_cache = true;
            plan.reflections = true;
            return plan;

        case CpuReferenceMode::Final:
            plan.motion = true;
            plan.tracer = true;
            plan.cards = true;
            plan.shadows = true;
            plan.surface_cache = true;
            plan.scene_lighting = true;
            plan.radiosity = true;
            plan.radiance_cache = true;
            plan.direct = true;
            plan.screen_probes = true;
            plan.gi_taa = true;
            plan.reflections = true;
            plan.final_taa = true;
            return plan;
    }

    return plan;
}

inline bool cpuGeometryNeedsRefresh (
            bool valid,
            bool geometry_changed,
            bool material_changed,
            bool camera_changed
    )
{
    return !valid
        || geometry_changed
        || material_changed
        || camera_changed;
}

inline bool cpuDirectNeedsRefresh (
            bool valid,
            bool geometry_dirty,
            bool lighting_changed
    )
{
    return !valid
        || geometry_dirty
        || lighting_changed;
}

inline bool cpuWindowPresentationRequired (bool capture_run)
{
    return !capture_run;
}

struct RendererRuntimePlan
{
    bool display_required = true;
    bool input_required = true;
    bool window_updates_required = true;
    bool fixed_reference_size = false;
};

inline RendererRuntimePlan rendererRuntimePlan (
            bool renderercheck_mode,
            bool performance_mode
    )
{
    const bool headless_visual = renderercheck_mode && !performance_mode;

    RendererRuntimePlan plan;
    plan.display_required = !headless_visual;
    plan.input_required = !headless_visual;
    plan.window_updates_required = !headless_visual;
    plan.fixed_reference_size = headless_visual;
    return plan;
}

} // namespace Renderer

#endif
