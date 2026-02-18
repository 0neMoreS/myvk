#include "RTG.hpp"
#include "CubeIntegrator.hpp"

#include <iostream>
#include <string>
#include <stdexcept>

static void print_usage(const char *prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <in.png> --lambertian <out.png>\n"
              << "  " << prog << " <in.png> --ggx <out_stem>\n"
              << "\n"
              << "Options:\n"
              << "  --lambertian <out.png>  Produce a 32x32 Lambertian irradiance cubemap\n"
              << "  --ggx <out_stem>        Produce 5 GGX-prefiltered mip levels:\n"
              << "                          out_stem.1.png (512x512) .. out_stem.5.png (32x32)\n"
              << "  --physical-device <n>   Select GPU by name\n"
              << "  --no-debug              Suppress Vulkan validation layers\n";
}

int main(int argc, char **argv) {
    try {
        // --- Parse our own arguments first ---
        std::string in_path;
        std::string mode;        // "lambertian" or "ggx"
        std::string out_path;    // output file or stem

        // Collect args for RTG separately
        std::vector<const char *> rtg_args;
        rtg_args.push_back(argv[0]);

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--lambertian") {
                if (i + 1 >= argc) { std::cerr << "--lambertian requires an argument\n"; return 1; }
                mode = "lambertian";
                out_path = argv[++i];
            } else if (arg == "--ggx") {
                if (i + 1 >= argc) { std::cerr << "--ggx requires an argument\n"; return 1; }
                mode = "ggx";
                out_path = argv[++i];
            } else if (arg == "--physical-device" || arg == "--no-debug" || arg == "--debug") {
                // Forward these to RTG
                rtg_args.push_back(argv[i]);
                if (arg == "--physical-device") {
                    if (i + 1 >= argc) { std::cerr << "--physical-device requires an argument\n"; return 1; }
                    rtg_args.push_back(argv[++i]);
                }
            } else if (arg.rfind("--", 0) == 0) {
                std::cerr << "Unknown option: " << arg << "\n";
                print_usage(argv[0]);
                return 1;
            } else {
                if (in_path.empty()) {
                    in_path = arg;
                } else {
                    std::cerr << "Unexpected positional argument: " << arg << "\n";
                    print_usage(argv[0]);
                    return 1;
                }
            }
        }

        if (in_path.empty() || mode.empty() || out_path.empty()) {
            print_usage(argv[0]);
            return 1;
        }

        // --- Set up headless Vulkan context ---
        RTG::Configuration configuration;
        configuration.application_info = VkApplicationInfo{
            .pApplicationName = "cube-preintegrator",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = "Unknown",
            .engineVersion = VK_MAKE_VERSION(0, 0, 0),
            .apiVersion = VK_API_VERSION_1_3,
        };
        configuration.headless = true;
        configuration.debug = false; // quieter by default for a CLI tool

        // Parse forwarded RTG flags (--physical-device, --debug/--no-debug)
        int rtg_argc = (int)rtg_args.size();
        try {
            configuration.parse(rtg_argc, const_cast<char **>(rtg_args.data()));
        } catch (std::exception &e) {
            std::cerr << "Failed to parse RTG arguments: " << e.what() << "\n";
            return 1;
        }

        RTG rtg(configuration);
        CubeIntegrator integrator(rtg);

        if (mode == "lambertian") {
            integrator.run_lambertian(in_path, out_path);
        } else if (mode == "ggx") {
            // out_path is treated as the stem (without .N.png suffix)
            // Strip trailing .png if user accidentally passed one
            std::string stem = out_path;
            if (stem.size() >= 4 && stem.substr(stem.size() - 4) == ".png") {
                stem = stem.substr(0, stem.size() - 4);
            }
            integrator.run_ggx(in_path, stem);
        }

        return 0;

    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
