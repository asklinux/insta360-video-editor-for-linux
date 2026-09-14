#include <camera/camera.h>
#include <camera/device_discovery.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

struct Options {
    bool list = false;
    std::string downloadRemote;
    std::string downloadLocal;
};

bool parseArgs(int argc, char **argv, Options &options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--list") {
            options.list = true;
        } else if (argument == "--download" && i + 2 < argc) {
            options.downloadRemote = argv[++i];
            options.downloadLocal = argv[++i];
        } else if (argument == "--version") {
            std::cout << "CameraSDK " << ins_camera::GetSDKVersion() << std::endl;
            std::exit(0);
        }
    }
    if (!options.list && options.downloadRemote.empty()) {
        std::cerr << "Usage: insta360_camera_tool --list | --download <remote> <local> | --version" << std::endl;
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parseArgs(argc, argv, options)) return 2;

    ins_camera::SetLogLevel(ins_camera::LogLevel::ERR);
    ins_camera::DeviceDiscovery discovery;
    auto devices = discovery.GetAvailableDevices();
    if (devices.empty()) {
        std::cerr << "camera_error=Tiada kamera Insta360 ditemui." << std::endl;
        return 1;
    }

    const std::string name = devices.front().camera_name;
    const std::string serial = devices.front().serial_number;
    const std::string firmware = devices.front().fw_version;
    auto camera = std::make_shared<ins_camera::Camera>(devices.front().info);
    if (!camera->Open()) {
        discovery.FreeDeviceDescriptors(devices);
        std::cerr << "camera_error=Kamera ditemui tetapi gagal dibuka." << std::endl;
        return 1;
    }
    discovery.FreeDeviceDescriptors(devices);
    std::cout << "device=" << name << '|' << serial << '|' << firmware << std::endl;

    int result = 0;
    if (options.list) {
        for (const auto &path : camera->GetCameraFilesList()) {
            std::cout << "file=" << path << std::endl;
        }
    } else {
        const auto parent = std::filesystem::path(options.downloadLocal).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
        const bool downloaded = camera->DownloadCameraFile(options.downloadRemote, options.downloadLocal,
            [](int64_t current, int64_t total) {
                std::cout << "progress=" << current << '/' << total << std::endl;
            });
        if (!downloaded) {
            std::cerr << "camera_error=Gagal memuat turun fail kamera." << std::endl;
            result = 1;
        } else {
            std::cout << "downloaded=" << options.downloadLocal << std::endl;
        }
    }

    camera->Close();
    return result;
}
