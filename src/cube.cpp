#include "RTG.hpp"
#include "CubeIntegrator.hpp"

#include <iostream>
#include <string>
#include <stdexcept>

static void print_usage(const char *prog) {
    std::cerr << "Usage:\n"
              << "  " << prog << " <in.png> --lambertian <out.png>\n"
              << "  " << prog << " <in.png> --ggx <out_stem>\n"
              << "\n";
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
            .apiVersion = VK_API_VERSION_1_4,
        };
        configuration.headless = true;
        configuration.debug = true;
		configuration.physical_device_name = "NVIDIA GeForce RTX 5080 Laptop GPU";

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
