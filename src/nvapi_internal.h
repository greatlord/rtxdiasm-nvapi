#ifndef ACUDA_NVAPI_INTERNAL_H
#define ACUDA_NVAPI_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <intrin.h>
#include <windows.h>
#define ACUDA_NVAPI_EXPORT __declspec(dllexport)
#define ACUDA_NVAPI_CALL __cdecl
#else
#include <dlfcn.h>
#define ACUDA_NVAPI_EXPORT __attribute__((visibility("default")))
#define ACUDA_NVAPI_CALL
#endif

typedef unsigned int NvU32;
typedef int NvAPI_Status;
typedef void *NvDisplayHandle;
typedef void *NvPhysicalGpuHandle;
typedef void *NvLogicalGpuHandle;
typedef char NvAPI_ShortString[64];

#define NVAPI_OK 0
#define NVAPI_ERROR (-1)
#define NVAPI_INVALID_ARGUMENT (-5)
#define NVAPI_NVIDIA_DEVICE_NOT_FOUND (-6)
#define NVAPI_END_ENUMERATION (-7)
#define NVAPI_INCOMPATIBLE_STRUCT_VERSION (-9)
#define NVAPI_NO_IMPLEMENTATION (-8)

#define FAKE_DISPLAY_HANDLE ((NvDisplayHandle)(size_t)0x100)
#define NVAPI_MAX_PHYSICAL_GPUS 64
#define NVAPI_MAX_LOGICAL_GPUS 64

typedef struct {
    NvU32 version;
    NvU32 drvVersion;
    NvU32 bldChangeListNum;
    NvAPI_ShortString szBuildBranchString;
    NvAPI_ShortString szAdapterString;
} NV_DISPLAY_DRIVER_VERSION;

typedef struct {
    NvPhysicalGpuHandle hPhysicalGpu;
    NvU32 flags;
} NV_COMPUTE_GPU;

#define NVAPI_MAX_GPU_PER_TOPOLOGY 8

typedef struct {
    NvU32 version;
    NvU32 gpuCount;
    struct {
        NvPhysicalGpuHandle hPhysicalGpu;
        NvU32 flags;
    } computeGpus[NVAPI_MAX_GPU_PER_TOPOLOGY];
} NV_COMPUTE_GPU_TOPOLOGY_V1;

typedef struct {
    NvU32 version;
    NvU32 gpuCount;
    NV_COMPUTE_GPU *computeGpus;
} NV_COMPUTE_GPU_TOPOLOGY_V2;

#define NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_CAPABLE (1u << 0)
#define NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_ENABLE (1u << 1)
#define NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_RECOMMENDED (1u << 3)
#define NV_COMPUTE_GPU_TOPOLOGY_CUDA_AVAILABLE (1u << 4)
#define NV_COMPUTE_GPU_TOPOLOGY_CUDA_CAPABLE (1u << 16)
#define NV_COMPUTE_GPU_TOPOLOGY_PHYSICS_AVAILABLE (1u << 21)

typedef struct {
    int valid;
    NvU32 gpu_id;
    NvU32 ppu_key; /* matches CUDA device attribute 0x20080402 */
    NvU32 pci_domain;
    NvU32 pci_bus;
    NvU32 pci_device;
    NvU32 pci_function;
    NvU32 vendor_id;
    NvU32 device_id;
    NvU32 subsystem_id;
    NvU32 revision_id;
    NvU32 memory_kb;
    NvAPI_ShortString name;
} acuda_nvapi_gpu_t;

extern acuda_nvapi_gpu_t *g_gpus;
extern NvU32 g_gpu_count;
extern NV_COMPUTE_GPU *g_compute_gpus;

void acuda_nvapi_log_msg(const char *fmt, ...);
void acuda_nvapi_log_caller(const char *tag, void *ra);
#if defined(_WIN32)
#define LOG_CALLER(tag) acuda_nvapi_log_caller((tag), _ReturnAddress())
#else
#define LOG_CALLER(tag) acuda_nvapi_log_caller((tag), NULL)
#endif

NvU32 acuda_nvapi_make_version(size_t size, NvU32 ver);
NvU32 acuda_nvapi_env_u32(const char *name, NvU32 fallback);
const char *acuda_nvapi_env_str(const char *name, const char *fallback);
void acuda_nvapi_copy_short_string(char *dst, const char *src);
NvPhysicalGpuHandle acuda_nvapi_gpu_handle(const acuda_nvapi_gpu_t *g);
acuda_nvapi_gpu_t *acuda_nvapi_find_gpu_by_handle(NvPhysicalGpuHandle h);
acuda_nvapi_gpu_t *acuda_nvapi_find_gpu_by_id(NvU32 id);

void acuda_nvapi_ensure_discovered(void);
void acuda_nvapi_free_gpus(void);
void acuda_nvapi_set_discovered(int v);
int acuda_nvapi_is_discovered(void);

#endif
