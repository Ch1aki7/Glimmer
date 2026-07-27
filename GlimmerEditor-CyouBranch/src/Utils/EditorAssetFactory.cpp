#include "EditorAssetFactory.h"

#include "Glimmer/Core/Log.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace gl {

	namespace {

		std::string CreateCubeOBJ()
		{
			return R"(# Glimmer generated cube
o Cube
v -0.5 -0.5  0.5
v  0.5 -0.5  0.5
v  0.5  0.5  0.5
v -0.5  0.5  0.5
v  0.5 -0.5 -0.5
v -0.5 -0.5 -0.5
v -0.5  0.5 -0.5
v  0.5  0.5 -0.5
v  0.5 -0.5  0.5
v  0.5 -0.5 -0.5
v  0.5  0.5 -0.5
v  0.5  0.5  0.5
v -0.5 -0.5 -0.5
v -0.5 -0.5  0.5
v -0.5  0.5  0.5
v -0.5  0.5 -0.5
v -0.5  0.5  0.5
v  0.5  0.5  0.5
v  0.5  0.5 -0.5
v -0.5  0.5 -0.5
v -0.5 -0.5 -0.5
v  0.5 -0.5 -0.5
v  0.5 -0.5  0.5
v -0.5 -0.5  0.5
vt 0.0 0.0
vt 1.0 0.0
vt 1.0 1.0
vt 0.0 1.0
vn  0.0  0.0  1.0
vn  0.0  0.0 -1.0
vn  1.0  0.0  0.0
vn -1.0  0.0  0.0
vn  0.0  1.0  0.0
vn  0.0 -1.0  0.0
f 1/1/1 2/2/1 3/3/1
f 1/1/1 3/3/1 4/4/1
f 5/1/2 6/2/2 7/3/2
f 5/1/2 7/3/2 8/4/2
f 9/1/3 10/2/3 11/3/3
f 9/1/3 11/3/3 12/4/3
f 13/1/4 14/2/4 15/3/4
f 13/1/4 15/3/4 16/4/4
f 17/1/5 18/2/5 19/3/5
f 17/1/5 19/3/5 20/4/5
f 21/1/6 22/2/6 23/3/6
f 21/1/6 23/3/6 24/4/6
)";
		}

		std::string CreatePlaneOBJ()
		{
			return R"(# Glimmer generated plane
o Plane
v -0.5 0.0 -0.5
v  0.5 0.0 -0.5
v  0.5 0.0  0.5
v -0.5 0.0  0.5
vt 0.0 0.0
vt 1.0 0.0
vt 1.0 1.0
vt 0.0 1.0
vn 0.0 1.0 0.0
f 1/1/1 3/3/1 2/2/1
f 1/1/1 4/4/1 3/3/1
)";
		}

		std::string CreateUVSphereOBJ()
		{
			constexpr uint32_t segments = 32;
			constexpr uint32_t rings = 16;
			constexpr float pi = 3.14159265358979323846f;
			std::ostringstream output;
			output << "# Glimmer generated UV sphere\n";
			output << "o UVSphere\n";

			for (uint32_t ring = 0; ring <= rings; ++ring)
			{
				const float v = static_cast<float>(ring) / rings;
				const float phi = v * pi;
				const float y = std::cos(phi) * 0.5f;
				const float radius = std::sin(phi) * 0.5f;

				for (uint32_t segment = 0; segment <= segments; ++segment)
				{
					const float u = static_cast<float>(segment) / segments;
					const float theta = u * pi * 2.0f;
					const float x = radius * std::cos(theta);
					const float z = radius * std::sin(theta);
					output << "v " << x << ' ' << y << ' ' << z << '\n';
				}
			}

			for (uint32_t ring = 0; ring <= rings; ++ring)
			{
				const float v = 1.0f - static_cast<float>(ring) / rings;
				for (uint32_t segment = 0; segment <= segments; ++segment)
				{
					const float u = static_cast<float>(segment) / segments;
					output << "vt " << u << ' ' << v << '\n';
				}
			}

			for (uint32_t ring = 0; ring <= rings; ++ring)
			{
				const float v = static_cast<float>(ring) / rings;
				const float phi = v * pi;
				for (uint32_t segment = 0; segment <= segments; ++segment)
				{
					const float u = static_cast<float>(segment) / segments;
					const float theta = u * pi * 2.0f;
					output << "vn "
						<< std::sin(phi) * std::cos(theta) << ' '
						<< std::cos(phi) << ' '
						<< std::sin(phi) * std::sin(theta) << '\n';
				}
			}

			const uint32_t stride = segments + 1;
			for (uint32_t ring = 0; ring < rings; ++ring)
			{
				for (uint32_t segment = 0; segment < segments; ++segment)
				{
					const uint32_t a = ring * stride + segment + 1;
					const uint32_t b = a + stride;
					const uint32_t c = b + 1;
					const uint32_t d = a + 1;
					output << "f " << a << '/' << a << '/' << a << ' '
						<< b << '/' << b << '/' << b << ' '
						<< c << '/' << c << '/' << c << '\n';
					output << "f " << a << '/' << a << '/' << a << ' '
						<< c << '/' << c << '/' << c << ' '
						<< d << '/' << d << '/' << d << '\n';
				}
			}
			return output.str();
		}

	}

	std::filesystem::path EditorAssetFactory::GetUniquePath(
		const std::filesystem::path& directory,
		const std::string& baseName,
		const std::string& extension)
	{
		std::filesystem::path candidate = directory / (baseName + extension);
		for (uint32_t index = 1; std::filesystem::exists(candidate); ++index)
			candidate = directory
				/ (baseName + " (" + std::to_string(index) + ")" + extension);
		return candidate;
	}

	bool EditorAssetFactory::WriteTextFile(
		const std::filesystem::path& path,
		const std::string& contents)
	{
		std::ofstream stream(path, std::ios::binary);
		if (!stream)
		{
			GL_CORE_ERROR("Could not create editor asset: {0}", path.string());
			return false;
		}
		stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
		return stream.good();
	}

	std::filesystem::path EditorAssetFactory::CreateFolder(
		const std::filesystem::path& directory)
	{
		const auto path = GetUniquePath(directory, "New Folder", "");
		std::error_code error;
		if (!std::filesystem::create_directory(path, error) || error)
		{
			GL_CORE_ERROR("Could not create folder: {0}", path.string());
			return {};
		}
		return path;
	}

	std::filesystem::path EditorAssetFactory::CreateMaterial(
		const std::filesystem::path& directory)
	{
		const auto path = GetUniquePath(directory, "New Material", ".glmat");
		return WriteTextFile(path, R"(Material:
  Shader: 0
  BaseColor: [1.0, 1.0, 1.0, 1.0]
  BaseColorTexture: 0
  TilingFactor: 1.0
  Metallic: 0.0
  Roughness: 0.5
)") ? path : std::filesystem::path{};
	}

	std::filesystem::path EditorAssetFactory::CreateSkybox(
		const std::filesystem::path& directory)
	{
		const auto path = GetUniquePath(directory, "New Skybox", ".glsky");
		return WriteTextFile(path, R"(Cubemap:
  ColorSpace: SRGB
  Right: ""
  Left: ""
  Top: ""
  Bottom: ""
  Front: ""
  Back: ""
  MissingFaceColor: [20, 20, 20, 255]
)") ? path : std::filesystem::path{};
	}

	std::filesystem::path EditorAssetFactory::CreateScene(
		const std::filesystem::path& directory)
	{
		const auto path = GetUniquePath(directory, "New Scene", ".glimmer");
		return WriteTextFile(path, R"(Scene: Untitled
Version: 6
Entities: []
)") ? path : std::filesystem::path{};
	}

	std::filesystem::path EditorAssetFactory::CreateShader(
		const std::filesystem::path& directory)
	{
		const auto path = GetUniquePath(directory, "New Shader", ".glsl");
		return WriteTextFile(path, R"(#type vertex
#version 410 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

void main()
{
    gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 410 core

layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = vec4(1.0);
}
)") ? path : std::filesystem::path{};
	}

	std::filesystem::path EditorAssetFactory::CreateGeometry(
		const std::filesystem::path& directory,
		PrimitiveGeometry geometry)
	{
		std::string name;
		std::string contents;
		switch (geometry)
		{
		case PrimitiveGeometry::Cube:
			name = "Cube";
			contents = CreateCubeOBJ();
			break;
		case PrimitiveGeometry::UVSphere:
			name = "UV Sphere";
			contents = CreateUVSphereOBJ();
			break;
		case PrimitiveGeometry::Plane:
			name = "Plane";
			contents = CreatePlaneOBJ();
			break;
		}

		const auto path = GetUniquePath(directory, name, ".obj");
		return WriteTextFile(path, contents) ? path : std::filesystem::path{};
	}

}
