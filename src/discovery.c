#include "nvapi_internal.h"
#include <acuda/driver.h>

acuda_nvapi_gpu_t *g_gpus;
NvU32 g_gpu_count;
NV_COMPUTE_GPU *g_compute_gpus;
static int g_discovered;

void acuda_nvapi_log_msg(const char *fmt, ...) {
    const char *path = getenv("ACUDA_NVAPI_LOG");
    va_list ap;
    FILE *f;
    if (!path || !*path) return;
    f = fopen(path, "a");
    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

void acuda_nvapi_log_caller(const char *tag, void *ra) {
#if defined(_WIN32)
    HMODULE module = NULL;
    char path[MAX_PATH] = "";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)ra, &module) && module) {
        GetModuleFileNameA(module, path, sizeof(path));
        acuda_nvapi_log_msg("caller %s ra=%p base=%p rva=0x%lx module=%s", tag, ra,
                            (void *)module, (unsigned long)((size_t)ra - (size_t)module), path);
    } else {
        acuda_nvapi_log_msg("caller %s ra=%p module=?", tag, ra);
    }
#else
    (void)tag; (void)ra;
#endif
}

NvU32 acuda_nvapi_make_version(size_t size, NvU32 ver) {
    return (NvU32)size | (ver << 16);
}

NvU32 acuda_nvapi_env_u32(const char *name, NvU32 fallback) {
    const char *s = getenv(name);
    char *end = NULL;
    unsigned long v;
    if (!s || !*s) return fallback;
    v = strtoul(s, &end, 0);
    if (end == s) return fallback;
    return (NvU32)v;
}

const char *acuda_nvapi_env_str(const char *name, const char *fallback) {
    const char *s = getenv(name);
    return (s && *s) ? s : fallback;
}

void acuda_nvapi_copy_short_string(char *dst, const char *src) {
    size_t n = src ? strlen(src) : 0;
    if (n >= 64) n = 63;
    if (n) memcpy(dst, src, n);
    dst[n] = '\0';
}

NvPhysicalGpuHandle acuda_nvapi_gpu_handle(const acuda_nvapi_gpu_t *g) {
    /* Real NVAPI physical GPU handle/GPUID/CUDA attr 0x20080402 are the same
     * value (e.g. 0x100). Keep them unified for PhysX device matching. */
    return (NvPhysicalGpuHandle)(size_t)g->ppu_key;
}

acuda_nvapi_gpu_t *acuda_nvapi_find_gpu_by_handle(NvPhysicalGpuHandle h) {
    NvU32 i;
    NvU32 key = (NvU32)(size_t)h;
    for (i = 0; i < g_gpu_count; i++) {
        if (g_gpus[i].valid && (g_gpus[i].ppu_key == key || g_gpus[i].gpu_id == key))
            return &g_gpus[i];
    }
    /* Accept legacy normalized/unmasked forms. */
    key &= ~0x100u;
    for (i = 0; i < g_gpu_count; i++) {
        if (g_gpus[i].valid && (g_gpus[i].ppu_key == key || g_gpus[i].gpu_id == key))
            return &g_gpus[i];
    }
    return NULL;
}

acuda_nvapi_gpu_t *acuda_nvapi_find_gpu_by_id(NvU32 id) {
    NvU32 i;
    for (i = 0; i < g_gpu_count; i++) {
        if (g_gpus[i].valid && (g_gpus[i].gpu_id == id || g_gpus[i].ppu_key == id))
            return &g_gpus[i];
    }
    return NULL;
}

void acuda_nvapi_free_gpus(void) {
    free(g_gpus);
    free(g_compute_gpus);
    g_gpus = NULL;
    g_compute_gpus = NULL;
    g_gpu_count = 0;
}

static int alloc_gpus(NvU32 count) {
    acuda_nvapi_gpu_t *g;
    NV_COMPUTE_GPU *cg;
    acuda_nvapi_free_gpus();
    if (count == 0) return 1;
    g = (acuda_nvapi_gpu_t *)calloc(count, sizeof(*g));
    cg = (NV_COMPUTE_GPU *)calloc(count, sizeof(*cg));
    if (!g || !cg) {
        free(g);
        free(cg);
        g_gpu_count = 0;
        return 0;
    }
    g_gpus = g;
    g_compute_gpus = cg;
    g_gpu_count = count;
    return 1;
}

static void parse_bdf(const char *s, NvU32 *domain, NvU32 *bus, NvU32 *dev,
                      NvU32 *func) {
    unsigned d = 0, b = 0, dv = 0, f = 0;
    if (!s) return;
    if (sscanf(s, "%x:%x:%x.%x", &d, &b, &dv, &f) == 4 ||
        sscanf(s, "%x:%x:%x", &d, &b, &dv) == 3) {
        *domain = d; *bus = b; *dev = dv; *func = f;
    }
}

#if defined(_WIN32)
/* Recover the PCI vendor/device/subsystem/revision IDs from the Windows
 * device registry, matched by bus/device/function.  Newer PhysX-era GPU
 * checks read these through NvAPI_GPU_GetPCIIdentifiers; reporting zeros
 * (as before) makes the GPU unidentifiable.  LocationInformation for PCI
 * devices is usually a MUI reference of the form
 * "@...;PCI bus %1, device %2, function %3;(bus,device,function)".
 * Environment overrides (ACUDA_NVAPI_VENDOR_ID and friends, hex or
 * decimal) take precedence when set. */
static void discover_pci_ids(acuda_nvapi_gpu_t *g) {
    NvU32 ven, dev, sub, rev;
    HKEY pci = NULL, hdev = NULL;
    char subname[256];
    DWORD idx = 0;
    ven = acuda_nvapi_env_u32("ACUDA_NVAPI_VENDOR_ID", 0);
    dev = acuda_nvapi_env_u32("ACUDA_NVAPI_DEVICE_ID", 0);
    sub = acuda_nvapi_env_u32("ACUDA_NVAPI_SUBSYSTEM_ID", 0);
    rev = acuda_nvapi_env_u32("ACUDA_NVAPI_REVISION_ID", 0);
    if (ven && dev) {
        g->vendor_id = ven;
        g->device_id = dev;
        g->subsystem_id = sub;
        g->revision_id = rev;
        return;
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SYSTEM\\CurrentControlSet\\Enum\\PCI",
                      0, KEY_READ, &pci) != ERROR_SUCCESS)
        return;
    while (RegEnumKeyA(pci, idx++, subname, sizeof(subname)) == ERROR_SUCCESS) {
        unsigned vv = 0, dd = 0, ss = 0, rr = 0;
        DWORD j = 0;
        if (sscanf(subname, "VEN_%x&DEV_%x&SUBSYS_%x&REV_%x",
                   &vv, &dd, &ss, &rr) != 4)
            continue;
        if (RegOpenKeyExA(pci, subname, 0, KEY_READ, &hdev) != ERROR_SUCCESS)
            continue;
        for (;;) {
            char inst[256];
            HKEY h = NULL;
            char loc[256] = {0};
            DWORD locsz = sizeof(loc), type = 0, b = 0, d = 0, f = 0;
            int matched = 0;
            if (RegEnumKeyA(hdev, j++, inst, sizeof(inst)) != ERROR_SUCCESS)
                break;
            if (RegOpenKeyExA(hdev, inst, 0, KEY_READ, &h) != ERROR_SUCCESS)
                continue;
            if (RegQueryValueExA(h, "LocationInformation", NULL, &type,
                                 (LPBYTE)loc, &locsz) == ERROR_SUCCESS &&
                (type == REG_SZ || type == REG_EXPAND_SZ)) {
                if (loc[0] == '@') {
                    /* MUI reference: take the plain numbers in the
                     * trailing "(bus,device,function)" argument list. */
                    char *lp = strrchr(loc, '(');
                    char *rp;
                    if (lp && (rp = strrchr(lp, ')')) != NULL) {
                        *rp = 0;
                        if (sscanf(lp + 1, "%u,%u,%u", &b, &d, &f) == 3)
                            matched = (b == g->pci_bus &&
                                       d == g->pci_device &&
                                       f == g->pci_function);
                    }
                } else if (sscanf(loc, "PCI bus %u, device %u, function %u",
                                  &b, &d, &f) == 3) {
                    matched = (b == g->pci_bus && d == g->pci_device &&
                               f == g->pci_function);
                }
            }
            RegCloseKey(h);
            if (matched) {
                g->vendor_id = (NvU32)vv;
                g->device_id = (NvU32)dd;
                g->subsystem_id = (NvU32)ss;
                g->revision_id = (NvU32)rr;
                RegCloseKey(hdev);
                RegCloseKey(pci);
                return;
            }
        }
        RegCloseKey(hdev);
        hdev = NULL;
    }
    if (hdev) RegCloseKey(hdev);
    RegCloseKey(pci);
}
#endif

static void discover_from_acuda(void) {
    int count = 0;
    int i;

    if (cuInit(0) != CUDA_SUCCESS) {
        acuda_nvapi_log_msg("nvapi: cuInit failed (acuda driver unavailable)");
        return;
    }
    if (cuDeviceGetCount(&count) != CUDA_SUCCESS || count <= 0) {
        acuda_nvapi_log_msg("nvapi: cuDeviceGetCount failed/count=%d", count);
        return;
    }
    if (!alloc_gpus((NvU32)count)) {
        acuda_nvapi_log_msg("nvapi: failed to allocate %d GPUs", count);
        return;
    }
    for (i = 0; i < count; i++) {
        acuda_nvapi_gpu_t *g = &g_gpus[i];
        char bdf[64] = {0};
        int v = 0;
        size_t mem = 0;
        CUdevice dev = (CUdevice)i;
        memset(g, 0, sizeof(*g));
        g->valid = 1;
        g->gpu_id = acuda_nvapi_env_u32("ACUDA_NVAPI_GPU_ID", 0);
        cuDeviceGetName(g->name, sizeof(g->name), dev);
        if (cuDeviceGetPCIBusId(bdf, sizeof(bdf), dev) == CUDA_SUCCESS)
            parse_bdf(bdf, &g->pci_domain, &g->pci_bus, &g->pci_device,
                      &g->pci_function);
        if (cuDeviceGetAttribute(&v, CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, dev) == CUDA_SUCCESS)
            g->pci_domain = (NvU32)v;
        v = 0;
        if (cuDeviceGetAttribute(&v, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, dev) == CUDA_SUCCESS)
            g->pci_bus = (NvU32)v;
        v = 0;
        if (cuDeviceGetAttribute(&v, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, dev) == CUDA_SUCCESS)
            g->pci_device = (NvU32)v;
        v = 0;
        if (cuDeviceGetAttribute(&v, 0x20080402, dev) == CUDA_SUCCESS)
            g->ppu_key = (NvU32)v;
        if (cuDeviceTotalMem(&mem, dev) == CUDA_SUCCESS)
            g->memory_kb = (NvU32)(mem / 1024ull);
#if defined(_WIN32)
        discover_pci_ids(g);
#endif
        if (!g->ppu_key) {
            /* Default to real-NVAPI-like PPU handle/id when CUDA doesn't report
             * attr 0x20080402; ensure bit 0x100 is present for legacy PhysX. */
            g->ppu_key = 0x100u | (NvU32)i;
        }
        if (!g->gpu_id)
            g->gpu_id = g->ppu_key;
        if (!g->name[0]) acuda_nvapi_copy_short_string(g->name, "ACuda Graphics Device");
        acuda_nvapi_log_msg("nvapi: gpu[%d] name=%s bdf=%04x:%02x:%02x.%x ppu=0x%x mem_kb=%u pci=%04x:%04x:%08x:%02x",
                            i, g->name, g->pci_domain, g->pci_bus, g->pci_device,
                            g->pci_function, g->ppu_key, g->memory_kb,
                            g->vendor_id, g->device_id, g->subsystem_id,
                            g->revision_id);
    }
}

void acuda_nvapi_ensure_discovered(void) {
    acuda_nvapi_gpu_t *g;
    if (g_discovered) return;
    g_discovered = 1;
    acuda_nvapi_free_gpus();
    discover_from_acuda();
    if (g_gpu_count == 0) {
        if (!alloc_gpus(1)) return;
        g = &g_gpus[0];
        g->valid = 1;
        g->ppu_key = acuda_nvapi_env_u32("ACUDA_NVAPI_GPU_HANDLE", 0x100u);
        if (!g->ppu_key) g->ppu_key = 0x100u;
        g->gpu_id = acuda_nvapi_env_u32("ACUDA_NVAPI_GPU_ID", g->ppu_key);
        if (!g->gpu_id) g->gpu_id = g->ppu_key;
        g->pci_domain = 0;
        g->pci_bus = acuda_nvapi_env_u32("ACUDA_NVAPI_PCI_BUS", 0);
        g->pci_device = acuda_nvapi_env_u32("ACUDA_NVAPI_PCI_SLOT", 0);
#if defined(_WIN32)
        discover_pci_ids(g);
#endif
        acuda_nvapi_copy_short_string(g->name,
                                      acuda_nvapi_env_str("ACUDA_NVAPI_GPU_NAME", "ACuda Graphics Device"));
        acuda_nvapi_log_msg("nvapi: fallback to single acuda GPU (driver not available)");
    }
}

void acuda_nvapi_set_discovered(int v) { g_discovered = v; }
int acuda_nvapi_is_discovered(void) { return g_discovered; }
