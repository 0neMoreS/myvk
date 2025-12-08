#include "s72.hpp"

#include "sejp.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace s72 {
namespace {

using Object = std::map< std::string, sejp::value >;

[[noreturn]] void fail(std::string_view msg) {
	throw std::runtime_error(std::string(msg));
}

std::string describe(std::string_view ctx, std::string_view detail) {
	std::string out(ctx);
	if (!out.empty()) out.append(": ");
	out.append(detail);
	return out;
}

Object const expect_object(sejp::value const &v, std::string_view ctx) {
	auto obj = v.as_object();
	if (!obj) fail(describe(ctx, "expected object"));
	return obj.value();
}

// std::vector< sejp::value > const &expect_array(sejp::value const &v, std::string_view ctx) {
// 	auto arr = v.as_array();
// 	if (!arr) fail(describe(ctx, "expected array"));
// 	return *arr;
// }

std::string expect_string(Object const &obj, std::string_view key, std::string_view ctx) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) fail(describe(ctx, std::string("missing '") + std::string(key) + "'"));
	auto s = it->second.as_string();
	if (!s) fail(describe(ctx, std::string("'") + std::string(key) + "' must be string"));
	return *s;
}

std::optional< std::string > optional_string(Object const &obj, std::string_view key) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) return std::nullopt;
	auto s = it->second.as_string();
	if (!s) fail(describe(key, "must be string"));
	return *s;
}

double expect_number(Object const &obj, std::string_view key, std::string_view ctx) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) fail(describe(ctx, std::string("missing '") + std::string(key) + "'"));
	auto n = it->second.as_number();
	if (!n) fail(describe(ctx, std::string("'") + std::string(key) + "' must be number"));
	return *n;
}

std::optional< double > optional_number(Object const &obj, std::string_view key) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) return std::nullopt;
	auto n = it->second.as_number();
	if (!n) fail(describe(key, "must be number"));
	return *n;
}

uint32_t to_u32(double value, std::string_view ctx) {
	double integral = 0.0;
	double frac = std::modf(value, &integral);
	if (frac != 0.0 || integral < 0.0 || integral > double(std::numeric_limits< uint32_t >::max())) {
		fail(describe(ctx, "expected unsigned 32-bit integer"));
	}
	return static_cast< uint32_t >(integral);
}

template< size_t N >
glm::vec<N, float> parse_vec(sejp::value const &v, std::string_view ctx) {
	auto arr = v.as_array();
	if (!arr || arr->size() != N) fail(describe(ctx, "expected array of correct length"));
	glm::vec<N, float, glm::defaultp> out{};
	for (size_t i = 0; i < N; ++i) {
		auto n = (*arr)[i].as_number();
		if (!n) fail(describe(ctx, "vector elements must be numbers"));
		out[i] = *n;
	}
	return out;
}

template< size_t N >
glm::vec<N, float> vec_or_default(Object const &obj, std::string_view key, glm::vec<N, float> def, std::string_view ctx) {
	auto it = obj.find(std::string(key));
	if (it == obj.end()) return def;
	return parse_vec<N>(it->second, ctx);
}

std::vector< double > parse_number_array(sejp::value const &v, std::string_view ctx) {
	auto arr = v.as_array();
	if (!arr) fail(describe(ctx, "expected array"));
	std::vector< double > out;
	out.reserve(arr->size());
	for (auto const &entry : *arr) {
		auto n = entry.as_number();
		if (!n) fail(describe(ctx, "array entries must be numbers"));
		out.push_back(*n);
	}
	return out;
}

std::vector< std::string > parse_string_array(sejp::value const &v, std::string_view ctx) {
	auto arr = v.as_array();
	if (!arr) fail(describe(ctx, "expected array"));
	std::vector< std::string > out;
	out.reserve(arr->size());
	for (auto const &entry : *arr) {
		auto s = entry.as_string();
		if (!s) fail(describe(ctx, "array entries must be strings"));
		out.push_back(*s);
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
			p.albedo_value = parse_vec<3>(it->second, describe(ctx, "albedo"));
		} else {
			p.albedo_texture = parse_texture(it->second, describe(ctx, "albedo"));
		}
	}
	it = obj.find("roughness");
	if (it != obj.end()) {
		auto n = it->second.as_number();
		if (n) {
			p.roughness_value = *n;
		} else {
			p.roughness_texture = parse_texture(it->second, describe(ctx, "roughness"));
		}
	}
	it = obj.find("metalness");
	if (it != obj.end()) {
		auto n = it->second.as_number();
		if (n) {
			p.metalness_value = *n;
		} else {
			p.metalness_texture = parse_texture(it->second, describe(ctx, "metalness"));
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
			m.albedo_value = parse_vec<3>(it->second, describe(ctx, "albedo"));
		} else {
			m.albedo_texture = parse_texture(it->second, describe(ctx, "albedo"));
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
		if (!allow_stride) fail(describe(ctx, "stride not allowed here"));
		ds.stride = to_u32(*stride_number, ctx);
	} else if (require_stride) {
		fail(describe(ctx, "missing 'stride'"));
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
	if (it == obj.end()) fail("MESH: missing 'attributes'");
	auto const &attrs = expect_object(it->second, "MESH.attributes");
	for (auto const &entry : attrs) {
		mesh.attributes.emplace(entry.first, parse_data_stream(entry.second, describe("MESH.attributes", entry.first), true, true));
	}
	if (mesh.attributes.empty()) fail("MESH: attributes must not be empty");
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
	if (!cam.perspective) fail("CAMERA: must specify projection");
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
		if (it == obj.end()) fail("DRIVER: missing 'times'");
		driver.times = parse_number_array(it->second, "DRIVER.times");
	}
	{
		auto it = obj.find("values");
		if (it == obj.end()) fail("DRIVER: missing 'values'");
		driver.values = parse_number_array(it->second, "DRIVER.values");
	}
	if (driver.channel == "translation" || driver.channel == "scale") {
		if (driver.values.size() != driver.times.size() * 3) fail("DRIVER: channel expects 3D values");
	} else if (driver.channel == "rotation") {
		if (driver.values.size() != driver.times.size() * 4) fail("DRIVER: rotation channel expects 4D values");
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
	if (shading_count != 1) fail("MATERIAL: exactly one shading model required");
	return mat;
}

Environment parse_environment(Object const &obj) {
	Environment env;
	env.name = expect_string(obj, "name", "ENVIRONMENT");
	auto it = obj.find("radiance");
	if (it == obj.end()) fail("ENVIRONMENT: missing 'radiance'");
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
	if (kind != 1) fail("LIGHT: exactly one light definition required");
	return light;
}

void ensure_unique(std::unordered_set< std::string > &names, std::string const &name, std::string_view ctx) {
	if (!names.insert(name).second) fail(describe(ctx, std::string("duplicate name '") + name + "'"));
}

Document parse_document(sejp::value const &root) {
	auto arr_opt = root.as_array();
	if (!arr_opt || arr_opt->empty()) fail("Root must be non-empty array");
	auto first = (*arr_opt)[0].as_string();
	if (!first || *first != "s72-v2") fail("First entry must be 's72-v2'");

	Document doc;
	bool scene_set = false;
	std::unordered_set< std::string > node_names;
	std::unordered_set< std::string > mesh_names;
	std::unordered_set< std::string > camera_names;
	std::unordered_set< std::string > driver_names;
	std::unordered_set< std::string > material_names;
	std::unordered_set< std::string > environment_names;
	std::unordered_set< std::string > light_names;

	for (size_t i = 1; i < arr_opt->size(); ++i) {
		auto const &entry = (*arr_opt)[i];
		auto const &obj = expect_object(entry, "object");
		std::string type = expect_string(obj, "type", "object");
		if (type == "SCENE") {
			if (scene_set) fail("Multiple SCENE objects not allowed");
			doc.scene = parse_scene(obj);
			scene_set = true;
		} else if (type == "NODE") {
			Node n = parse_node(obj);
			ensure_unique(node_names, n.name, "NODE");
			doc.nodes.push_back(std::move(n));
		} else if (type == "MESH") {
			Mesh m = parse_mesh(obj);
			ensure_unique(mesh_names, m.name, "MESH");
			doc.meshes.push_back(std::move(m));
		} else if (type == "CAMERA") {
			Camera c = parse_camera(obj);
			ensure_unique(camera_names, c.name, "CAMERA");
			doc.cameras.push_back(std::move(c));
		} else if (type == "DRIVER") {
			Driver d = parse_driver(obj);
			ensure_unique(driver_names, d.name, "DRIVER");
			doc.drivers.push_back(std::move(d));
		} else if (type == "MATERIAL") {
			Material m = parse_material(obj);
			ensure_unique(material_names, m.name, "MATERIAL");
			doc.materials.push_back(std::move(m));
		} else if (type == "ENVIRONMENT") {
			Environment e = parse_environment(obj);
			ensure_unique(environment_names, e.name, "ENVIRONMENT");
			doc.environments.push_back(std::move(e));
		} else if (type == "LIGHT") {
			Light l = parse_light(obj);
			ensure_unique(light_names, l.name, "LIGHT");
			doc.lights.push_back(std::move(l));
		} else {
			fail(describe("object", std::string("unknown type '") + type + "'"));
		}
	}
	if (!scene_set) fail("File must contain exactly one SCENE");
	return doc;
}

} // namespace

Document load_file(std::string const &path) {
	auto root = sejp::load(path);
	return parse_document(root);
}

Document load_string(std::string const &contents) {
	auto root = sejp::parse(contents);
	return parse_document(root);
}

} // namespace s72
