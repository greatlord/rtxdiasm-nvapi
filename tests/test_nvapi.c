#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define ACUDA_LIB_HANDLE HMODULE
#define acuda_open_lib(p) LoadLibraryA(p)
#define acuda_get_sym(h, n) ((void *)GetProcAddress((HMODULE)(h), (n)))
#define acuda_close_lib(h) FreeLibrary((HMODULE)(h))
#else
#include <dlfcn.h>
#define ACUDA_LIB_HANDLE void *
#define acuda_open_lib(p) dlopen((p), RTLD_NOW)
#define acuda_get_sym(h, n) dlsym((h), (n))
#define acuda_close_lib(h) dlclose(h)
#endif

typedef unsigned int NvU32;
typedef int NvAPI_Status;
typedef void *NvPhysicalGpuHandle;
typedef void *NvLogicalGpuHandle;
typedef void *NvDisplayHandle;
typedef void *(*nvapi_query_fn)(NvU32);
typedef char NvAPI_ShortString[64];

#define NVAPI_OK 0
#define NVAPI_END_ENUMERATION (-7)

typedef struct { NvPhysicalGpuHandle hPhysicalGpu; NvU32 flags; } NV_COMPUTE_GPU;
typedef struct {
    NvU32 version; NvU32 gpuCount; NV_COMPUTE_GPU *computeGpus;
} NV_COMPUTE_GPU_TOPOLOGY_V2;

static int g_fail;
#define CHECK(c, msg) do { if (!(c)) { \
    fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail++; \
} else { fprintf(stderr, "ok: %s\n", msg); } } while (0)

static NvU32 mkver(size_t sz, NvU32 v) { return (NvU32)sz | (v << 16); }

static ACUDA_LIB_HANDLE open_nvapi(const char *hint) {
    const char *names[] = {
#if defined(_WIN64) || (defined(_WIN32) && defined(_M_X64))
        "nvapi64.dll", "nvapi.dll",
#elif defined(_WIN32)
        "nvapi.dll", "nvapi64.dll",
#else
        "libnvidia-api.so.1", "libnvidia-api.so", "nvapi.so",
#endif
        NULL
    };
    int i;
    if (hint && *hint) {
        ACUDA_LIB_HANDLE h = acuda_open_lib(hint);
        if (h) return h;
    }
    for (i = 0; names[i]; i++) {
        ACUDA_LIB_HANDLE h = acuda_open_lib(names[i]);
        if (h) return h;
    }
    return NULL;
}

static void *open_acuda_driver(void) {
#if defined(_WIN32)
    const char *names[] = {"nvcuda.dll", "cuda.dll", NULL};
#else
    const char *names[] = {"libcuda.so.1", "libcuda.so", NULL};
#endif
    int i;
    for (i = 0; names[i]; i++) {
        void *h = acuda_open_lib(names[i]);
        if (h) return h;
    }
    return NULL;
}

int main(int argc, char **argv) {
    ACUDA_LIB_HANDLE nv = NULL;
    nvapi_query_fn query = NULL;
    NvAPI_Status st;
    NvPhysicalGpuHandle gpus[64];
    NvU32 count = 0;
    NvU32 i;

    nv = open_nvapi(argc > 1 ? argv[1] : NULL);
    CHECK(nv != NULL, "load nvapi library");
    if (!nv) return 1;

    query = (nvapi_query_fn)acuda_get_sym(nv, "nvapi_QueryInterface");
    if (!query) query = (nvapi_query_fn)acuda_get_sym(nv, "nvapi_QueryInterface@4");
    CHECK(query != NULL, "nvapi_QueryInterface symbol");
    if (!query) return 1;

    {
        NvAPI_Status (*init_fn)(void) = (NvAPI_Status (*)(void))query(0x0150e828u);
        CHECK(init_fn != NULL, "NvAPI_Initialize resolved");
        if (init_fn) {
            st = init_fn();
            CHECK(st == NVAPI_OK, "NvAPI_Initialize returns OK");
        }
    }

    {
        NvAPI_Status (*enum_gpus)(NvPhysicalGpuHandle *, NvU32 *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle *, NvU32 *))query(0xe5ac921fu);
        CHECK(enum_gpus != NULL, "NvAPI_EnumPhysicalGPUs resolved");
        if (enum_gpus) {
            memset(gpus, 0, sizeof(gpus));
            st = enum_gpus(gpus, &count);
            CHECK(st == NVAPI_OK, "NvAPI_EnumPhysicalGPUs returns OK");
            CHECK(count >= 1, "at least one GPU discovered (from acuda listing)");
            fprintf(stderr, "info: gpu count=%u\n", count);
        }
    }

    for (i = 0; i < count && i < 64; i++) {
        NvAPI_ShortString name = {0};
        NvU32 bus = 0, slot = 0, fb = 0, devid = 0, subid = 0, rev = 0, extid = 0;
        NvU32 gpu_id = 0;
        NvAPI_Status (*get_name)(NvPhysicalGpuHandle, char *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle, char *))query(0xceee8e9fu);
        NvAPI_Status (*get_bus)(NvPhysicalGpuHandle, NvU32 *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle, NvU32 *))query(0x1be0b8e5u);
        NvAPI_Status (*get_slot)(NvPhysicalGpuHandle, NvU32 *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle, NvU32 *))query(0x2a0a350fu);
        NvAPI_Status (*get_fb)(NvPhysicalGpuHandle, NvU32 *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle, NvU32 *))query(0x46fbeb03u);
        NvAPI_Status (*get_pci)(NvPhysicalGpuHandle, NvU32 *, NvU32 *, NvU32 *, NvU32 *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle, NvU32 *, NvU32 *, NvU32 *, NvU32 *))query(0x2ddfb66eu);
        NvAPI_Status (*get_id)(NvPhysicalGpuHandle, NvU32 *) =
            (NvAPI_Status (*)(NvPhysicalGpuHandle, NvU32 *))query(0x6533ea3eu);

        if (get_name) { st = get_name(gpus[i], name); CHECK(st == NVAPI_OK, "GPU_GetFullName"); }
        if (get_bus) { st = get_bus(gpus[i], &bus); CHECK(st == NVAPI_OK, "GPU_GetBusId"); }
        if (get_slot) { st = get_slot(gpus[i], &slot); CHECK(st == NVAPI_OK, "GPU_GetBusSlotId"); }
        if (get_fb) { st = get_fb(gpus[i], &fb); CHECK(st == NVAPI_OK, "GPU_GetPhysicalFrameBufferSize"); }
        if (get_pci) { st = get_pci(gpus[i], &devid, &subid, &rev, &extid); CHECK(st == NVAPI_OK, "GPU_GetPCIIdentifiers"); }
        if (get_id) { st = get_id(gpus[i], &gpu_id); CHECK(st == NVAPI_OK, "GetGPUIDfromPhysicalGPU"); }
        fprintf(stderr, "info: gpu[%u] handle=%p id=%u name='%s' bus=%u slot=%u fb_kb=%u\n",
                i, gpus[i], gpu_id, name, bus, slot, fb);
        fprintf(stderr, "info: gpu[%u] pci dev=0x%x sub=0x%x rev=0x%x ext=0x%x\n",
                i, devid, subid, rev, extid);
        CHECK(name[0] != 0, "GPU name not empty");
        if (count == 1) CHECK(gpus[i] != NULL, "physical GPU handle not NULL (PPU key)");
    }

    {
        NV_COMPUTE_GPU_TOPOLOGY_V2 topo;
        NV_COMPUTE_GPU gtmp[16];
        NvAPI_Status (*topo_fn)(void *) =
            (NvAPI_Status (*)(void *))query(0x5786cc6eu);
        CHECK(topo_fn != NULL, "CudaEnumComputeCapableGpus resolved");
        if (topo_fn) {
            memset(&topo, 0, sizeof(topo));
            memset(gtmp, 0, sizeof(gtmp));
            topo.version = mkver(sizeof(topo), 2);
            topo.computeGpus = gtmp;
            st = topo_fn(&topo);
            CHECK(st == NVAPI_OK, "CudaEnumComputeCapableGpus v2");
            CHECK(topo.gpuCount >= 1, "compute topology has GPUs");
            fprintf(stderr, "info: compute topology count=%u\n", topo.gpuCount);
        }
    }

    /* Verify NVAPI physical handle matches acuda driver's PPU attribute (0x20080402)
     * for the same GPUs acuda lists. This proves nvapi extracts from acuda. */
    {
        void *drv = open_acuda_driver();
        if (drv) {
            int (*cuInit)(unsigned int) = (int (*)(unsigned int))acuda_get_sym(drv, "cuInit");
            int (*cuDeviceGetCount)(int *) = (int (*)(int *))acuda_get_sym(drv, "cuDeviceGetCount");
            int (*cuDeviceGetAttribute)(int *, int, int) = (int (*)(int *, int, int))acuda_get_sym(drv, "cuDeviceGetAttribute");
            if (cuInit && cuDeviceGetCount && cuDeviceGetAttribute && cuInit(0) == 0) {
                int dcount = 0;
                if (cuDeviceGetCount(&dcount) == 0 && dcount > 0) {
                    int attr = 0;
                    if (cuDeviceGetAttribute(&attr, 0x20080402, 0) == 0) {
                        NvU32 ppu = (NvU32)attr;
                        fprintf(stderr, "info: acuda driver[0] PPU attr 0x20080402=0x%x (%u)\n", ppu, ppu);
                        if (count > 0) {
                            NvU32 nv_handle = (NvU32)(size_t)gpus[0];
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "NVAPI physical handle (0x%x) matches acuda PPU attr (0x%x)",
                                     nv_handle, ppu);
                            CHECK(nv_handle == ppu, msg);
                        }
                    }
                }
            } else {
                fprintf(stderr, "info: acuda driver present but cuInit/query unavailable\n");
            }
            acuda_close_lib(drv);
        } else {
            fprintf(stderr, "info: acuda driver (nvcuda/cuda) not loaded; skipping PPU match check\n");
        }
    }

    /* Test helper exports */
    {
        NvU32 (*get_count)(void) = (NvU32 (*)(void))acuda_get_sym(nv, "acuda_nvapi_get_gpu_count");
        if (get_count) {
            NvU32 c = get_count();
            CHECK(c == count, "acuda_nvapi_get_gpu_count matches EnumPhysicalGPUs");
        }
    }

    acuda_close_lib(nv);
    if (g_fail) {
        fprintf(stderr, "\n%d test(s) FAILED\n", g_fail);
        return 1;
    }
    fprintf(stderr, "\nall nvapi tests passed\n");
    return 0;
}
