#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace s72 {

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
	glm::vec3 translation{0.0, 0.0, 0.0};
	glm::vec4 rotation{0.0, 0.0, 0.0, 1.0};
	glm::vec3 scale{1.0, 1.0, 1.0};
	std::vector<std::string> children;
	std::shared_ptr<Node> parent;
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
	std::shared_ptr<Node> parent;
};

struct Camera {
	struct Perspective {
		float aspect = 1.0;
		float vfov = 1.0;
		float near = 0.1;
		std::optional<float> far;
	};

	std::string name;
	std::optional<Perspective> perspective;
	std::shared_ptr<Node> parent;
};

struct Driver {
	std::string name;
	std::string node;
	std::string channel;
	std::vector<float> times;
	std::vector<float> values;
	std::string interpolation = "LINEAR";
};

struct Texture {
	std::string src;
	std::string type = "2D"; //"2D" or "cube"
	std::string format = "linear"; //"linear", "srgb", or "rgbe"
};

struct Material {
	struct PBR {
		std::optional<glm::vec3> albedo_value;
		std::optional<Texture> albedo_texture;
		std::optional<float> roughness_value;
		std::optional<Texture> roughness_texture;
		std::optional<float> metalness_value;
		std::optional<Texture> metalness_texture;
	};

	struct Lambertian {
		std::optional<glm::vec3> albedo_value;
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
	std::shared_ptr<Node> parent;
};

struct Light {
	struct Sun {
		float angle = 0.0;
		float strength = 0.0;
	};

	struct Sphere {
		float radius = 0.0;
		float power = 0.0;
		std::optional<float> limit;
	};

	struct Spot {
		float radius = 0.0;
		float power = 0.0;
		std::optional<float> limit;
		float fov = 0.0;
		float blend = 0.0;
	};

	std::string name;
	glm::vec3 tint{1.0, 1.0, 1.0};
	uint32_t shadow = 0;
	std::optional<Sun> sun;
	std::optional<Sphere> sphere;
	std::optional<Spot> spot;
	std::shared_ptr<Node> parent;
};

struct Document {
	Scene scene;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Camera>> cameras;
	std::vector<std::shared_ptr<Driver>> drivers;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<Environment>> environments;
	std::vector<std::shared_ptr<Light>> lights;
};

std::shared_ptr<Document> load_file(const std::string &path);
std::shared_ptr<Document> load_string(const std::string &contents);

// Load binary mesh data from b72 files according to mesh attributes
// Returns interleaved vertex data as a vector of bytes
std::vector<uint8_t> load_mesh_data(const std::string &base_path, const Mesh &mesh);

} // namespace s72
