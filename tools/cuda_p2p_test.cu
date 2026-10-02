#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CUDA_OK(call) do { \
    cudaError_t e = (call); \
    if (e != cudaSuccess) { \
        std::fprintf(stderr, "%s failed: %s\n", #call, cudaGetErrorString(e)); \
        return 2; \
    } \
} while (0)

int main() {
    int count = 0;
    CUDA_OK(cudaGetDeviceCount(&count));
    std::printf("CUDA devices: %d\n", count);
    if (count < 2) return 1;

    std::vector<int> cmp;
    for (int i = 0; i < count; ++i) {
        cudaDeviceProp p{};
        CUDA_OK(cudaGetDeviceProperties(&p, i));
        std::printf("GPU%d: %s  cc=%d.%d  pci=%04x:%02x:%02x\n",
                    i, p.name, p.major, p.minor,
                    p.pciDomainID, p.pciBusID, p.pciDeviceID);
        if (std::string(p.name).find("CMP 90HX") != std::string::npos)
            cmp.push_back(i);
    }

    if (cmp.size() < 2) {
        std::fprintf(stderr, "Need at least two CMP 90HX CUDA devices.\n");
        return 1;
    }

    std::puts("\nPeer-access matrix:");
    for (int a : cmp) {
        for (int b : cmp) {
            int can = 0;
            if (a != b) CUDA_OK(cudaDeviceCanAccessPeer(&can, a, b));
            std::printf("%d->%d:%s  ", a, b, a == b ? "-" : (can ? "YES" : "NO"));
        }
        std::puts("");
    }

    const size_t bytes = 256ull * 1024ull * 1024ull;
    const int iters = 32;

    for (size_t ai = 0; ai < cmp.size(); ++ai) {
        for (size_t bi = 0; bi < cmp.size(); ++bi) {
            if (ai == bi) continue;
            int a = cmp[ai], b = cmp[bi], can = 0;
            CUDA_OK(cudaDeviceCanAccessPeer(&can, a, b));
            if (!can) {
                std::printf("GPU%d -> GPU%d: P2P unavailable\n", a, b);
                continue;
            }

            void *src = nullptr, *dst = nullptr;
            cudaEvent_t start{}, stop{};

            CUDA_OK(cudaSetDevice(a));
            cudaError_t e = cudaDeviceEnablePeerAccess(b, 0);
            if (e != cudaSuccess && e != cudaErrorPeerAccessAlreadyEnabled) {
                std::fprintf(stderr, "enable peer %d->%d failed: %s\n", a, b, cudaGetErrorString(e));
                return 2;
            }
            CUDA_OK(cudaMalloc(&src, bytes));
            CUDA_OK(cudaMemset(src, 0x5a, bytes));

            CUDA_OK(cudaSetDevice(b));
            e = cudaDeviceEnablePeerAccess(a, 0);
            if (e != cudaSuccess && e != cudaErrorPeerAccessAlreadyEnabled) {
                std::fprintf(stderr, "enable peer %d->%d failed: %s\n", b, a, cudaGetErrorString(e));
                return 2;
            }
            CUDA_OK(cudaMalloc(&dst, bytes));
            CUDA_OK(cudaEventCreate(&start));
            CUDA_OK(cudaEventCreate(&stop));
            CUDA_OK(cudaEventRecord(start));

            for (int i = 0; i < iters; ++i)
                CUDA_OK(cudaMemcpyPeer(dst, b, src, a, bytes));

            CUDA_OK(cudaEventRecord(stop));
            CUDA_OK(cudaEventSynchronize(stop));

            float ms = 0.0f;
            CUDA_OK(cudaEventElapsedTime(&ms, start, stop));
            const double gb = (double)bytes * iters / 1e9;
            const double gbs = gb / (ms / 1000.0);
            std::printf("GPU%d -> GPU%d: %.2f GB/s  (%zu MiB x %d, %.1f ms)\n",
                        a, b, gbs, bytes / 1024 / 1024, iters, ms);

            CUDA_OK(cudaEventDestroy(start));
            CUDA_OK(cudaEventDestroy(stop));
            CUDA_OK(cudaFree(dst));
            CUDA_OK(cudaSetDevice(a));
            CUDA_OK(cudaFree(src));
        }
    }
    return 0;
}
