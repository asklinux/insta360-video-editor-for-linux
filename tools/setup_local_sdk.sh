#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
default_zip="$(cd "${project_root}/.." && pwd)/system-360-studio/Linux_CameraSDK-2.1.8_MediaSDK-3.1.5.zip"
sdk_zip="${1:-${default_zip}}"
archive_member="Linux_CameraSDK-2.1.8_MediaSDK-3.1.5/MediaSDK-3.1.5-20260819-linux64.tar_1787191716602.gz"
camera_archive_member="Linux_CameraSDK-2.1.8_MediaSDK-3.1.5/CameraSDK-2.1.8-20260828_171805-linux-x86_64.tar_1787909155024.gz"
package_dir="${project_root}/vendor/insta360-mediasdk-package"
runtime_dir="${project_root}/vendor/insta360-mediasdk-runtime"
camera_dir="${project_root}/vendor/insta360-camerasdk"
deb_path="${package_dir}/MediaSDK-3.1.5-linux-amd64.deb"

if [[ ! -f "${sdk_zip}" ]]; then
    echo "SDK ZIP tidak ditemui: ${sdk_zip}" >&2
    echo "Penggunaan: $0 /path/to/Linux_CameraSDK-2.1.8_MediaSDK-3.1.5.zip" >&2
    exit 1
fi

mkdir -p "${package_dir}" "${runtime_dir}" "${camera_dir}"
unzip -p "${sdk_zip}" "${archive_member}" \
    | tar -xzf - -C "${package_dir}" --strip-components=1
dpkg-deb -x "${deb_path}" "${runtime_dir}"
rm -f "${deb_path}"
unzip -p "${sdk_zip}" "${camera_archive_member}" \
    | tar -xzf - -C "${camera_dir}" --strip-components=1

echo "MediaSDK 3.1.5 tersedia di ${runtime_dir}/opt/MediaSDK-3.1.5-linux"
echo "CameraSDK 2.1.8 tersedia di ${camera_dir}"
echo "Jalankan: cmake -S \"${project_root}\" -B \"${project_root}/build\" && cmake --build \"${project_root}/build\" -j"
