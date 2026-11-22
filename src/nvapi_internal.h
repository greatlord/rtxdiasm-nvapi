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

/* Use official NVIDIA NVAPI headers. */
#include "nvapi.h"

#ifndef FAKE_DISPLAY_HANDLE
#define FAKE_DISPLAY_HANDLE ((NvDisplayHandle)(size_t)0x100)
#endif

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
