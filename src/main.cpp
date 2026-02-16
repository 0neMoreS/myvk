#include "RTG.hpp"

#include "BatchCache.hpp"
#include "Tutorial.hpp"
#include "A1.hpp"
#include "A2.hpp"
// #include "A3.hpp"

#include <iostream>

int main(int argc, char **argv) {
	//main wrapped in a try-catch so we can print some debug info about uncaught exceptions:
	try {
		//configure application:
		RTG::Configuration configuration;

		configuration.application_info = VkApplicationInfo{
			.pApplicationName = "nakluV Tutorial",
			.applicationVersion = VK_MAKE_VERSION(0,0,0),
			.pEngineName = "Unknown",
			.engineVersion = VK_MAKE_VERSION(0,0,0),
			.apiVersion = VK_API_VERSION_1_4
		};

		// configuration.workspaces = 1;

		bool print_usage = false;

		try {
			configuration.parse(argc, argv);
		} catch (std::runtime_error &e) {
			std::cerr << "Failed to parse arguments:\n" << e.what() << std::endl;
			print_usage = true;
		}

		if (print_usage) {
			std::cerr << "Usage:" << std::endl;
			RTG::Configuration::usage( [](const char *arg, const char *desc){ 
				std::cerr << "    " << arg << "\n        " << desc << std::endl;
			});
			return 1;
		}

		//loads vulkan library, creates surface, initializes helpers:
		RTG rtg(configuration);

		std::cout << "Loaded scene '" << configuration.s72_filename << "path " << s72_dir << "'." << std::endl;

		//initializes global (whole-life-of-application) resources:
		// BatchCache application(rtg, configuration.index);
		A2 application(rtg, configuration.s72_filename);

		//main loop -- handles events, renders frames, etc:
		rtg.run(application);

	} catch (std::exception &e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
}
