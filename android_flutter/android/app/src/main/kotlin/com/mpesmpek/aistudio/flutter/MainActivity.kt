package com.mpesmpek.aistudio.flutter

import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import io.flutter.embedding.android.FlutterActivity
import java.io.File

class MainActivity : FlutterActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                val intent = Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)
                startActivity(intent)
            } else {
                initAIStudioFolders()
            }
        } else {
            initAIStudioFolders()
        }
    }

    override fun onResume() {
        super.onResume()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager()) {
            initAIStudioFolders()
        }
    }

    private fun initAIStudioFolders() {
        try {
            val docsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
            val baseDir = File(docsDir, "AIStudio")
            val subfolders = listOf(
                "models/Teks_LLM",
                "models/Gambar_Diffusion",
                "models/VAE",
                "models/LoRA",
                "models/ControlNet",
                "models/Audio_Suara",
                "outputs/foto"
            )
            for (sub in subfolders) {
                val dir = File(baseDir, sub)
                if (!dir.exists()) {
                    dir.mkdirs()
                }
            }
        } catch (_: Exception) {
        }
    }
}
