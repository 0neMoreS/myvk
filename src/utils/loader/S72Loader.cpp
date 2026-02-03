#include "S72Loader.hpp"

#include "sejp.hpp"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <iostream>

namespace S72Loader {
namespace {

using Object = std::map< std::string, sejp::value >;

Object const expect_object(sejp::value const &v, std::string_view ctx) {
	auto obj = v.as_object();
	if (!obj) S72_ERROR(ctx, "expected object");
	return obj.value();
}

std::string expect_string(Object const &obj, std::string_view key, std::string_view ctx) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) S72_ERROR(ctx, std::string("missing '") + std::string(key) + "'");
	auto s = it->second.as_string();
	if (!s) S72_ERROR(ctx, std::string("'") + std::string(key) + "' must be string");
	return *s;
}

std::optional< std::string > optional_string(Object const &obj, std::string_view key) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) return std::nullopt;
	auto s = it->second.as_string();
	if (!s) S72_ERROR(key, "must be string");
	return *s;
}

float expect_number(Object const &obj, std::string_view key, std::string_view ctx) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) S72_ERROR(ctx, std::string("missing '") + std::string(key) + "'");
	auto n = it->second.as_number();
	if (!n) S72_ERROR(ctx, std::string("'") + std::string(key) + "' must be number");
	return static_cast< float >(*n);
}

std::optional< float > optional_number(Object const &obj, std::string_view key) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) return std::nullopt;
	auto n = it->second.as_number();
	if (!n) S72_ERROR(key, "must be number");
	return static_cast< float >(*n);
}

uint32_t to_u32(float value, std::string_view ctx) {
	float integral = 0.0;
	float frac = std::modf(value, &integral);
	if (frac != 0.0 || integral < 0.0 || integral > float(std::numeric_limits< uint32_t >::max())) {
		S72_ERROR(ctx, "expected unsigned 32-bit integer");
	}
	return static_cast< uint32_t >(integral);
}

template< size_t N >
glm::vec<N, float> parse_vec(sejp::value const &v, std::string_view ctx) {
	auto arr = v.as_array();
	if (!arr || arr->size() != N) S72_ERROR(ctx, "expected array of correct length");
	glm::vec<N, float, glm::defaultp> out{};
	for (size_t i = 0; i < N; ++i) {
		auto n = (*arr)[i].as_number();
		if (!n) S72_ERROR(ctx, "vector elements must be numbers");
		out[static_cast<typename glm::vec<N, float>::length_type>(i)] = static_cast< float >(*n);
	}
	return out;
}

template< size_t N >
glm::vec<N, float> vec_or_default(Object const &obj, std::string_view key, glm::vec<N, float> def, std::string_view ctx) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) return def;
	return parse_vec<N>(it->second, ctx);
}

std::vector< float > parse_number_array(sejp::value const &v, std::string_view ctx) {
	auto arr = v.as_array();
	if (!arr) S72_ERROR(ctx, "expected array");
	std::vector< float > out;
	out.reserve(arr->size());
	for (auto const &entry : *arr) {
		auto n = entry.as_number();
		if (!n) S72_ERROR(ctx, "array entries must be numbers");
		out.push_back(static_cast< float >(*n));
	}
	return out;
}

std::vector< std::string > parse_string_array(sejp::value const &v, std::string_view ctx) {
	auto arr = v.as_array();
	if (!arr) S72_ERROR(ctx, "expected array");
	std::vector< std::string > out;
	out.reserve(arr->size());
	for (auto const &entry : *arr) {
		auto s = entry.as_string();
		if (!s) S72_ERROR(ctx, "array entries must be strings");
		out.push_back(static_cast< std::string >(*s));
	}
	return out;
}

Texture parse_texture(sejp::value const &v, std::string_view ctx) {
	auto const &obj = expect_object(v, ctx);
	Texture t;
	t.src = expect_string(obj, "src", ctx);
	t.type = optional_string(obj, "type").value_or("2D");
	t.format = optional_string(obj, "format").value_or("linear");
	return t;
}

Material::PBR parse_pbr(sejp::value const &v, std::string_view ctx) {
	auto const &obj = expect_object(v, ctx);
	Material::PBR p;
	auto it = obj.find("albedo");
	if (it != obj.end()) {
		auto arr = it->second.as_array();
		if (arr) {
			p.albedo_value = parse_vec<3>(it->second, std::string(ctx) + ".albedo");
		} else {
			p.albedo_texture = parse_texture(it->second, std::string(ctx) + ".albedo");
		}
	}
	it = obj.find("roughness");
	if (it != obj.end()) {
		auto n = it->second.as_number();
		if (n) {
			p.roughness_value = static_cast<float>(*n);
		} else {
			p.roughness_texture = parse_texture(it->second, std::string(ctx) + ".roughness");
		}
	}
	it = obj.find("metalness");
	if (it != obj.end()) {
		auto n = it->second.as_number();
		if (n) {
			p.metalness_value = static_cast<float>(*n);
		} else {
			p.metalness_texture = parse_texture(it->second, std::string(ctx) + ".metalness");
		}
	}
	return p;
}

Material::Lambertian parse_lambertian(sejp::value const &v, std::string_view ctx) {
	auto const &obj = expect_object(v, ctx);
	Material::Lambertian m;
	auto it = obj.find("albedo");
	if (it != obj.end()) {
		auto arr = it->second.as_array();
		if (arr) {
			m.albedo_value = parse_vec<3>(it->second, std::string(ctx) + ".albedo");
		} else {
			m.albedo_texture = parse_texture(it->second, std::string(ctx) + ".albedo");
		}
	}
	return m;
}

DataStream parse_data_stream(sejp::value const &v, std::string_view ctx, bool allow_stride, bool require_stride) {
	auto const &obj = expect_object(v, ctx);
	DataStream ds;
	ds.src = expect_string(obj, "src", ctx);
	ds.offset = to_u32(expect_number(obj, "offset", ctx), ctx);
	auto stride_number = optional_number(obj, "stride");
	if (stride_number) {
		if (!allow_stride) S72_ERROR(ctx, "stride not allowed here");
		ds.stride = to_u32(*stride_number, ctx);
	} else if (require_stride) {
		S72_ERROR(ctx, "missing 'stride'");
	}
	ds.format = expect_string(obj, "format", ctx);
	return ds;
}

Scene parse_scene(Object const &obj) {
	Scene scene;
	scene.name = expect_string(obj, "name", "SCENE");
	auto it = obj.find("roots");
	if (it != obj.end()) {
		scene.roots = parse_string_array(it->second, "SCENE.roots");
	}
	return scene;
}

Node parse_node(Object const &obj) {
	Node node;
	node.name = expect_string(obj, "name", "NODE");
	node.translation = vec_or_default<3>(obj, "translation", {0.0, 0.0, 0.0}, "NODE.translation");
	node.rotation = vec_or_default<4>(obj, "rotation", {0.0, 0.0, 0.0, 1.0}, "NODE.rotation");
	node.scale = vec_or_default<3>(obj, "scale", {1.0, 1.0, 1.0}, "NODE.scale");
	if (auto it = obj.find("children"); it != obj.end()) {
		node.children = parse_string_array(it->second, "NODE.children");
	}
	node.mesh = optional_string(obj, "mesh");
	node.camera = optional_string(obj, "camera");
	node.environment = optional_string(obj, "environment");
	node.light = optional_string(obj, "light");
	node.aabb_min = {-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
	node.aabb_max = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
	node.model_matrix_is_dirty = true;
	node.world_aabb_is_dirty = true;
	return node;
}

Mesh parse_mesh(Object const &obj) {
	Mesh mesh;
	mesh.name = expect_string(obj, "name", "MESH");
	mesh.topology = expect_string(obj, "topology", "MESH");
	mesh.count = to_u32(expect_number(obj, "count", "MESH"), "MESH.count");
	if (auto it = obj.find("indices"); it != obj.end()) {
		mesh.indices = parse_data_stream(it->second, "MESH.indices", false, false);
	}
	auto it = obj.find("attributes");
	if (it == obj.end()) S72_ERROR("", "MESH: missing 'attributes'");
	auto const &attrs = expect_object(it->second, "MESH.attributes");
	for (auto const &entry : attrs) {
		mesh.attributes.emplace(entry.first, parse_data_stream(entry.second, "MESH.attributes." + entry.first, true, true));
	}
	if (mesh.attributes.empty()) S72_ERROR("", "MESH: attributes must not be empty");
	mesh.material = optional_string(obj, "material");
	return mesh;
}

Camera parse_camera(Object const &obj) {
	Camera cam;
	cam.name = expect_string(obj, "name", "CAMERA");
	if (auto it = obj.find("perspective"); it != obj.end()) {
		Camera::Perspective p;
		auto const &per = expect_object(it->second, "CAMERA.perspective");
		p.aspect = expect_number(per, "aspect", "CAMERA.perspective");
		p.vfov = expect_number(per, "vfov", "CAMERA.perspective");
		p.near = expect_number(per, "near", "CAMERA.perspective");
		p.far = optional_number(per, "far");
		cam.perspective = p;
	}
	if (!cam.perspective) S72_ERROR("", "CAMERA: must specify projection");
	return cam;
}

Driver parse_driver(Object const &obj) {
	Driver driver;
	driver.name = expect_string(obj, "name", "DRIVER");
	driver.node = expect_string(obj, "node", "DRIVER");
	driver.channel = expect_string(obj, "channel", "DRIVER");
	driver.interpolation = optional_string(obj, "interpolation").value_or("LINEAR");
	{
		auto it = obj.find("times");
		if (it == obj.end()) S72_ERROR("", "DRIVER: missing 'times'");
		driver.times = parse_number_array(it->second, "DRIVER.times");
	}
	{
		auto it = obj.find("values");
		if (it == obj.end()) S72_ERROR("", "DRIVER: missing 'values'");
		driver.values = parse_number_array(it->second, "DRIVER.values");
	}
	if (driver.channel == "translation" || driver.channel == "scale") {
		if (driver.values.size() != driver.times.size() * 3) S72_ERROR("", "DRIVER: channel expects 3D values");
	} else if (driver.channel == "rotation") {
		if (driver.values.size() != driver.times.size() * 4) S72_ERROR("", "DRIVER: rotation channel expects 4D values");
	}
	return driver;
}

Material parse_material(Object const &obj) {
	Material mat;
	mat.name = expect_string(obj, "name", "MATERIAL");
	if (auto it = obj.find("normalMap"); it != obj.end()) {
		mat.normal_map = parse_texture(it->second, "MATERIAL.normalMap");
	}
	if (auto it = obj.find("displacementMap"); it != obj.end()) {
		mat.displacement_map = parse_texture(it->second, "MATERIAL.displacementMap");
	}
	size_t shading_count = 0;
	if (auto it = obj.find("pbr"); it != obj.end()) {
		mat.pbr = parse_pbr(it->second, "MATERIAL.pbr");
		++shading_count;
	}
	if (auto it = obj.find("lambertian"); it != obj.end()) {
		mat.lambertian = parse_lambertian(it->second, "MATERIAL.lambertian");
		++shading_count;
	}
	if (auto it = obj.find("mirror"); it != obj.end()) {
		expect_object(it->second, "MATERIAL.mirror");
		mat.mirror = true;
		++shading_count;
	}
	if (auto it = obj.find("environment"); it != obj.end()) {
		expect_object(it->second, "MATERIAL.environment");
		mat.environment = true;
		++shading_count;
	}
	if (shading_count != 1) S72_ERROR("", "MATERIAL: exactly one shading model required");
	return mat;
}

Environment parse_environment(Object const &obj) {
	Environment env;
	env.name = expect_string(obj, "name", "ENVIRONMENT");
	auto it = obj.find("radiance");
	if (it == obj.end()) S72_ERROR("", "ENVIRONMENT: missing 'radiance'");
	env.radiance = parse_texture(it->second, "ENVIRONMENT.radiance");
	return env;
}

Light parse_light(Object const &obj) {
	Light light;
	light.name = expect_string(obj, "name", "LIGHT");
	light.tint = vec_or_default<3>(obj, "tint", {1.0, 1.0, 1.0}, "LIGHT.tint");
	if (auto n = optional_number(obj, "shadow")) {
		light.shadow = to_u32(*n, "LIGHT.shadow");
	}
	size_t kind = 0;
	if (auto it = obj.find("sun"); it != obj.end()) {
		auto const &sun_obj = expect_object(it->second, "LIGHT.sun");
		Light::Sun sun;
		sun.angle = expect_number(sun_obj, "angle", "LIGHT.sun");
		sun.strength = expect_number(sun_obj, "strength", "LIGHT.sun");
		light.sun = sun;
		++kind;
	}
	if (auto it = obj.find("sphere"); it != obj.end()) {
		auto const &sphere_obj = expect_object(it->second, "LIGHT.sphere");
		Light::Sphere sphere;
		sphere.radius = expect_number(sphere_obj, "radius", "LIGHT.sphere");
		sphere.power = expect_number(sphere_obj, "power", "LIGHT.sphere");
		sphere.limit = optional_number(sphere_obj, "limit");
		light.sphere = sphere;
		++kind;
	}
	if (auto it = obj.find("spot"); it != obj.end()) {
		auto const &spot_obj = expect_object(it->second, "LIGHT.spot");
		Light::Spot spot;
		spot.radius = expect_number(spot_obj, "radius", "LIGHT.spot");
		spot.power = expect_number(spot_obj, "power", "LIGHT.spot");
		spot.limit = optional_number(spot_obj, "limit");
		spot.fov = expect_number(spot_obj, "fov", "LIGHT.spot");
		spot.blend = expect_number(spot_obj, "blend", "LIGHT.spot");
		light.spot = spot;
		++kind;
	}
	if (kind != 1) S72_ERROR("", "LIGHT: exactly one light definition required");
	return light;
}

std::shared_ptr<Document> parse_document(sejp::value const &root) {
	// Parse Root
	auto arr_opt = root.as_array();
	if (!arr_opt || arr_opt->empty()) S72_ERROR("", "Root must be non-empty array");
	auto first = (*arr_opt)[0].as_string();
	if (!first || *first != "s72-v2") S72_ERROR("", "First entry must be 's72-v2'");

	auto doc = std::make_shared<Document>();

	bool scene_set = false;

	for (size_t i = 1; i < arr_opt->size(); ++i) {
		auto const &entry = (*arr_opt)[i];
		auto const &obj = expect_object(entry, "object");
		std::string type = expect_string(obj, "type", "object");
		if (type == "SCENE") {
			if (scene_set) S72_ERROR("", "Multiple SCENE objects not allowed");
			doc->scene = parse_scene(obj);
			scene_set = true;
		} else if (type == "NODE") {
			Node node = parse_node(obj);
			if (!node_map.emplace(node.name, doc->nodes.size()).second) {
				S72_ERROR("NODE", std::string("duplicate name '") + node.name + "'");
			}
			doc->nodes.push_back(std::move(node));
		} else if (type == "MESH") {
			auto mesh = parse_mesh(obj);
			if (!mesh_map.emplace(mesh.name, doc->meshes.size()).second) {
				S72_ERROR("MESH", std::string("duplicate name '") + mesh.name + "'");
			}
			doc->meshes.push_back(std::move(mesh));
		} else if (type == "CAMERA") {
			auto cam = parse_camera(obj);
			if (!camera_map.emplace(cam.name, doc->cameras.size()).second) {
				S72_ERROR("CAMERA", std::string("duplicate name '") + cam.name + "'");
			}
			doc->cameras.push_back(std::move(cam));
		} else if (type == "DRIVER") {
			auto driver = parse_driver(obj);
			if (!driver_map.emplace(driver.name, doc->drivers.size()).second) {
				S72_ERROR("DRIVER", std::string("duplicate name '") + driver.name + "'");
			}
			doc->drivers.push_back(std::move(driver));
		} else if (type == "MATERIAL") {
			auto mat = parse_material(obj);
			if (!material_map.emplace(mat.name, doc->materials.size()).second) {
				S72_ERROR("MATERIAL", std::string("duplicate name '") + mat.name + "'");
			}
			doc->materials.push_back(std::move(mat));
		} else if (type == "ENVIRONMENT") {
			auto env = parse_environment(obj);
			if (!environment_map.emplace(env.name, doc->environments.size()).second) {
				S72_ERROR("ENVIRONMENT", std::string("duplicate name '") + env.name + "'");
			}
			doc->environments.push_back(std::move(env));
		} else if (type == "LIGHT") {
			auto light = parse_light(obj);
			if (!light_map.emplace(light.name, doc->lights.size()).second) {
				S72_ERROR("LIGHT", std::string("duplicate name '") + light.name + "'");
			}
			doc->lights.push_back(std::move(light));
		} else {
			S72_ERROR("object", std::string("unknown type '") + type + "'");
		}
	}
	if (!scene_set) S72_ERROR("", "File must contain exactly one SCENE");
	
	return doc;
}

} // namespace S72Loader

std::shared_ptr<Document> load_file(std::string const &path) {
	auto root = sejp::load(path);
	return parse_document(root);
}

std::shared_ptr<Document> load_string(std::string const &contents) {
	auto root = sejp::parse(contents);
	return parse_document(root);
}

std::vector<uint8_t> load_mesh_data(const std::string &base_path, const std::string &src){    
    // Build full file path
    std::string filepath = base_path;
    if (!base_path.empty() && base_path.back() != '/') {
        filepath += '/';
    }
    filepath += src;
    
    // Open and read the file
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open data file: " + filepath);
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(file_size);
    file.read(reinterpret_cast<char*>(data.data()), file_size);
    if (!file) {
        throw std::runtime_error("Failed to read data file: " + filepath);
    }
    
    return data;
}

std::vector<uint8_t> load_mesh_data(const std::string &base_path, const Mesh &mesh) {
    // Determine which file to load
    std::string src;
    if (mesh.indices) {
        // If mesh has indices, load the indices data
        src = mesh.indices->src;
    } else if (!mesh.attributes.empty()) {
        // Otherwise load the first attribute's data source
        src = mesh.attributes.begin()->second.src;
    } else {
        throw std::runtime_error("Mesh has no data sources");
    }
	
    return load_mesh_data(base_path, src);
}
} // namespace S72Loader
