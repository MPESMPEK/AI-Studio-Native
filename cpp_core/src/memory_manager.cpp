#include "../include/memory_manager.h"
#include <algorithm>
#include <stdio.h>

MemoryManager& MemoryManager::Get() {
    static MemoryManager instance;
    return instance;
}

MemoryManager::MemoryManager() : m_pd3dDevice(nullptr), m_pd3dDeviceContext(nullptr) {}

MemoryManager::~MemoryManager() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_trackedTextures.clear();
}

void MemoryManager::Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_pd3dDevice = pDevice;
    m_pd3dDeviceContext = pContext;
}

void MemoryManager::RegisterTexture(ID3D11ShaderResourceView** ppTextureView) {
    if (!ppTextureView) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto it = std::find(m_trackedTextures.begin(), m_trackedTextures.end(), ppTextureView);
    if (it == m_trackedTextures.end()) {
        m_trackedTextures.push_back(ppTextureView);
    }
}

void MemoryManager::UnregisterTexture(ID3D11ShaderResourceView** ppTextureView) {
    if (!ppTextureView) return;
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    auto it = std::find(m_trackedTextures.begin(), m_trackedTextures.end(), ppTextureView);
    if (it != m_trackedTextures.end()) {
        m_trackedTextures.erase(it);
    }
}

size_t MemoryManager::GetCurrentWorkingSetBytes() const {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}

size_t MemoryManager::GetPeakWorkingSetBytes() const {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.PeakWorkingSetSize;
    }
    return 0;
}

std::string MemoryManager::GetLastStatusMessage() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_lastStatusMsg;
}

void MemoryManager::SetLastStatusMessage(const std::string& msg) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_lastStatusMsg = msg;
}

size_t MemoryManager::PurgeAll() {
    bool expected = false;
    if (!m_isPurging.compare_exchange_strong(expected, true)) {
        return 0; // Hindari re-entrancy atau purging ganda saat thread sedang bekerja
    }

    SetLastStatusMessage("Sedang membersihkan RAM & VRAM...");
    size_t beforeBytes = GetCurrentWorkingSetBytes();

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // 1. RELEASE D3D11 SHADER RESOURCE VIEWS SECARA THREAD-SAFE
        // Set pointer asli ke nullptr sebelum memanggil Release()
        for (auto ppView : m_trackedTextures) {
            if (ppView && *ppView) {
                ID3D11ShaderResourceView* pViewToRelease = *ppView;
                *ppView = nullptr;
                pViewToRelease->Release();
            }
        }
        m_trackedTextures.clear();

        // 2. FLUSH GPU COMMAND PIPELINE & VRAM ALLOCATIONS
        if (m_pd3dDeviceContext) {
            m_pd3dDeviceContext->ClearState();
            m_pd3dDeviceContext->Flush();
        }
    }

    // 3. WIN32 PHYSICAL RAM COMPACTION
    HANDLE hProcess = GetCurrentProcess();
    EmptyWorkingSet(hProcess);
    SetProcessWorkingSetSize(hProcess, (SIZE_T)-1, (SIZE_T)-1);

    size_t afterBytes = GetCurrentWorkingSetBytes();
    size_t reclaimed = (beforeBytes > afterBytes) ? (beforeBytes - afterBytes) : 0;
    float reclaimedMB = (float)reclaimed / (1024.0f * 1024.0f);
    float currentMB = (float)afterBytes / (1024.0f * 1024.0f);

    char buf[256];
    snprintf(buf, sizeof(buf), "Selesai! Berhasil membebaskan %.1f MB (RAM sekarang: %.1f MB).", reclaimedMB, currentMB);
    SetLastStatusMessage(buf);

    m_isPurging.store(false);
    return reclaimed;
}

void MemoryManager::PurgeAllAsync(std::function<void(size_t bytesReclaimed)> onComplete) {
    std::thread([this, onComplete]() {
        size_t freed = this->PurgeAll();
        if (onComplete) {
            onComplete(freed);
        }
    }).detach();
}
