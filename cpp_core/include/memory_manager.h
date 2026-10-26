#pragma once
#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <d3d11.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <string>

/**
 * @class MemoryManager
 * @brief Pengelola memori fisik (RAM) dan memori GPU (VRAM DirectX 11) terpusat.
 * 
 * Thread-safe: Operasi purge dapat dijalankan dari thread mana pun secara
 * sinkron maupun asinkron tanpa mengganggu pipeline render Direct3D 11.
 */
class MemoryManager {
public:
    // Mendapatkan instance singleton global
    static MemoryManager& Get();

    // Inisialisasi perangkat Direct3D 11
    void Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

    // Registrasi pointer tekstur yang aktif agar bisa dilacak & dibersihkan otomatis
    void RegisterTexture(ID3D11ShaderResourceView** ppTextureView);

    // Hapus registrasi pointer tekstur jika sudah dilepas secara manual
    void UnregisterTexture(ID3D11ShaderResourceView** ppTextureView);

    /**
     * @brief Membersihkan seluruh buffer memori, tekstur GPU, dan memadatkan RAM sistem.
     * @return Jumlah perkiraan memori fisik (dalam byte) yang berhasil direklamasi.
     */
    size_t PurgeAll();

    /**
     * @brief Menjalankan PurgeAll() secara asinkron di latar belakang (Non-Blocking).
     * @param onComplete Callback opsional yang dipanggil setelah pembersihan selesai.
     */
    void PurgeAllAsync(std::function<void(size_t bytesReclaimed)> onComplete = nullptr);

    // Telemetri penggunaan RAM proses saat ini (dalam byte)
    size_t GetCurrentWorkingSetBytes() const;

    // Telemetri puncak penggunaan RAM proses (dalam byte)
    size_t GetPeakWorkingSetBytes() const;

    // Status apakah proses purging sedang berlangsung
    bool IsPurging() const { return m_isPurging.load(); }

    // Pesan status terakhir pembersihan memori untuk ditampilkan di UI
    std::string GetLastStatusMessage() const;
    void SetLastStatusMessage(const std::string& msg);

private:
    MemoryManager();
    ~MemoryManager();

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    ID3D11Device*           m_pd3dDevice = nullptr;
    ID3D11DeviceContext*    m_pd3dDeviceContext = nullptr;

    mutable std::recursive_mutex m_mutex;
    std::vector<ID3D11ShaderResourceView**> m_trackedTextures;
    std::atomic<bool>       m_isPurging{false};
    std::string             m_lastStatusMsg = "RAM & VRAM Siap";
};

#endif // MEMORY_MANAGER_H
