#pragma once
#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <thread>

// Struktur Model yang Terdeteksi
struct DetectedModel {
    std::string display_name;
    std::string filename;
    std::string full_path;
    std::string desc_plus;
    std::string desc_minus;
    std::string rec;
    bool is_custom;
    int size_mb;
};

// Kategori Model
enum ModelCategory {
    MODEL_CAT_UNKNOWN = 0,
    MODEL_CAT_TEXT_LLM,
    MODEL_CAT_IMAGE_DIFFUSION,
    MODEL_CAT_VISION_MULTIMODAL,
    MODEL_CAT_VAE,
    MODEL_CAT_LORA
};

// Hasil Inspeksi File Model Baru
struct ModelInspectionResult {
    bool is_valid_file = false;
    bool is_gguf = false;
    bool is_safetensors = false;
    std::string filepath = "";
    std::string filename = "";
    int file_size_mb = 0;

    ModelCategory category = MODEL_CAT_UNKNOWN;
    std::string category_name = "";
    std::string target_folder = "";

    bool is_critical_error = false;
    std::string critical_reason = "";
    std::vector<std::string> dependencies_status;
    std::vector<std::string> consequences;
    std::string recommendation = "";
};

// Daftar Model Global
extern std::vector<DetectedModel> g_detected_llms;
extern std::vector<DetectedModel> g_detected_photos;
extern std::vector<std::string> g_detected_llm_labels;
extern std::vector<std::string> g_detected_photo_labels;
extern std::vector<const char*> g_llm_items;
extern std::vector<const char*> g_photo_items;

// State Modal Import & Hapus
extern bool g_show_import_modal;
extern bool g_is_importing;
extern float g_import_progress;
extern std::string g_import_status_text;
extern ModelInspectionResult g_current_inspection;
extern std::thread g_import_thread;

extern bool g_show_delete_confirm_modal;
extern std::string g_delete_target_path;
extern std::string g_delete_target_name;

// Fungsi-Fungsi Pengelola Model
void ScanModelsDirectory();
ModelInspectionResult InspectModelFile(const std::string& filepath);
void StartImportModelWorker(const ModelInspectionResult& insp);
void DeleteModelFile(const std::string& path_to_del);

#endif // MODEL_MANAGER_H
