#include "nvapi_internal.h"

static NvU32 compute_gpu_flags(void) {
    /* Match real NVAPI flags observed on a GeForce GPU (0xb). */
    return NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_CAPABLE |
           NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_ENABLE |
           NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_RECOMMENDED;
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
    NvU32 i, n;
    NV_COMPUTE_GPU *out;
    acuda_nvapi_ensure_discovered();
    out = topo->computeGpus ? topo->computeGpus : g_compute_gpus;
    n = g_gpu_count;
    topo->gpuCount = n;
    for (i = 0; i < n; i++) {
        out[i].hPhysicalGpu = acuda_nvapi_gpu_handle(&g_gpus[i]);
        out[i].flags = compute_gpu_flags();
        acuda_nvapi_log_msg("topo v2 gpu[%u] handle=%p flags=0x%x", i,
                            (void *)out[i].hPhysicalGpu, out[i].flags);
    }
    if (!topo->computeGpus)
        topo->computeGpus = g_compute_gpus;
    acuda_nvapi_log_msg("topo v2 count=%u", topo->gpuCount);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_CudaEnumComputeCapableGpus(NV_COMPUTE_GPU_TOPOLOGY *pComputeTopo) {
    acuda_nvapi_log_msg("call 5786cc6e ptr=%p version=0x%x", (void *)pComputeTopo,
                        pComputeTopo ? pComputeTopo->version : 0);
    LOG_CALLER("5786cc6e");
    if (!pComputeTopo) return NVAPI_INVALID_ARGUMENT;
    if (pComputeTopo->version == NV_COMPUTE_GPU_TOPOLOGY_VER1)
        return NvAPI_GPU_CudaEnumComputeCapableGpus_v1((NV_COMPUTE_GPU_TOPOLOGY_V1 *)pComputeTopo);
    if (pComputeTopo->version == NV_COMPUTE_GPU_TOPOLOGY_VER)
        return NvAPI_GPU_CudaEnumComputeCapableGpus_v2((NV_COMPUTE_GPU_TOPOLOGY_V2 *)pComputeTopo);
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
    if (!g && g_gpu_count == 1)
        g = &g_gpus[0]; /* tolerate OpenCL/driver-internal GPU ID schemes */
    if (!g) { acuda_nvapi_log_msg("call 5380ad1a -> not found"); return NVAPI_INVALID_ARGUMENT; }
    *physical_gpu = acuda_nvapi_gpu_handle(g);
    acuda_nvapi_log_msg("call 5380ad1a -> handle=%p", (void *)*physical_gpu);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetGPUIDfromPhysicalGPU(NvPhysicalGpuHandle physical_gpu, NvU32 *gpu_id) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    acuda_nvapi_log_msg("call 6533ea3e handle=%p", physical_gpu);
    LOG_CALLER("6533ea3e");
    if (!gpu_id) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!g) { acuda_nvapi_log_msg("call 6533ea3e -> not found"); return NVAPI_INVALID_ARGUMENT; }
    *gpu_id = g->gpu_id;
    acuda_nvapi_log_msg("call 6533ea3e -> id=%u", *gpu_id);
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
    acuda_nvapi_log_msg("EnumPhysicalGPUs count=%u h0=%p", n, n ? (void *)handles[0] : NULL);
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
    acuda_nvapi_log_msg("EnumLogicalGPUs count=%u h0=%p", n, n ? (void *)handles[0] : NULL);
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
    acuda_nvapi_log_msg("GetPhysicalGPUsFromLogicalGPU lg=%p -> count=%u h0=%p", logical_gpu, *count, (void *)handles[0]);
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
    acuda_nvapi_log_msg("GetLogicalGPUFromPhysicalGPU pg=%p -> lg=%p", physical_gpu, *logical_gpu);
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
    acuda_nvapi_log_msg("GetPhysicalGPUsFromDisplay count=%u h0=%p", *count, (void *)handles[0]);
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

static NvAPI_Status NvAPI_GetPhysicalGPUFromUnAttachedDisplay(NvUnAttachedDisplayHandle hNvUnAttachedDisp,
                                                              NvPhysicalGpuHandle *pPhysicalGpu) {
    (void)hNvUnAttachedDisp;
    acuda_nvapi_ensure_discovered();
    acuda_nvapi_log_msg("call 5018ed61 unattached=%p", (void *)hNvUnAttachedDisp);
    LOG_CALLER("5018ed61");
    if (!pPhysicalGpu) return NVAPI_INVALID_ARGUMENT;
    if (g_gpu_count == 0) return NVAPI_NVIDIA_DEVICE_NOT_FOUND;
    *pPhysicalGpu = acuda_nvapi_gpu_handle(&g_gpus[0]);
    acuda_nvapi_log_msg("call 5018ed61 -> handle=%p", (void *)*pPhysicalGpu);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_EnumNvidiaUnAttachedDisplayHandle(NvU32 thisEnum, NvUnAttachedDisplayHandle *pNvUnAttachedDispHandle) {
    acuda_nvapi_log_msg("call 20de9260 idx=%u", thisEnum);
    LOG_CALLER("20de9260");
    if (!pNvUnAttachedDispHandle) return NVAPI_INVALID_ARGUMENT;
    if (thisEnum != 0 || g_gpu_count == 0) return NVAPI_END_ENUMERATION;
    acuda_nvapi_ensure_discovered();
    *pNvUnAttachedDispHandle = (NvUnAttachedDisplayHandle)(uintptr_t)(0xA0000001u + thisEnum);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GetUnAttachedAssociatedDisplayName(NvUnAttachedDisplayHandle hNvUnAttachedDisp, NvAPI_ShortString szDisplayName) {
    (void)hNvUnAttachedDisp;
    acuda_nvapi_log_msg("call 4888d790 unattached=%p", (void *)hNvUnAttachedDisp);
    LOG_CALLER("4888d790");
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

static NvAPI_Status NvAPI_GPU_GetGPUType(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_TYPE *pGpuType) {
    acuda_nvapi_ensure_discovered();
    if (!pGpuType || !acuda_nvapi_find_gpu_by_handle(hPhysicalGpu)) return NVAPI_INVALID_ARGUMENT;
    *pGpuType = NV_SYSTEM_TYPE_DGPU;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetBusType(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_BUS_TYPE *pBusType) {
    acuda_nvapi_ensure_discovered();
    if (!pBusType || !acuda_nvapi_find_gpu_by_handle(hPhysicalGpu)) return NVAPI_INVALID_ARGUMENT;
    *pBusType = NVAPI_GPU_BUS_TYPE_PCI_EXPRESS;
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
    /* Real-dump default (RTX 3070 Laptop): 8388096 KB */
    *size_kb = acuda_nvapi_env_u32("ACUDA_NVAPI_FB_SIZE_KB", 8388096u);
    acuda_nvapi_log_msg("GetPhysicalFrameBufferSize h=%p -> %u KB", physical_gpu, *size_kb);
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetVirtualFrameBufferSize(NvPhysicalGpuHandle physical_gpu, NvU32 *size_kb) {
    acuda_nvapi_gpu_t *g;
    acuda_nvapi_ensure_discovered();
    g = acuda_nvapi_find_gpu_by_handle(physical_gpu);
    if (!size_kb || !g) return NVAPI_INVALID_ARGUMENT;
    /* Real-dump default (RTX 3070 Laptop): 8388608 KB */
    *size_kb = acuda_nvapi_env_u32("ACUDA_NVAPI_VFB_SIZE_KB", 8388608u);
    acuda_nvapi_log_msg("GetVirtualFrameBufferSize h=%p -> %u KB", physical_gpu, *size_kb);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_SYS_GetDriverAndBranchVersion(NvU32 *pDriverVersion,
                                                        NvAPI_ShortString szBuildBranchString) {
    if (!pDriverVersion || !szBuildBranchString) return NVAPI_INVALID_ARGUMENT;
    *pDriverVersion = 99999;
    acuda_nvapi_copy_short_string(szBuildBranchString, "r999_99");
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

static NvAPI_Status NvAPI_GPU_GetVbiosVersionString(NvPhysicalGpuHandle h, NvAPI_ShortString s) {
    (void)h;
    if (!s) return NVAPI_INVALID_ARGUMENT;
    acuda_nvapi_copy_short_string(s, "90.00.00.00.00");
    return NVAPI_OK;
}

typedef struct {
    NvU32 version;
    NvU32 dedicatedVideoMemory;
    NvU32 availableDedicatedVideoMemory;
    NvU32 systemVideoMemory;
    NvU32 sharedSystemMemory;
    NvU32 curAvailableDedicatedVideoMemory;
} NV_DISPLAY_DRIVER_MEMORY_INFO_BASE;

/* NVAPI version dwords encode the caller's struct size in the low 16 bits
 * (acuda_nvapi_make_version(size, ver) = size | ver << 16).  Clients pass
 * noticeably smaller structs than the newest layout (PhysX 2.8 passes
 * NV_GPU_THERMAL_SETTINGS_V1 = 0x44 bytes), so every write into a caller
 * buffer must be clamped to the declared size instead of assuming the full
 * current struct: overrunning it corrupts whatever follows the struct in
 * the caller's frame (stack cookies included). */
static NvU32 nvapi_struct_size(const void *p, NvU32 max_size) {
    NvU32 ver = ((const NvU32 *)p)[0];
    NvU32 size = ver & 0xFFFFu;
    if (size < sizeof(NvU32)) size = sizeof(NvU32);
    if (size > max_size) size = max_size;
    return size;
}

static NvAPI_Status fill_memory_info(NvPhysicalGpuHandle h, void *p, NvU32 id) {
    acuda_nvapi_gpu_t *g;
    NV_DISPLAY_DRIVER_MEMORY_INFO_BASE *m;
    NvU32 size, ver, ded, avail, cur;
    (void)id;
    acuda_nvapi_ensure_discovered();
    if (!p) return NVAPI_INVALID_ARGUMENT;
    g = acuda_nvapi_find_gpu_by_handle(h);
    if (!g) return NVAPI_INVALID_ARGUMENT;
    m = (NV_DISPLAY_DRIVER_MEMORY_INFO_BASE *)p;
    ver = m->version;
    size = nvapi_struct_size(p, sizeof(*m));
    memset(p, 0, size);
    m->version = ver;
    /* Defaults from real NVAPI dump (RTX 3070 Laptop, ver=0x20018). */
    ded   = acuda_nvapi_env_u32("ACUDA_NVAPI_MEM_DEDICATED_KB", 8388608u);
    avail = acuda_nvapi_env_u32("ACUDA_NVAPI_MEM_AVAILABLE_KB", 8210432u);
    cur   = acuda_nvapi_env_u32("ACUDA_NVAPI_MEM_CUR_AVAILABLE_KB", 8206876u);
    if (size >= offsetof(NV_DISPLAY_DRIVER_MEMORY_INFO_BASE, dedicatedVideoMemory) + 4)
        m->dedicatedVideoMemory = ded;
    if (size >= offsetof(NV_DISPLAY_DRIVER_MEMORY_INFO_BASE, availableDedicatedVideoMemory) + 4)
        m->availableDedicatedVideoMemory = avail;
    if (size >= offsetof(NV_DISPLAY_DRIVER_MEMORY_INFO_BASE, systemVideoMemory) + 4)
        m->systemVideoMemory = acuda_nvapi_env_u32("ACUDA_NVAPI_MEM_SYSTEM_KB", 0u);
    if (size >= offsetof(NV_DISPLAY_DRIVER_MEMORY_INFO_BASE, sharedSystemMemory) + 4)
        m->sharedSystemMemory = acuda_nvapi_env_u32("ACUDA_NVAPI_MEM_SHARED_KB", 8050716u);
    if (size >= offsetof(NV_DISPLAY_DRIVER_MEMORY_INFO_BASE, curAvailableDedicatedVideoMemory) + 4)
        m->curAvailableDedicatedVideoMemory = cur;
    acuda_nvapi_log_msg("GetMemoryInfo id=0x%x h=%p ver=0x%x size=%u ded=%u avail=%u sys=%u shared=%u cur=%u",
                        id, h, m->version, size, m->dedicatedVideoMemory, m->availableDedicatedVideoMemory,
                        m->systemVideoMemory, m->sharedSystemMemory, m->curAvailableDedicatedVideoMemory);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_GPU_GetMemoryInfo(NvPhysicalGpuHandle hPhysicalGpu, NV_DISPLAY_DRIVER_MEMORY_INFO *pMemoryInfo) {
    return fill_memory_info(hPhysicalGpu, pMemoryInfo, 0x07f9b368u);
}
static NvAPI_Status NvAPI_GPU_GetMemoryInfoEx(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_MEMORY_INFO_EX *pMemoryInfo) {
    return fill_memory_info(hPhysicalGpu, pMemoryInfo, 0x774aa982u);
}
static NvAPI_Status NvAPI_GPU_GetCurrentPCIEDownstreamWidth(NvPhysicalGpuHandle h, NvU32 *w) {
    (void)h;
    if (!w) return NVAPI_INVALID_ARGUMENT;
    *w = 16;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetTachReading(NvPhysicalGpuHandle h, NvU32 *v) {
    (void)h;
    if (!v) return NVAPI_INVALID_ARGUMENT;
    *v = 0;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetThermalSettings(NvPhysicalGpuHandle hPhysicalGpu, NvU32 sensorIndex, NV_GPU_THERMAL_SETTINGS *pThermalSettings) {
    (void)hPhysicalGpu; (void)sensorIndex;
    if (pThermalSettings) {
        NvU32 ver = pThermalSettings->version;
        memset(pThermalSettings, 0, nvapi_struct_size(pThermalSettings, sizeof(*pThermalSettings)));
        pThermalSettings->version = ver;
        pThermalSettings->count = 0;
    }
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetCurrentPstate(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATE_ID *pCurrentPstate) {
    (void)hPhysicalGpu;
    if (pCurrentPstate) *pCurrentPstate = NVAPI_GPU_PERF_PSTATE_P0;
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetDynamicPstatesInfoEx(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_DYNAMIC_PSTATES_INFO_EX *pDynamicPstatesInfoEx) {
    (void)hPhysicalGpu;
    if (pDynamicPstatesInfoEx) {
        NvU32 ver = pDynamicPstatesInfoEx->version;
        memset(pDynamicPstatesInfoEx, 0, nvapi_struct_size(pDynamicPstatesInfoEx, sizeof(*pDynamicPstatesInfoEx)));
        pDynamicPstatesInfoEx->version = ver;
    }
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetPstates20(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO *pPstatesInfo) {
    (void)hPhysicalGpu;
    if (pPstatesInfo) {
        NvU32 ver = pPstatesInfo->version;
        memset(pPstatesInfo, 0, nvapi_struct_size(pPstatesInfo, sizeof(*pPstatesInfo)));
        pPstatesInfo->version = ver;
    }
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_GPU_GetAllClockFrequencies(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_CLOCK_FREQUENCIES *pClkFreqs) {
    (void)hPhysicalGpu;
    if (pClkFreqs) {
        NvU32 ver = pClkFreqs->version;
        memset(pClkFreqs, 0, nvapi_struct_size(pClkFreqs, sizeof(*pClkFreqs)));
        pClkFreqs->version = ver;
    }
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_Stub_Success(void) { return NVAPI_OK; }

#if defined(_WIN32)
static __declspec(thread) NvU32 g_stub_last_id;
#else
static __thread NvU32 g_stub_last_id;
#endif

/* PhysXCore (2.8.x era) queries these interfaces; return success when present
 * to avoid hard-fail paths in PPU/GPU detection. Signatures vary by NVAPI
 * version/caller, so accept and ignore arguments in a generic way. */
static NvAPI_Status NvAPI_Stub_Success_1(void *a) {
    (void)a;
    acuda_nvapi_log_msg("stub1 id=0x%08x", g_stub_last_id);
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_Stub_Success_2(void *a, void *b) {
    void *ra = NULL;
#if defined(_WIN32)
    ra = _ReturnAddress();
#endif
    acuda_nvapi_log_caller("stub2", ra);
    acuda_nvapi_log_msg("stub2 id=0x%08x a=%p b=%p", g_stub_last_id, a, b);
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_Stub_Success_3(void *a, void *b, void *c) {
    void *ra = NULL;
#if defined(_WIN32)
    ra = _ReturnAddress();
#endif
    acuda_nvapi_log_caller("stub3", ra);
    acuda_nvapi_log_msg("stub3 id=0x%08x a=%p b=%p c=%p", g_stub_last_id, a, b, c);
    return NVAPI_OK;
}
static NvAPI_Status NvAPI_Stub_Success_4(void *a, void *b, void *c, void *d) {
    void *ra = NULL;
#if defined(_WIN32)
    ra = _ReturnAddress();
#endif
    acuda_nvapi_log_caller("stub4", ra);
    acuda_nvapi_log_msg("stub4 id=0x%08x a=%p b=%p c=%p d=%p", g_stub_last_id, a, b, c, d);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_Stub_NoImpl_4(void *a, void *b, void *c, void *d) {
    void *ra = NULL;
#if defined(_WIN32)
    ra = _ReturnAddress();
#endif
    acuda_nvapi_log_caller("stub-noimpl", ra);
    acuda_nvapi_log_msg("stub-noimpl id=0x%08x a=%p b=%p c=%p d=%p", g_stub_last_id, a, b, c, d);
    /* Official NVAPI_NO_IMPLEMENTATION is -3, but nvoglv64 hangs on -3 for
     * some private IDs; old shim value -8 (NVAPI_INVALID_HANDLE in official
     * headers) preserves the previous working behavior. */
    return (NvAPI_Status)(-8);
}

static NvAPI_Status NvAPI_ID_84244524(void *a, void *b, void *c, void *d) {
    void *ra = NULL;
#if defined(_WIN32)
    ra = _ReturnAddress();
#endif
    acuda_nvapi_log_caller("84244524", ra);
    acuda_nvapi_log_msg("call 84244524 a=%p b=%p c=%p d=%p", a, b, c, d);
    return NVAPI_OK;
}

static NvAPI_Status NvAPI_ID_CB7B09D2(void *a, void *b, void *c, void *d) {
    void *ra = NULL;
#if defined(_WIN32)
    ra = _ReturnAddress();
#endif
    acuda_nvapi_log_caller("cb7b09d2", ra);
    acuda_nvapi_log_msg("call cb7b09d2 a=%p b=%p c=%p d=%p", a, b, c, d);
    return NVAPI_OK;
}

ACUDA_NVAPI_EXPORT void *ACUDA_NVAPI_CALL nvapi_QueryInterface(NvU32 interface_id) {
    void *result = NULL;
    g_stub_last_id = interface_id;
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
        case 0x5018ed61u: result = (void *)NvAPI_GetPhysicalGPUFromUnAttachedDisplay; break;
        case 0x20de9260u: result = (void *)NvAPI_EnumNvidiaUnAttachedDisplayHandle; break;
        case 0x4888d790u: result = (void *)NvAPI_GetUnAttachedAssociatedDisplayName; break;
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
        case 0xa561fd7du: result = (void *)NvAPI_GPU_GetVbiosVersionString; break;
        case 0xd048c3b1u: result = (void *)NvAPI_GPU_GetCurrentPCIEDownstreamWidth; break;
        case 0x5f608315u: result = (void *)NvAPI_GPU_GetTachReading; break;
        case 0xe3640a56u: result = (void *)NvAPI_GPU_GetThermalSettings; break;
        case 0x927da4f6u: result = (void *)NvAPI_GPU_GetCurrentPstate; break;
        case 0x60ded2edu: result = (void *)NvAPI_GPU_GetDynamicPstatesInfoEx; break;
        case 0x6ff81213u: result = (void *)NvAPI_GPU_GetPstates20; break;
        case 0xdcb616c3u: result = (void *)NvAPI_GPU_GetAllClockFrequencies; break;
        case 0x07f9b368u: result = (void *)NvAPI_GPU_GetMemoryInfo; break;
        case 0x774aa982u: result = (void *)NvAPI_GPU_GetMemoryInfoEx; break;
        case 0x3092ac32u: result = (void *)NvAPI_Stub_Success_2; break;
        case 0x33c7358cu: result = (void *)NvAPI_Stub_Success; break;
        case 0x593e8644u: result = (void *)NvAPI_Stub_Success; break;
        case 0xad298d3fu: result = (void *)NvAPI_Stub_Success_4; break; /* private init, seen in nvoglv64 */
        /* PhysXCore (2.8.4) lookups observed in decompiled binary */
        case 0x84244524u: result = (void *)NvAPI_ID_84244524; break;
        case 0xcb7b09d2u: result = (void *)NvAPI_ID_CB7B09D2; break;
        /* Private/unknown IDs observed from nvoglv64; returning NVAPI_OK with
         * unwritten outputs can hang the GL driver. Return legacy -8 shim
         * status (see NvAPI_Stub_NoImpl_4). */
        case 0xf0f4a5e0u: result = (void *)NvAPI_Stub_NoImpl_4; break; /* D3D_CudaInteropGetObjectHandle - private */
        case 0x1efc3957u: result = (void *)NvAPI_Stub_NoImpl_4; break;
        /* D3D/GL interop - not fully implementable in shim; explicit noimpl avoids silent success. */
        case 0xc83c4d5du: result = (void *)NvAPI_Stub_NoImpl_4; break; /* NvAPI_D3D11_RegisterDeviceFromOGL */
        case 0x64d6c83du: result = (void *)NvAPI_Stub_NoImpl_4; break; /* NvAPI_D3D_CudaInteropFunction */
        default:
            acuda_nvapi_log_msg("qi-default id=0x%08x", interface_id);
            result = (void *)NvAPI_Stub_Success_4;
            break;
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
