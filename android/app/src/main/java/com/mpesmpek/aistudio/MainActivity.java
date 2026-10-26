package com.mpesmpek.aistudio;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Vibrator;
import android.view.View;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Toast;
import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class MainActivity extends AppCompatActivity {

    private WebView mWebView;
    private ValueCallback<Uri[]> mFilePathCallback;
    private static final int FILE_CHOOSER_RESULT_CODE = 1001;
    private static final int MODEL_IMPORT_RESULT_CODE = 1002;

    private File mAiStudioDir;

    @SuppressLint("SetJavaScriptEnabled")
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Otomatis membuat struktur folder AIStudio di penyimpanan HP saat aplikasi pertama dibuka
        initAIStudioFolders();

        // Immersive dark mode status bar
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }

        mWebView = new WebView(this);
        setContentView(mWebView);

        WebSettings settings = mWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);
        settings.setSupportZoom(false);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);

        // Hardware Acceleration
        mWebView.setLayerType(View.LAYER_TYPE_HARDWARE, null);

        mWebView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                if (url.startsWith("http://") || url.startsWith("https://") || url.startsWith("file:///")) {
                    view.loadUrl(url);
                    return true;
                }
                return false;
            }
        });

        mWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView webView, ValueCallback<Uri[]> filePathCallback,
                                             FileChooserParams fileChooserParams) {
                if (mFilePathCallback != null) {
                    mFilePathCallback.onReceiveValue(null);
                }
                mFilePathCallback = filePathCallback;

                Intent intent = fileChooserParams.createIntent();
                try {
                    startActivityForResult(intent, FILE_CHOOSER_RESULT_CODE);
                } catch (Exception e) {
                    mFilePathCallback = null;
                    return false;
                }
                return true;
            }
        });

        mWebView.addJavascriptInterface(new WebAppInterface(this), "AndroidNative");

        // Load local asset UI
        mWebView.loadUrl("file:///android_asset/index.html");

        // Handle Back button
        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                if (mWebView.canGoBack()) {
                    mWebView.goBack();
                } else {
                    setEnabled(false);
                    getOnBackPressedDispatcher().onBackPressed();
                }
            }
        });
    }

    /**
     * Otomatis membuat direktori 'AIStudio' dan subfolder model lengkap di penyimpanan internal HP
     */
    private void initAIStudioFolders() {
        try {
            // 1. Coba di folder Documents/AIStudio (agar langsung muncul di File Manager HP)
            File docsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS);
            File primaryDir = new File(docsDir, "AIStudio");

            // 2. Fallback root storage /storage/emulated/0/AIStudio
            File rootDir = new File(Environment.getExternalStorageDirectory(), "AIStudio");

            // 3. Fallback app-specific storage (pasti selalu sukses tanpa butuh izin khusus)
            File appFilesDir = new File(getExternalFilesDir(null), "AIStudio");

            File targetDir = primaryDir;
            if (!targetDir.exists() && !targetDir.mkdirs()) {
                if (rootDir.exists() || rootDir.mkdirs()) {
                    targetDir = rootDir;
                } else {
                    targetDir = appFilesDir;
                    targetDir.mkdirs();
                }
            }
            mAiStudioDir = targetDir;

            // Buat seluruh subfolder model AI
            String[] subfolders = {
                "models/Teks_LLM",
                "models/Gambar_Diffusion",
                "models/VAE",
                "models/LoRA",
                "models/ControlNet",
                "models/Audio_Suara",
                "outputs/foto",
                "outputs/suara",
                "outputs/chat"
            };

            for (String sub : subfolders) {
                File dir = new File(mAiStudioDir, sub);
                if (!dir.exists()) {
                    dir.mkdirs();
                }
            }

            // Tulis file panduan import model otomatis
            File guideFile = new File(mAiStudioDir, "PETUNJUK_IMPORT_MODEL.txt");
            if (!guideFile.exists()) {
                String guide = "========================================================================\n"
                             + "   PANDUAN IMPORT MODEL AI STUDIO MOBILE (ANDROID)\n"
                             + "========================================================================\n"
                             + "Folder ini dibuat otomatis saat aplikasi AI Studio diinstall di HP.\n"
                             + "Anda dapat memindahkan file model AI (.gguf / .safetensors) ke folder ini:\n\n"
                             + "1. models/Teks_LLM/\n"
                             + "   - Letakkan model percakapan & koding format .gguf\n"
                             + "   - Contoh: DeepSeek-R1-Distill-Qwen-1.5B, Qwen2.5-Coder\n\n"
                             + "2. models/Gambar_Diffusion/\n"
                             + "   - Letakkan checkpoint generasi foto format .safetensors atau .gguf\n"
                             + "   - Contoh: CyberRealistic_V5, Realistic_Vision, DreamShaper8\n\n"
                             + "3. models/VAE/\n"
                             + "   - Letakkan taesd.safetensors (Fast VAE CPU) atau vae-ft-mse\n\n"
                             + "4. models/LoRA/\n"
                             + "   - Letakkan add_detail.safetensors atau lcm_lora_sd15.safetensors\n\n"
                             + "5. models/ControlNet/\n"
                             + "   - Letakkan control_sd15_canny.safetensors untuk kunci pose\n\n"
                             + "6. outputs/\n"
                             + "   - Seluruh hasil foto, chat, dan suara akan otomatis tersimpan di sini.\n"
                             + "========================================================================\n";
                FileOutputStream fos = new FileOutputStream(guideFile);
                fos.write(guide.getBytes(StandardCharsets.UTF_8));
                fos.flush();
                fos.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void openModelImportPicker() {
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        String[] mimeTypes = {"application/octet-stream", "*/*"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        try {
            startActivityForResult(Intent.createChooser(intent, "Pilih Model (.gguf / .safetensors)"), MODEL_IMPORT_RESULT_CODE);
        } catch (Exception e) {
            Toast.makeText(this, "Tidak ada file manager yang mendukung picker", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == FILE_CHOOSER_RESULT_CODE) {
            if (mFilePathCallback == null) return;
            Uri[] results = null;
            if (resultCode == Activity.RESULT_OK && data != null) {
                String dataString = data.getDataString();
                if (dataString != null) {
                    results = new Uri[]{Uri.parse(dataString)};
                }
            }
            mFilePathCallback.onReceiveValue(results);
            mFilePathCallback = null;
        } else if (requestCode == MODEL_IMPORT_RESULT_CODE && resultCode == Activity.RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                copyModelToAIStudioFolder(uri);
            }
        }
    }

    private void copyModelToAIStudioFolder(Uri uri) {
        new Thread(() -> {
            try {
                String fileName = "imported_model_" + System.currentTimeMillis();
                String uriStr = uri.getLastPathSegment();
                if (uriStr != null) {
                    int lastSlash = uriStr.lastIndexOf('/');
                    fileName = (lastSlash >= 0) ? uriStr.substring(lastSlash + 1) : uriStr;
                }

                File targetSubdir;
                if (fileName.endsWith(".gguf")) {
                    targetSubdir = new File(mAiStudioDir, "models/Teks_LLM");
                } else if (fileName.contains("vae") || fileName.contains("taesd")) {
                    targetSubdir = new File(mAiStudioDir, "models/VAE");
                } else if (fileName.contains("lora")) {
                    targetSubdir = new File(mAiStudioDir, "models/LoRA");
                } else if (fileName.contains("control")) {
                    targetSubdir = new File(mAiStudioDir, "models/ControlNet");
                } else {
                    targetSubdir = new File(mAiStudioDir, "models/Gambar_Diffusion");
                }

                if (!targetSubdir.exists()) targetSubdir.mkdirs();
                File destFile = new File(targetSubdir, fileName);

                InputStream in = getContentResolver().openInputStream(uri);
                OutputStream out = new FileOutputStream(destFile);
                byte[] buf = new byte[8192];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
                in.close();
                out.flush();
                out.close();

                runOnUiThread(() -> {
                    Toast.makeText(this, "Sukses mengimpor model: " + destFile.getName(), Toast.LENGTH_LONG).show();
                    mWebView.loadUrl("javascript:onModelImported('" + destFile.getName() + "');");
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    Toast.makeText(this, "Gagal mengimpor model: " + e.getMessage(), Toast.LENGTH_SHORT).show();
                });
            }
        }).start();
    }

    public class WebAppInterface {
        Activity mActivity;

        WebAppInterface(Activity activity) {
            mActivity = activity;
        }

        @JavascriptInterface
        public void vibrate(int milliseconds) {
            try {
                Vibrator v = (Vibrator) mActivity.getSystemService(Context.VIBRATOR_SERVICE);
                if (v != null) {
                    v.vibrate(milliseconds);
                }
            } catch (Exception ignored) {}
        }

        @JavascriptInterface
        public String getPlatformName() {
            return "Android " + Build.VERSION.RELEASE + " (SDK " + Build.VERSION.SDK_INT + ")";
        }

        @JavascriptInterface
        public String getAIStudioPath() {
            return (mAiStudioDir != null) ? mAiStudioDir.getAbsolutePath() : "Documents/AIStudio";
        }

        @JavascriptInterface
        public void triggerImportModel() {
            openModelImportPicker();
        }

        @JavascriptInterface
        public String scanLocalModels() {
            if (mAiStudioDir == null) return "[]";
            StringBuilder sb = new StringBuilder("[");
            File llmDir = new File(mAiStudioDir, "models/Teks_LLM");
            if (llmDir.exists()) {
                File[] files = llmDir.listFiles();
                if (files != null) {
                    for (File f : files) {
                        if (f.isFile() && f.getName().endsWith(".gguf")) {
                            if (sb.length() > 1) sb.append(",");
                            sb.append("\"").append(f.getName()).append("\"");
                        }
                    }
                }
            }
            sb.append("]");
            return sb.toString();
        }
    }
}
