# MediaSDK 3.1.5 Coverage

Rujukan: [Insta360 Desktop Media SDK API](https://insta360develop.github.io/Insta360-Developer_Docs/en/x/desktop/media/), disemak 14 September 2026.

## Offline video dan imej

Semua API awam dalam `ins_stitcher.h` digunakan oleh `insta360_sdk_exporter`:

- initialization, version, model root, log level/path dan `GetMediaFileInfo`
- input/output, resolusi 2:1, bitrate, H.264/H.265 dan export 10-bit
- Template, Dynamic Stitch, Optical Flow dan AI Flow
- FlowState, direction lock dan export data stabilisasi
- CUDA, software encode/decode dan image processing Auto/CPU
- lens/accessory correction dan cooling-shell detection
- stitch fusion, denoise, defringe, deflicker dan ColorPlus strength
- exposure, highlights, shadows, contrast, brightness, black point, saturation,
  vibrance, warmth, tint dan definition
- JPEG/PNG image sequence dan pemilihan frame
- progress/error callbacks, polling progress, cancellation dan feature-status map

Kawalan global berada di `MediaSDK > Project & SDK Settings`. Kawalan per klip
berada pada tab inspector `Effects` dan `MediaSDK`.

## Real-time

Dialog `MediaSDK > Live Camera / Real-Time Stitching` menggunakan CameraSDK
2.1.8 untuk discovery/open/live stream. Video, gyro dan exposure diteruskan kepada
`RealTimeStitcher`; frame RGBA yang terhasil dipaparkan dalam Qt. UI menyediakan
output resolution, stitch type, accessory, video delay, bitrate, FlowState,
direction lock, defringe, deflicker dan software decode.

## Peraturan output

- Input MediaSDK sentiasa distitch pada nisbah 2:1 seperti diwajibkan manual.
- Projek Standard Video menukar equirectangular kepada rectilinear dengan FFmpeg
  `v360`, bukannya meregangkan panorama kepada 16:9.
- Export 10-bit mengekalkan pipeline 10-bit (`p010le` atau `yuv420p10le`) dan H.265.
- Status sebenar fitur yang boleh di-skip mengikut model kamera dicetak sebagai
  `feature.<name>=<status>` dalam log export.
