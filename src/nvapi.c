#include "nvapi_internal.h"

static NvU32 compute_gpu_flags(void) {
    return NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_CAPABLE |
           NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_ENABLE |
           NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_RECOMMENDED |
           NV_COMPUTE_GPU_TOPOLOGY_CUDA_AVAILABLE |
           NV_COMPUTE_GPU_TOPOLOGY_CUDA_CAPABLE |
           NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_AVAILABLE;
}

static NvAPI_Status NvAPI_Initialize(void) {
    acuda_nvapi_ensure_discovered();
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_Unload(void) { return NVAPI_OK; }

static NvAPI_Status NvAPI_GetInterfaceVersionString(NvAPI_ShortString szDesc) {
    if (!szDesc) return NVAPI_INVALID_ARGUMENT;
    acuda_nvapi_copy_short_string(szDesc, "NVidia Complete Version 1.10");
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetErrorMessage(NvAPI_Status nr, NvAPI_ShortString szDesc) {
    if (!szDesc) return NVAPI_INVALID_ARGUMENT;
    snprintf(szDesc, 64, "NVAPI error %d", nr);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetDisplayDriverVersion(NvDisplayHandle display,
                                                  NV_DISPLAY_DRIVER_VERSION *version) {
    (void)display;
    if (!version) return NVAPI_INVALID_ARGUMENT;
    if (version->version != acuda_nvapi_make_version(sizeof(NV_DISPLAY_DRIVER_VERSION), 1))
        return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
    version->drvVersion = 99999;
    version->bldChangeListNum = 0;
    memset(version->szBuildBranchString, 0, sizeof(version->szBuildBranchString));
    acuda_nvapi_copy_short_string(version->szBuildBranchString, "r999_99");
    memset(version->szAdapterString, 0, sizeof(version->szAdapterString));
    acuda_nvapi_ensure_discovered();
    acuda_nvapi_copy_short_string(version->szAdapterString,
                                  g_gpu_count ? g_gpus[0].name : "ACuda Graphics Device");
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_CudaEnumComputeCapableGpus_v1(NV_COMPUTE_GPU_TOPOLOGY_V1 *topo) {
    NvU32 i, n;
    acuda_nvapi_ensure_discovered();
    n = g_gpu_count > NVAPI_MAX_GPU_PER_TOPOLOGY ? NVAPI_MAX_GPU_PER_TOPOLOGY : g_gpu_count;
    topo->gpuCount = n;
    for (i = 0; i < n; i++) {
        topo->computeGpus[i].hPhysicalGpu = acuda_nvapi_gpu_handle(&g_gpus[i]);
        topo->computeGpus[i].flags = compute_gpu_flags();
        acuda_nvapi_log_msg("topo v1 gpu[%u] handle=%p flags=0x%x", i,
                            (void *)topo->computeGpus[i].hPhysicalGpu,
                            topo->computeGpus[i].flags);
    }
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_CudaEnumComputeCapableGpus_v2(NV_COMPUTE_GPU_TOPOLOGY_V2 *topo) {
    NvU32 i;
    acuda_nvapi_ensure_discovered();
    for (i = 0; i < g_gpu_count; i++) {
        g_compute_gpus[i].hPhysicalGpu = acuda_nvapi_gpu_handle(&g_gpus[i]);
        g_compute_gpus[i].flags = compute_gpu_flags();
        acuda_nvapi_log_msg("topo v2 gpu[%u] handle=%p flags=0x%x", i,
                            (void *)g_compute_gpus[i].hPhysicalGpu,
                            g_compute_gpus[i].flags);
    }
    topo->gpuCount = g_gpu_count;
    topo->computeGpus = g_compute_gpus;
    acuda_nvapi_log_msg("topo v2 count=%u", topo->gpuCount);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_CudaEnumComputeCapableGpus(NvU32 *version_ptr) {
    acuda_nvapi_log_msg("call 5786cc6e ptr=%p version=0x%x", (void *)version_ptr,
                        version_ptr ? *version_ptr : 0);
    LOG_CALLER("5786cc6e");
    if (!version_ptr) return NVAPI_INVALID_ARGUMENT;
    if (*version_ptr == acuda_nvapi_make_version(sizeof(NV_COMPUTE_GPU_TOPOLOGY_V1), 1))
        return NvAPI_GPU_CudaEnumComputeCapableGpus_v1((NV_COMPUTE_GPU_TOPOLOGY_V1 *)version_ptr);
    if (*version_ptr == acuda_nvapi_make_version(sizeof(NV_COMPUTE_GPU_TOPOLOGY_V2), 2))
        return NvAPI_GPU_CudaEnumComputeCapableGpus_v2((NV_COMPUTE_GPU_TOPOLOGY_V2 *)version_ptr);
    return NVAPI_INCOMPATIBLE_STRUCT_VERSION;
}

static NvAPI_Status NvAPI_GPU_GetComputeCapableGpus(NV_COMPUTE_GPU_TOPOLOGY_V2 *topo,
                                                    NvU32 u1, NvU32 u2, NvU32 u3) {
    (void)u1; (void)u2; (void)u3;
    acuda_nvapi_log_msg("call 36e39e6b topo=%p", (void *)topo);
    LOG_CALLER("36e39e6b");
    if (!topo) return NVAPI_INVALID_ARGUMENT;
    if (topo->version == acuda_nvapi_make_version(sizeof(NV_COMPUTE_GPU_TOPOLOGY_V1), 1))
        return NvAPI_GPU_CudaEnumComputeCapableGpus_v1((NV_COMPUTE_GPU_TOPOLOGY_V1 *)topo);
    return NvAPI_GPU_CudaEnumComputeCapableGpus_v2(topo);
}

static NvAPI_Status NvAPI_GetPhysicalGPUFromGPUID(NvU32 gpu_id, NvPhysicalGpuHandle *physical_gpu) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    acuda_nvapi_log_msg("call 5380ad1a id=%u", gpu_id);
    LOG_CALLER("5380ad1a");
    if (!physical_gpu) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_id(gpu_id);
    if (!g) return NVAPI_INVALID_ARGUMENT;
    *physical_gpu = acuda_nvapi_gpu_handle(g);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetGPUIDfromPhysicalGPU(NvPhysicalGpuHandle physical_gpu, NvU32 *gpu_id) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    acuda_nvapi_log_msg("call 6533ea3e handle=%p", physical_gpu);
    LOG_CALLER("6533ea3e");
    if (!gpu_id) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!g) return NVAPI_INVALID_ARGUMENT;
    *gpu_id = g->gpu_id;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_EnumPhysicalGPUs(NvPhysicalGpuHandle *handles, NvU32 *count) {
    NvU32 i, n;
    acuda_nvapi_ensure_discovered();
    if (!handles || !count) return NVAPI_INVALID_ARGUMENT;
    n = g_gpu_count > NVAPI_MAX_PHYSICAL_GPUS ? NVAPI_MAX_PHYSICAL_GPUS : g_gpu_count;
    for (i = 0; i < n; i++)
        handles[i] = acuda_nvapi_gpu_handle(&g_gpus[i]);
    *count = n;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_EnumLogicalGPUs(NvLogicalGpuHandle *handles, NvU32 *count) {
    NvU32 i, n;
    acuda_nvapi_ensure_discovered();
    if (!handles || !count) return NVAPI_INVALID_ARGUMENT;
    n = g_gpu_count > NVAPI_MAX_LOGICAL_GPUS ? NVAPI_MAX_LOGICAL_GPUS : g_gpu_count;
    for (i = 0; i < n; i++)
        handles[i] = (NvLogicalGpuHandle)(size_t)g_gpus[i].gpu_id;
    *count = n;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_EnumTCCPhysicalGPUs(NvPhysicalGpuHandle *handles, NvU32 *count) {
    (void)handles;
    if (!count) return NVAPI_INVALID_ARGUMENT;
    *count = 0;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetPhysicalGPUsFromLogicalGPU(NvLogicalGpuHandle logical_gpu,
                                                        NvPhysicalGpuHandle *handles,
                                                        NvU32 *count) {
    NvU32 id = (NvU32)(size_t)logical_gpu;
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    if (!handles || !count) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_id(id);
    if (!g && g_gpu_count) g = &g_gpus[0];
    if (!g) return NVAPI_INVALID_ARGUMENT;
    handles[0] = acuda_nvapi_gpu_handle(g);
    *count = 1;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetLogicalGPUFromPhysicalGPU(NvPhysicalGpuHandle physical_gpu,
                                                       NvLogicalGpuHandle *logical_gpu) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    if (!logical_gpu) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!g) return NVAPI_INVALID_ARGUMENT;
    *logical_gpu = (NvLogicalGpuHandle)(size_t)g->gpu_id;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_EnumNvidiaDisplayHandle(NvU32 index, NvDisplayHandle *display) {
    if (!display) return NVAPI_INVALID_ARGUMENT;
    if (index != 0) return NVAPI_END_ENUMERATION;
    *display = FAKE_DISPLAY_HANDLE;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetPhysicalGPUsFromDisplay(NvDisplayHandle display,
                                                     NvPhysicalGpuHandle *handles,
                                                     NvU32 *count) {
    (void)display;
    acuda_nvapi_ensure_discovered();
    if (!handles || !count) return NVAPI_INVALID_ARGUMENT;
    if (g_gpu_count == 0) return NVAPI_NVIDIA_DEVICE_NOT_FOUND;
    handles[0] = acuda_nvapi_gpu_handle(&g_gpus[0]);
    *count = 1;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetAssociatedNvidiaDisplayHandle(const char *szDisplayName,
                                                           NvDisplayHandle *pNvDispHandle) {
    (void)szDisplayName;
    if (!pNvDispHandle) return NVAPI_INVALID_ARGUMENT;
    *pNvDispHandle = FAKE_DISPLAY_HANDLE;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetAssociatedNvidiaDisplayName(NvDisplayHandle NvDispHandle,
                                                         NvAPI_ShortString szDisplayName) {
    (void)NvDispHandle;
    if (!szDisplayName) return NVAPI_INVALID_ARGUMENT;
    acuda_nvapi_copy_short_string(szDisplayName, "\\\\.\\DISPLAY1");
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_GetConnectedOutputs(NvPhysicalGpuHandle physical_gpu, NvU32 *outputs) {
    acuda_nvapi_ensure_discovered();
    if (!outputs || !acuda_nvapi_find_gpu_by_handle(physical_gpu)) return NVAPI_INVALID_ARGUMENT;
    /* Optimus laptops drive the panel from the iGPU (Disp.A Off on the
     * NVIDIA GPU).  PhysX 2.8.x rejects a PhysX-capable GPU whose
     * connected/active output mask is empty (see the display-selection
     * path in PhysXCore), so report one logical output: the CUDA/PPU
     * device is still usable even when it owns no physical connector. */
    *outputs = 1;
    acuda_nvapi_log_msg("outputs handle=%p mask=0x%x", physical_gpu, *outputs);
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetConnectedOutputsWithLidState(NvPhysicalGpuHandle p, NvU32 *o) {
    return NvAPI_GPU_GetConnectedOutputs(p, o);
}
static NvAPI_Status NvAPI_GPU_GetActiveOutputs(NvPhysicalGpuHandle p, NvU32 *o) {
    return NvAPI_GPU_GetConnectedOutputs(p, o);
}
static NvAPI_Status NvAPI_GPU_GetAllOutputs(NvPhysicalGpuHandle p, NvU32 *o) {
    return NvAPI_GPU_GetConnectedOutputs(p, o);
}

static NvAPI_Status NvAPI_GPU_GetFullName(NvPhysicalGpuHandle physical_gpu, char *name) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    if (!name) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!g) return NVAPI_INVALID_ARGUMENT;
    acuda_nvapi_copy_short_string(name, g->name);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_GetPCIIdentifiers(NvPhysicalGpuHandle physical_gpu,
                                                NvU32 *device_id, NvU32 *subsystem_id,
                                                NvU32 *revision_id, NvU32 *ext_device_id) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!g || !device_id || !subsystem_id || !revision_id || !ext_device_id)
        return NVAPI_INVALID_ARGUMENT;
    *device_id = ((g->device_id & 0xFFFFu) << 16) | (g->vendor_id & 0xFFFFu);
    *subsystem_id = g->subsystem_id;
    *revision_id = g->revision_id;
    *ext_device_id = g->device_id;
    acuda_nvapi_log_msg("pci ids handle=%p dev=0x%x sub=0x%x rev=0x%x ext=0x%x",
                        physical_gpu, *device_id, *subsystem_id,
                        *revision_id, *ext_device_id);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_GetGPUType(NvPhysicalGpuHandle physical_gpu, NvU32 *gpu_type) {
    acuda_nvapi_ensure_discovered();
    if (!gpu_type || !acuda_nvapi_find_gpu_by_handle(physical_gpu)) return NVAPI_INVALID_ARGUMENT;
    *gpu_type = 2;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetBusType(NvPhysicalGpuHandle physical_gpu, NvU32 *bus_type) {
    acuda_nvapi_ensure_discovered();
    if (!bus_type || !acuda_nvapi_find_gpu_by_handle(physical_gpu)) return NVAPI_INVALID_ARGUMENT;
    *bus_type = 3;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetBusId(NvPhysicalGpuHandle physical_gpu, NvU32 *bus_id) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!bus_id || !g) return NVAPI_INVALID_ARGUMENT;
    *bus_id = g->pci_bus;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetBusSlotId(NvPhysicalGpuHandle physical_gpu, NvU32 *slot_id) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!slot_id || !g) return NVAPI_INVALID_ARGUMENT;
    *slot_id = g->pci_device;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetPhysicalFrameBufferSize(NvPhysicalGpuHandle physical_gpu, NvU32 *size_kb) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!size_kb || !g) return NVAPI_INVALID_ARGUMENT;
    *size_kb = acuda_nvapi_env_u32("ACUDA_NVAPI_FB_SIZE_KB", g->memory_kb ? g->memory_kb : 8388608u);
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetVirtualFrameBufferSize(NvPhysicalGpuHandle physical_gpu, NvU32 *size_kb) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!size_kb || !g) return NVAPI_INVALID_ARGUMENT;
    *size_kb = acuda_nvapi_env_u32("ACUDA_NVAPI_VFB_SIZE_KB", g->memory_kb ? g->memory_kb : 8388608u);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_SYS_GetDriverAndBranchVersion(NvU32 *driver_version,
                                                        NvAPI_ShortString *build_branch_string) {
    if (!driver_version || !build_branch_string) return NVAPI_INVALID_ARGUMENT;
    *driver_version = 99999;
    acuda_nvapi_copy_short_string(*build_branch_string, "r999_99");
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_DISP_GetDisplayIdByDisplayName(const char *displayName, NvU32 *displayId) {
    (void)displayName;
    if (!displayId) return NVAPI_INVALID_ARGUMENT;
    *displayId = 0x80000000u;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_DISP_GetGDIPrimaryDisplayId(NvU32 *displayId) {
    if (!displayId) return NVAPI_INVALID_ARGUMENT;
    *displayId = 0x80000000u;
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_Stub_Success(void) { return NVAPI_OK; }

ACUDA_NVAPI_EXPORT void *ACUDA_NVAPI_CALL nvapi_QueryInterface(NvU32 interface_id) {
    void *result = NULL;
    switch (interface_id) {
        case 0x0150e828u: result = (void *)NvAPI_Initialize; break;
        case 0xd22bdd7eu: result = (void *)NvAPI_Unload; break;
        case 0x01053fa5u: result = (void *)NvAPI_GetInterfaceVersionString; break;
        case 0x6c2d048cu: result = (void *)NvAPI_GetErrorMessage; break;
        case 0xf951a4d1u: result = (void *)NvAPI_GetDisplayDriverVersion; break;
        case 0x2926aaadu: result = (void *)NvAPI_SYS_GetDriverAndBranchVersion; break;
        case 0x5380ad1au: result = (void *)NvAPI_GetPhysicalGPUFromGPUID; break;
        case 0x6533ea3eu: result = (void *)NvAPI_GetGPUIDfromPhysicalGPU; break;
        case 0x5786cc6eu: result = (void *)NvAPI_GPU_CudaEnumComputeCapableGpus; break;
        case 0x36e39e6bu: result = (void *)NvAPI_GPU_GetComputeCapableGpus; break;
        case 0xe5ac921fu: result = (void *)NvAPI_EnumPhysicalGPUs; break;
        case 0x48b3ea59u: result = (void *)NvAPI_EnumLogicalGPUs; break;
        case 0x1730bfc9u: result = (void *)NvAPI_GPU_GetConnectedOutputs; break;
        case 0xcf8caf39u: result = (void *)NvAPI_GPU_GetConnectedOutputsWithLidState; break;
        case 0xe3e89b6fu: result = (void *)NvAPI_GPU_GetActiveOutputs; break;
        case 0x7d554f8eu: result = (void *)NvAPI_GPU_GetAllOutputs; break;
        case 0xaea3fa32u: result = (void *)NvAPI_GetPhysicalGPUsFromLogicalGPU; break;
        case 0xadd604d1u: result = (void *)NvAPI_GetLogicalGPUFromPhysicalGPU; break;
        case 0xd9930b07u: result = (void *)NvAPI_EnumTCCPhysicalGPUs; break;
        case 0x34ef9506u: result = (void *)NvAPI_GetPhysicalGPUsFromDisplay; break;
        case 0x35c29134u: result = (void *)NvAPI_GetAssociatedNvidiaDisplayHandle; break;
        case 0x22a78b05u: result = (void *)NvAPI_GetAssociatedNvidiaDisplayName; break;
        case 0xceee8e9fu: result = (void *)NvAPI_GPU_GetFullName; break;
        case 0x2ddfb66eu: result = (void *)NvAPI_GPU_GetPCIIdentifiers; break;
        case 0xc33baeb1u: result = (void *)NvAPI_GPU_GetGPUType; break;
        case 0x1bb18724u: result = (void *)NvAPI_GPU_GetBusType; break;
        case 0x1be0b8e5u: result = (void *)NvAPI_GPU_GetBusId; break;
        case 0x2a0a350fu: result = (void *)NvAPI_GPU_GetBusSlotId; break;
        case 0x46fbeb03u: result = (void *)NvAPI_GPU_GetPhysicalFrameBufferSize; break;
        case 0x5a04b644u: result = (void *)NvAPI_GPU_GetVirtualFrameBufferSize; break;
        case 0x9abdd40du: result = (void *)NvAPI_EnumNvidiaDisplayHandle; break;
        case 0x1e9d8a31u: result = (void *)NvAPI_DISP_GetDisplayIdByDisplayName; break;
        case 0x1e9d8a32u: result = (void *)NvAPI_DISP_GetGDIPrimaryDisplayId; break;
        case 0x33c7358cu: result = (void *)NvAPI_Stub_Success; break;
        default: result = (void *)0; break;
    }
    acuda_nvapi_log_msg("id=0x%08x ret=%p", interface_id, result);
    if (!result) LOG_CALLER("query-null");
    return result;
}

/* Test/helper exports */
ACUDA_NVAPI_EXPORT NvU32 ACUDA_NVAPI_CALL acuda_nvapi_get_gpu_count(void) {
    acuda_nvapi_ensure_discovered();
    return g_gpu_count;
}
ACUDA_NVAPI_EXPORT int ACUDA_NVAPI_CALL acuda_nvapi_get_gpu_info(
    NvU32 index, NvU32 *gpu_id, NvU32 *ppu_key, NvU32 *domain, NvU32 *bus,
    NvU32 *device, NvU32 *function, NvU32 *memory_kb, char *name, NvU32 name_size) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    if (index >= g_gpu_count) return 0;
    g = &g_gpus[index];
    if (gpu_id) *gpu_id = g->gpu_id;
    if (ppu_key) *ppu_key = g->ppu_key;
    if (domain) *domain = g->pci_domain;
    if (bus) *bus = g->pci_bus;
    if (device) *device = g->pci_device;
    if (function) *function = g->pci_function;
    if (memory_kb) *memory_kb = g->memory_kb;
    if (name && name_size) {
        strncpy(name, g->name, name_size - 1);
        name[name_size - 1] = 0;
    }
    return 1;
}
ACUDA_NVAPI_EXPORT void ACUDA_NVAPI_CALL acuda_nvapi_reset(void) {
    acuda_nvapi_set_discovered(0);
    acuda_nvapi_free_gpus();
}
