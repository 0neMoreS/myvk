#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace s72 {

using Vec2 = glm::fvec2;
using Vec3 = glm::fvec3;
using Vec4 = glm::fvec4;

struct DataStream {
	std::string src;
	uint32_t offset = 0;
	std::optional<uint32_t> stride;
	std::string format;
};

struct Scene {
	std::string name;
	std::vector<std::string> roots;
};

struct Node {
	std::string name;
	Vec3 translation{0.0, 0.0, 0.0};
	Vec4 rotation{0.0, 0.0, 0.0, 1.0};
	Vec3 scale{1.0, 1.0, 1.0};
	std::vector<std::string> children;
	std::optional<std::string> mesh;
	std::optional<std::string> camera;
	std::optional<std::string> environment;
	std::optional<std::string> light;
};

struct Mesh {
	std::string name;
	std::string topology;
	uint32_t count = 0;
	std::optional<DataStream> indices;
	std::map<std::string, DataStream> attributes;
	std::optional<std::string> material;
};

struct Camera {
	struct Perspective {
		double aspect = 1.0;
		double vfov = 1.0;
		double near = 0.1;
		std::optional<double> far;
	};

	std::string name;
	std::optional<Perspective> perspective;
};

struct Driver {
	std::string name;
	std::string node;
	std::string channel;
	std::vector<double> times;
	std::vector<double> values;
	std::string interpolation = "LINEAR";
};

struct Texture {
	std::string src;
	std::string type = "2D"; //"2D" or "cube"
	std::string format = "linear"; //"linear", "srgb", or "rgbe"
};

struct Material {
	struct PBR {
		std::optional<Vec3> albedo_value;
		std::optional<Texture> albedo_texture;
		std::optional<double> roughness_value;
		std::optional<Texture> roughness_texture;
		std::optional<double> metalness_value;
		std::optional<Texture> metalness_texture;
	};

	struct Lambertian {
		std::optional<Vec3> albedo_value;
		std::optional<Texture> albedo_texture;
	};

	std::string name;
	std::optional<Texture> normal_map;
	std::optional<Texture> displacement_map;
	std::optional<PBR> pbr;
	std::optional<Lambertian> lambertian;
	bool mirror = false;
	bool environment = false;
};

struct Environment {
	std::string name;
	Texture radiance;
};

struct Light {
	struct Sun {
		double angle = 0.0;
		double strength = 0.0;
	};

	struct Sphere {
		double radius = 0.0;
		double power = 0.0;
		std::optional<double> limit;
	};

	struct Spot {
		double radius = 0.0;
		double power = 0.0;
		std::optional<double> limit;
		double fov = 0.0;
		double blend = 0.0;
	};

	std::string name;
	Vec3 tint{1.0, 1.0, 1.0};
	uint32_t shadow = 0;
	std::optional<Sun> sun;
	std::optional<Sphere> sphere;
	std::optional<Spot> spot;
};

struct Document {
	Scene scene;
	std::vector<Node> nodes;
	std::vector<Mesh> meshes;
	std::vector<Camera> cameras;
	std::vector<Driver> drivers;
	std::vector<Material> materials;
	std::vector<Environment> environments;
	std::vector<Light> lights;
};

Document load_file(const std::string &path);
Document load_string(const std::string &contents);

} // namespace s72
