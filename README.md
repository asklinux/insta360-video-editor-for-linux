# Insta360 Studio Editor

Desktop video editor C++/Qt untuk media Insta360. Aplikasi ini dibuat di atas rujukan repo rasmi `Insta360Develop/Desktop-MediaSDK-Cpp` dan diintegrasikan dengan Linux MediaSDK 3.1.5 daripada `Linux_CameraSDK-2.1.8_MediaSDK-3.1.5.zip`.

Aplikasi tetap boleh dibina tanpa SDK; export `.mp4/.mov/.jpg` akan menggunakan FFmpeg. Selepas setup SDK, `.insv/.insp/.lrv` akan distitch melalui helper MediaSDK sebenar, termasuk preview fail mentah dan model AI yang dibekalkan.

Untuk release sebenar, build dengan SDK rasmi supaya semua runtime library Insta360 dibundle bersama aplikasi. GPU/CUDA untuk fail Insta360 berjalan melalui MediaSDK tersebut.

## Fungsi Utama

- Start screen untuk `Create Project` atau `Open Project`.
- Dialog projek dengan pilihan:
  - format `360 Panorama / Equirectangular` atau `Standard Video`
  - resolusi video, FPS, codec H.264/H.265 dan bitrate
  - stitch type `optflow`, `dynamicstitch`, `template`, `aistitch`
  - FlowState, direction lock, CUDA, software encode/decode
  - lokasi fail projek `.i360proj`
- Import media tempatan atau baca terus senarai fail kamera melalui CameraSDK.
- Senarai kamera dipaparkan dalam Media Library di sebelah kiri tanpa memuat turun fail.
- Fail kamera hanya dimuat turun apabila pengguna double-click atau menambahnya ke timeline; pasangan `_00_`/`_10_` turut diambil secara automatik.
- Dark professional editor UI dengan ikon native bertema pada fungsi utama, menu, media dan timeline.
- Video Preview di tengah menggunakan FFmpeg frame extraction.
- Inspector di sebelah kanan dengan tabs `Video`, `360`, `Color`, `Audio`, `Effects`.
- Timeline di bahagian bawah dengan double click atau drag/drop dari media bin.
- Export timeline ke MP4.
- Projek 360 memasukkan metadata spherical/equirectangular ke output MP4.
- Semua tetapan offline MediaSDK 3.1.5 tersedia melalui Project Settings serta tab Effects/MediaSDK per klip.
- Metadata fail mentah melalui `GetMediaFileInfo` dan paparan status versi SDK.
- Export JPEG/PNG frame sequence dengan pemilihan indeks frame.
- Export 10-bit H.265, data stabilisasi, SDK logging dan cancellation.
- Live Camera menggunakan CameraSDK 2.1.8 + `RealTimeStitcher` dengan paparan RGBA Qt.
- GPU mode:
  - media Insta360: guna CUDA/GPU melalui Insta360 MediaSDK helper
  - media biasa: guna FFmpeg `h264_nvenc` / `hevc_nvenc` jika tersedia

## Build

Dependencies:

- CMake 3.16+
- C++17 compiler
- Qt5 Widgets dan Qt5 Concurrent
- FFmpeg di `PATH`

```bash
./tools/setup_local_sdk.sh /path/to/Linux_CameraSDK-2.1.8_MediaSDK-3.1.5.zip
cmake -S . -B build
cmake --build build -j
./build/insta360_editor
```

Dalam workspace semasa, skrip tersebut secara automatik mencari ZIP di `../system-360-studio/Linux_CameraSDK-2.1.8_MediaSDK-3.1.5.zip`, jadi argumen path boleh ditinggalkan. Ia hanya mengekstrak MediaSDK x86_64 yang diperlukan; pakej CameraSDK ARM dalam ZIP tidak diekstrak. Folder SDK tempatan berada di bawah `vendor/` dan diabaikan oleh Git.

## Build Dengan Insta360 MediaSDK Dan Bundle Semua Lib

Setup tempatan di atas dikesan secara automatik oleh CMake. Untuk SDK yang dipasang di lokasi lain, pastikan folder include mempunyai `ins_stitcher.h` dan set `INSTA360_MEDIASDK_ROOT`; CMake akan cuba guna `${root}/include` serta `${root}/lib` atau `${root}/bin`.

```bash
cmake -S . -B build \
  -DINSTA360_MEDIASDK_ROOT=/path/to/official/Insta360MediaSDK
cmake --build build -j
```

Jika struktur SDK berbeza, set include/library secara manual:

```bash
cmake -S . -B build \
  -DINSTA360_MEDIASDK_INCLUDE_DIR=/path/to/MediaSDK/include \
  -DINSTA360_MEDIASDK_LIBRARY_DIR=/path/to/MediaSDK/lib \
  -DINSTA360_MEDIASDK_MODEL_DIR=/path/to/MediaSDK/models \
  -DINSTA360_MEDIASDK_LIBRARIES=/path/to/libMediaSDK.so
cmake --build build -j
```

Build pembangunan menggunakan RPATH ke library SDK yang diekstrak dan symlink `build/models`. Untuk pakej release berdikari, tambah `-DINSTA360_BUNDLE_MEDIASDK_RUNTIME=ON`; ini memerlukan beberapa GB ruang kerana dependency CUDA/TensorRT rasmi akan disalin.

Selepas build dengan SDK rasmi:

- `insta360_sdk_exporter` disalin ke folder yang sama dengan `insta360_editor`.
- `insta360_camera_tool` dibina untuk senarai jauh dan download atas permintaan melalui CameraSDK 2.1.8.
- Runtime library SDK dari `INSTA360_MEDIASDK_LIBRARY_DIR` disalin ke folder executable.
- RPATH diset kepada `$ORIGIN`, jadi aplikasi/helper akan mencari library yang dibundle dahulu.

Apabila helper ditemui, fail `.insv/.insp/.lrv` diproses melalui API MediaSDK:

- `VideoStitcher::SetInputPath`
- `VideoStitcher::SetOutputPath`
- `VideoStitcher::SetOutputSize`
- `VideoStitcher::SetStitchType`
- `VideoStitcher::EnableFlowState`
- `VideoStitcher::EnableDirectionLock`
- `VideoStitcher::EnableH265Encoder(true)`
- `SetModelFileRootDir` untuk AI Stitching/ColorPlus/model pemprosesan

Jika user pilih `CUDA/GPU`, aplikasi tidak menghantar `--disable_cuda`, jadi MediaSDK GPU path digunakan. Jika user pilih GPU untuk media Insta360 tetapi helper SDK tidak ditemui, export akan dihentikan dengan mesej ralat supaya proses tidak jatuh balik secara senyap kepada CPU.

Untuk bahan dual-file seperti `_00_` dan `_10_`, exporter akan mencari pasangan fail dalam folder yang sama.

## Media Kamera Tanpa Download Awal

Sambungkan kamera Insta360 melalui kaedah yang disokong CameraSDK, kemudian tekan
`Browse Camera (No Download)`. Aplikasi memanggil `GetCameraFilesList` dan meletakkan
entri berikon rangkaian dalam Media Library sebelah kiri. Entri berlabel
`Pada kamera` menyimpan path jauh sahaja; kandungan fail belum disalin ke projek.

Pilih `Add To Timeline`, double-click, atau drag entri ke timeline untuk memanggil
`DownloadCameraFile`. Progress dipaparkan dalam UI dan fail disimpan ke folder
`media/` projek. Membatalkan operasi akan memadam fail separa. Pembukaan item untuk
preview sahaja tidak menyebabkan download.

## Inspector Tabs

Pilih klip dalam timeline untuk mengedit inspector. Nilai disimpan dalam `.i360proj` dan digunakan semasa export.

- `Video`: trim in/out, position X/Y, scale dan rotation.
- `360`: projection dropdown, interactive preview mode, yaw/pitch/roll sliders dan toggle metadata 360.
- `Color`: preset dropdown, brightness, contrast, saturation dan temperature sliders.
- `Audio`: preset dropdown, enable/mute audio, volume slider, fade in dan fade out.
- `Effects`: FlowState/stabilize, denoise, defringe, deflicker, stitch fusion,
  cooling-shell detection dan ColorPlus dengan strength 0–100%.
- `MediaSDK`: exposure, highlights, shadows, contrast, brightness, black point,
  saturation, vibrance, warmth, tint dan definition mengikut julat API rasmi.

Menu `MediaSDK` menyediakan tetapan projek/SDK, semakan versi dan model, metadata
fail mentah serta Live Camera/Real-Time Stitching. Menu `Export` turut menyediakan
JPEG/PNG frame sequence. Nilai status fitur selepas export ialah `0=off`, `1=on`,
`2=skipped (auto)` dan `3=failed`.

Untuk preview 360 interaktif: pilih klip 360 dalam timeline, guna dropdown atas `Video Preview` dan pilih `Interactive 360 Mouse View`, kemudian drag mouse pada preview untuk pusing pandangan. Drag akan mengubah nilai yaw/pitch klip. Butang `Reset 360 View` akan set yaw/pitch/roll semula ke 0. Mouse-look 360 dirender dalam Qt daripada frame equirectangular yang dicache, jadi drag tidak memanggil FFmpeg berulang kali.

Playback preview menggunakan satu proses FFmpeg berterusan dengan `image2pipe` MJPEG. Ini mengelakkan UI freeze yang berlaku jika aplikasi spawn FFmpeg untuk setiap frame.

Untuk MP4 360 yang sudah equirectangular, preview panorama menggunakan FFmpeg `v360`. Untuk fail Insta360 mentah seperti `.insv/.insp/.lrv`, aplikasi menggunakan helper `insta360_sdk_exporter` dan MediaSDK rasmi supaya frame boleh distitch ke JPEG equirectangular sebelum dipaparkan sebagai panorama.

## Semak GPU FFmpeg

Untuk fallback media biasa, GPU memerlukan FFmpeg dengan NVENC:

```bash
ffmpeg -hide_banner -encoders | grep -E 'h264_nvenc|hevc_nvenc'
```

Jika encoder itu tiada, aplikasi akan log fallback kepada CPU `libx264` atau `libx265`.

## Nota 360 Metadata

Untuk projek `360 Panorama`, output akhir disalin dengan XMP spherical metadata:

- `GSpherical:Spherical=true`
- `GSpherical:Stitched=true`
- `GSpherical:ProjectionType=equirectangular`

Ini memastikan fail MP4 membawa metadata 360 panorama. Untuk workflow penerbitan yang sangat ketat, sahkan output akhir dengan pemain/platform sasaran.

## Struktur

- `src/` - aplikasi Qt utama
- `tools/insta360_sdk_exporter.cpp` - helper CLI pilihan untuk MediaSDK sebenar
- `tools/insta360_camera_tool.cpp` - senarai fail kamera dan download atas permintaan
- `vendor/Desktop-MediaSDK-Cpp/` - repo rujukan rasmi pilihan (tidak dimasukkan dalam Git)
- `MEDIASDK_COVERAGE.md` - pemetaan API manual kepada fungsi aplikasi
