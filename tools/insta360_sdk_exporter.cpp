#include <ins_stitcher.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct Options {
    std::vector<std::string> inputs;
    std::string output;
    std::string previewOutput;
    std::string imageSequenceDir;
    std::string imageType = "jpg";
    std::string stabOutput;
    std::string logLevel = "error";
    std::string logPath;
    std::string stitchType = "optflow";
    std::string modelRoot;
    int width = 3840;
    int height = 1920;
    int fps = 30;
    uint64_t frameIndex = 0;
    int64_t bitrate = 60 * 1000 * 1000;
    bool h265 = false;
    bool flowState = false;
    bool directionLock = false;
    bool cuda = true;
    bool softEncode = false;
    bool softDecode = false;
    bool tenBit = false;
    bool denoise = false;
    bool defringe = false;
    bool deflicker = false;
    bool colorPlus = false;
    bool stitchFusion = false;
    bool coolingShell = false;
    bool imageProcessingCpu = false;
    bool probe = false;
    bool version = false;
    float colorPlusStrength = 1.0f;
    int cameraAccessory = -1;
    int exposure = 0;
    int highlights = 0;
    int shadows = 0;
    int contrast = 0;
    int brightness = 0;
    int blackpoint = 0;
    int saturation = 0;
    int vibrance = 0;
    int warmth = 0;
    int tint = 0;
    int definition = 0;
    std::vector<uint64_t> frameIndices;
};

std::atomic_bool cancelRequested {false};

void requestCancel(int)
{
    cancelRequested.store(true);
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string suffix(const std::string &path)
{
    const auto pos = path.find_last_of('.');
    if (pos == std::string::npos) {
        return {};
    }
    return lower(path.substr(pos + 1));
}

std::string ensureTrailingSeparator(std::string path)
{
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path.push_back('/');
    }
    return path;
}

std::string defaultModelRoot(const char *executable)
{
    if (const char *environment = std::getenv("INSTA360_MEDIASDK_MODEL_DIR")) {
        if (*environment) {
            return ensureTrailingSeparator(environment);
        }
    }

    std::error_code ec;
    const auto besideExecutable = std::filesystem::absolute(executable, ec).parent_path() / "models";
    if (!ec && std::filesystem::is_directory(besideExecutable)) {
        return ensureTrailingSeparator(besideExecutable.string());
    }

#ifdef INSTA360_DEFAULT_MODEL_DIR
    if (std::filesystem::is_directory(INSTA360_DEFAULT_MODEL_DIR)) {
        return ensureTrailingSeparator(INSTA360_DEFAULT_MODEL_DIR);
    }
#endif
    return {};
}

ins::STITCH_TYPE stitchTypeFromString(const std::string &value)
{
    if (value == "template") {
        return ins::STITCH_TYPE::TEMPLATE;
    }
    if (value == "dynamicstitch") {
        return ins::STITCH_TYPE::DYNAMICSTITCH;
    }
    if (value == "aistitch") {
        return ins::STITCH_TYPE::AIFLOW;
    }
    return ins::STITCH_TYPE::OPTFLOW;
}

ins::InsLogLevel logLevelFromString(const std::string &value)
{
    const auto normalized = lower(value);
    if (normalized == "verbose" || normalized == "debug") return ins::InsLogLevel::VERBOSE;
    if (normalized == "info") return ins::InsLogLevel::INFO;
    if (normalized == "warning" || normalized == "warn") return ins::InsLogLevel::WARNING;
    if (normalized == "fatal") return ins::InsLogLevel::FATAL;
    return ins::InsLogLevel::ERR;
}

std::vector<uint64_t> parseFrameIndices(const std::string &text)
{
    std::vector<uint64_t> result;
    std::string token;
    for (const char c : text + ",") {
        if (c == ',' || c == '-' || std::isspace(static_cast<unsigned char>(c))) {
            if (!token.empty()) {
                result.push_back(static_cast<uint64_t>(std::strtoull(token.c_str(), nullptr, 10)));
                token.clear();
            }
        } else {
            token.push_back(c);
        }
    }
    return result;
}

template <typename Stitcher>
void applyCommonSettings(Stitcher &stitcher, const Options &options)
{
    stitcher.SetOutputSize(options.width, options.height);
    stitcher.SetStitchType(stitchTypeFromString(options.stitchType));
    stitcher.EnableFlowState(options.flowState);
    stitcher.EnableCuda(options.cuda);
    stitcher.EnableDenoise(options.denoise);
    stitcher.EnableColorPlus(options.colorPlus, options.colorPlusStrength);
    stitcher.EnableStitchFusion(options.stitchFusion);
    stitcher.EnableCoolingShellDetection(options.coolingShell);
    stitcher.SetCameraAccessoryType(static_cast<ins::CameraAccessoryType>(options.cameraAccessory));
    stitcher.SetImageProcessingAccelType(options.imageProcessingCpu
        ? ins::ImageProcessingAccel::kCPU : ins::ImageProcessingAccel::kAuto);
    stitcher.SetExposure(options.exposure);
    stitcher.SetHighlights(options.highlights);
    stitcher.SetShadows(options.shadows);
    stitcher.SetContrast(options.contrast);
    stitcher.SetBrightness(options.brightness);
    stitcher.SetBlackpoint(options.blackpoint);
    stitcher.SetSaturation(options.saturation);
    stitcher.SetVibrance(options.vibrance);
    stitcher.SetWarmth(options.warmth);
    stitcher.SetTint(options.tint);
    stitcher.SetDefinition(options.definition);
}

bool parseArgs(int argc, char **argv, Options *options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](const char *name) -> const char * {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << std::endl;
                std::exit(2);
            }
            return argv[++i];
        };

        if (arg == "--input") {
            options->inputs.emplace_back(requireValue("--input"));
        } else if (arg == "--output") {
            options->output = requireValue("--output");
        } else if (arg == "--preview_output") {
            options->previewOutput = requireValue("--preview_output");
        } else if (arg == "--image_sequence_dir") {
            options->imageSequenceDir = requireValue("--image_sequence_dir");
        } else if (arg == "--image_type") {
            options->imageType = lower(requireValue("--image_type"));
        } else if (arg == "--frames") {
            options->frameIndices = parseFrameIndices(requireValue("--frames"));
        } else if (arg == "--stab_output") {
            options->stabOutput = requireValue("--stab_output");
        } else if (arg == "--log_level") {
            options->logLevel = requireValue("--log_level");
        } else if (arg == "--log_path") {
            options->logPath = requireValue("--log_path");
        } else if (arg == "--frame_index") {
            options->frameIndex = static_cast<uint64_t>(std::atoll(requireValue("--frame_index")));
        } else if (arg == "--width") {
            options->width = std::atoi(requireValue("--width"));
        } else if (arg == "--height") {
            options->height = std::atoi(requireValue("--height"));
        } else if (arg == "--fps") {
            options->fps = std::atoi(requireValue("--fps"));
        } else if (arg == "--bitrate") {
            options->bitrate = std::atoll(requireValue("--bitrate"));
        } else if (arg == "--stitch_type") {
            options->stitchType = requireValue("--stitch_type");
        } else if (arg == "--model_root") {
            options->modelRoot = requireValue("--model_root");
        } else if (arg == "--ai_model") {
            // Compatibility with projects created before MediaSDK 3.1.5. The
            // new API accepts a model directory rather than one AI model file.
            options->modelRoot = std::filesystem::path(requireValue("--ai_model")).parent_path().string();
        } else if (arg == "--h265") {
            options->h265 = true;
        } else if (arg == "--flowstate") {
            options->flowState = true;
        } else if (arg == "--direction_lock") {
            options->directionLock = true;
        } else if (arg == "--disable_cuda") {
            options->cuda = false;
        } else if (arg == "--soft_encode") {
            options->softEncode = true;
        } else if (arg == "--soft_decode") {
            options->softDecode = true;
        } else if (arg == "--ten_bit") {
            options->tenBit = true;
        } else if (arg == "--denoise") {
            options->denoise = true;
        } else if (arg == "--defringe") {
            options->defringe = true;
        } else if (arg == "--deflicker") {
            options->deflicker = true;
        } else if (arg == "--color_plus") {
            options->colorPlus = true;
        } else if (arg == "--color_plus_strength") {
            options->colorPlusStrength = std::clamp(std::strtof(requireValue("--color_plus_strength"), nullptr), 0.0f, 1.0f);
        } else if (arg == "--stitch_fusion") {
            options->stitchFusion = true;
        } else if (arg == "--cooling_shell") {
            options->coolingShell = true;
        } else if (arg == "--image_processing_cpu") {
            options->imageProcessingCpu = true;
        } else if (arg == "--camera_accessory") {
            options->cameraAccessory = std::atoi(requireValue("--camera_accessory"));
        } else if (arg == "--exposure") {
            options->exposure = std::atoi(requireValue("--exposure"));
        } else if (arg == "--highlights") {
            options->highlights = std::atoi(requireValue("--highlights"));
        } else if (arg == "--shadows") {
            options->shadows = std::atoi(requireValue("--shadows"));
        } else if (arg == "--contrast") {
            options->contrast = std::atoi(requireValue("--contrast"));
        } else if (arg == "--brightness") {
            options->brightness = std::atoi(requireValue("--brightness"));
        } else if (arg == "--blackpoint") {
            options->blackpoint = std::atoi(requireValue("--blackpoint"));
        } else if (arg == "--saturation") {
            options->saturation = std::atoi(requireValue("--saturation"));
        } else if (arg == "--vibrance") {
            options->vibrance = std::atoi(requireValue("--vibrance"));
        } else if (arg == "--warmth") {
            options->warmth = std::atoi(requireValue("--warmth"));
        } else if (arg == "--tint") {
            options->tint = std::atoi(requireValue("--tint"));
        } else if (arg == "--definition") {
            options->definition = std::atoi(requireValue("--definition"));
        } else if (arg == "--probe") {
            options->probe = true;
        } else if (arg == "--version") {
            options->version = true;
        }
    }

    if (options->version) return true;
    if (options->inputs.empty() || (!options->probe && options->output.empty()
        && options->previewOutput.empty() && options->imageSequenceDir.empty())) {
        std::cerr << "Usage: insta360_sdk_exporter --input file.insv [--input paired.insv] --output out.mp4 "
                  << "or --preview_output frame.jpg "
                  << "--width 3840 --height 1920 [--stitch_type optflow|dynamicstitch|template|aistitch] "
                  << "[--model_root /path/to/models]"
                  << std::endl;
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parseArgs(argc, argv, &options)) {
        return 2;
    }

    if (options.version) {
        std::cout << "MediaSDK " << ins::GetVersion() << " (major=" << ins::GetVersionMajor() << ")" << std::endl;
        return 0;
    }

    ins::SetLogLevel(logLevelFromString(options.logLevel));
    if (!options.logPath.empty()) ins::SetLogPath(options.logPath);
    ins::InitEnv();
    if (options.modelRoot.empty()) {
        options.modelRoot = defaultModelRoot(argv[0]);
    }
    if (std::filesystem::is_regular_file(options.modelRoot)) {
        options.modelRoot = std::filesystem::path(options.modelRoot).parent_path().string();
    }
    if (!options.modelRoot.empty()) {
        ins::SetModelFileRootDir(ensureTrailingSeparator(options.modelRoot));
    }

    if (options.probe) {
        ins::MediaFileInfo info {};
        if (!ins::GetMediaFileInfo(options.inputs, info)) return 1;
        std::cout << "media_type=" << static_cast<int>(info.media_type) << '\n'
                  << "width=" << info.width << '\n' << "height=" << info.height << '\n'
                  << "fps=" << info.fps << '\n' << "bitrate=" << info.bitrate << '\n'
                  << "duration_ms=" << info.duration_ms << std::endl;
        return 0;
    }

    const std::string ext = suffix(options.inputs.front());
    if (ext == "insp" || ext == "jpg" || ext == "jpeg") {
        auto stitcher = std::make_shared<ins::ImageStitcher>();
        stitcher->SetInputPath(options.inputs);
        stitcher->SetOutputPath(options.previewOutput.empty() ? options.output : options.previewOutput);
        applyCommonSettings(*stitcher, options);
        return stitcher->Stitch() ? 0 : 1;
    }

    std::mutex mutex;
    std::condition_variable condition;
    bool finished = false;
    bool failed = false;
    int lastProgress = -1;

    auto stitcher = std::make_shared<ins::VideoStitcher>();
    stitcher->SetInputPath(options.inputs);
    if (!options.imageSequenceDir.empty()) {
        std::filesystem::create_directories(options.imageSequenceDir);
        if (!options.frameIndices.empty()) stitcher->SetExportFrameSequence(options.frameIndices);
        stitcher->SetImageSequenceInfo(options.imageSequenceDir,
            options.imageType == "png" ? ins::IMAGE_TYPE::PNG : ins::IMAGE_TYPE::JPEG);
    } else if (options.previewOutput.empty()) {
        stitcher->SetOutputPath(options.output);
    } else {
        const std::filesystem::path previewPath(options.previewOutput);
        const std::filesystem::path previewDir = previewPath.parent_path();
        std::filesystem::create_directories(previewDir);
        stitcher->SetExportFrameSequence({options.frameIndex});
        stitcher->SetImageSequenceInfo(previewDir.string(), ins::IMAGE_TYPE::JPEG);
    }
    applyCommonSettings(*stitcher, options);
    stitcher->SetOutputBitRate(options.bitrate);
    stitcher->EnableDirectionLock(options.directionLock);
    stitcher->SetSoftwareCodecUsage(options.softEncode, options.softDecode);
    stitcher->EnableH265Encoder(options.h265);
    stitcher->Enable10BitExport(options.tenBit);
    stitcher->EnableDefringe(options.defringe);
    stitcher->EnableDeflicker(options.deflicker);
    if (!options.stabOutput.empty()) stitcher->SetStabDataOutputPath(options.stabOutput);

    stitcher->SetStitchProgressCallback([&](int progress, int error) {
        (void)error;
        if (progress != lastProgress) {
            std::cout << "progress=" << progress << std::endl;
            lastProgress = progress;
        }
        if (progress >= 100) {
            std::lock_guard<std::mutex> lock(mutex);
            finished = true;
            condition.notify_one();
        }
    });

    stitcher->SetStitchStateCallback([&](int error, const char *info) {
        std::cerr << "sdk_error=" << error << " " << (info ? info : "") << std::endl;
        std::lock_guard<std::mutex> lock(mutex);
        failed = true;
        condition.notify_one();
    });

    std::signal(SIGINT, requestCancel);
    std::signal(SIGTERM, requestCancel);
    stitcher->StartStitch();

    std::unique_lock<std::mutex> lock(mutex);
    while (!finished && !failed && !cancelRequested.load()) {
        condition.wait_for(lock, std::chrono::milliseconds(200));
    }
    if (cancelRequested.load()) {
        lock.unlock();
        stitcher->CancelStitch();
        std::cerr << "sdk_cancelled=1" << std::endl;
        return 130;
    }

    if (failed) {
        return 1;
    }

    std::cout << "progress_final=" << stitcher->GetStitchProgress() << std::endl;
    for (const auto &[name, status] : stitcher->GetFeatureStatusMap()) {
        std::cout << "feature." << name << '=' << status << std::endl;
    }

    if (!options.previewOutput.empty()) {
        const std::filesystem::path previewPath(options.previewOutput);
        std::filesystem::path generated = previewPath.parent_path() / (std::to_string(options.frameIndex) + ".jpg");
        if (!std::filesystem::is_regular_file(generated)) {
            for (const auto &entry : std::filesystem::directory_iterator(previewPath.parent_path())) {
                const auto candidate = entry.path();
                const auto candidateSuffix = lower(candidate.extension().string());
                if (entry.is_regular_file() && candidate != previewPath
                    && (candidateSuffix == ".jpg" || candidateSuffix == ".jpeg")) {
                    generated = candidate;
                    break;
                }
            }
        }
        if (!std::filesystem::is_regular_file(generated)) {
            std::cerr << "preview_frame_missing=" << generated << std::endl;
            return 1;
        }
        std::error_code ec;
        if (generated != previewPath) {
            std::filesystem::copy_file(generated, previewPath, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                std::cerr << "preview_copy_error=" << ec.message() << std::endl;
                return 1;
            }
        }
    }

    return 0;
}
