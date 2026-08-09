#include "glpch.h"
#include "SceneSerializer.h"
#include "Components.h"
#include "Entity.h"

#include <yaml-cpp/yaml.h>
#include <fstream>

namespace gl {

	// ============================================================
	// YAML 辅助函数 —— glm 类型序列化
	// ============================================================

	static void SerializeVec3(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
	}

	static void DeserializeVec3(const YAML::Node& node, glm::vec3& v)
	{
		if (node.IsSequence() && node.size() >= 3) {
			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();
		}
	}

	static void SerializeVec4(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
	}

	static void DeserializeVec4(const YAML::Node& node, glm::vec4& v)
	{
		if (node.IsSequence() && node.size() >= 4) {
			v.x = node[0].as<float>();
			v.y = node[1].as<float>();
			v.z = node[2].as<float>();
			v.w = node[3].as<float>();
		}
	}

	// ============================================================
	// 各组件序列化 / 反序列化
	// ============================================================

	static void SerializeComponent(YAML::Emitter& out, const TagComponent& comp)
	{
		out << YAML::Key << "TagComponent" << YAML::Value << comp.Tag;
	}

	static void DeserializeComponent(const YAML::Node& node, TagComponent& comp)
	{
		comp.Tag = node.as<std::string>();
	}

	static void SerializeComponent(YAML::Emitter& out, const TransformComponent& comp)
	{
		out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Translation" << YAML::Value; SerializeVec3(out, comp.Translation);
		out << YAML::Key << "Rotation"    << YAML::Value; SerializeVec3(out, comp.Rotation);
		out << YAML::Key << "Scale"       << YAML::Value; SerializeVec3(out, comp.Scale);
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, TransformComponent& comp)
	{
		if (node["Translation"]) DeserializeVec3(node["Translation"], comp.Translation);
		if (node["Rotation"])    DeserializeVec3(node["Rotation"], comp.Rotation);
		if (node["Scale"])       DeserializeVec3(node["Scale"], comp.Scale);
	}

	static void SerializeComponent(YAML::Emitter& out, const SpriteRendererComponent& comp)
	{
		out << YAML::Key << "SpriteRendererComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value; SerializeVec4(out, comp.Color);
		out << YAML::Key << "Texture" << YAML::Value
			<< static_cast<uint64_t>(comp.TextureHandle);
		out << YAML::Key << "TilingFactor" << YAML::Value << comp.TilingFactor;
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, SpriteRendererComponent& comp)
	{
		if (node["Color"]) DeserializeVec4(node["Color"], comp.Color);
		if (node["Texture"])
			comp.TextureHandle = AssetHandle(node["Texture"].as<uint64_t>());
		if (node["TilingFactor"])
			comp.TilingFactor = node["TilingFactor"].as<float>();
	}

	static void SerializeComponent(YAML::Emitter& out, const ModelRendererComponent& comp)
	{
		out << YAML::Key << "ModelRendererComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Model" << YAML::Value
			<< static_cast<uint64_t>(comp.ModelHandle);
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, ModelRendererComponent& comp)
	{
		if (node["Model"])
			comp.ModelHandle = AssetHandle(node["Model"].as<uint64_t>());
	}
	static void SerializeComponent(YAML::Emitter& out, const MaterialComponent& comp)
	{
		out << YAML::Key << "MaterialComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Material" << YAML::Value
			<< static_cast<uint64_t>(comp.MaterialHandle);
		if (!comp.Overrides.Empty())
		{
			const auto& values = comp.Overrides.Values;
			out << YAML::Key << "Overrides" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Mask" << YAML::Value << comp.Overrides.Mask;
			out << YAML::Key << "BaseColor" << YAML::Value;
			SerializeVec4(out, values.BaseColor);
			out << YAML::Key << "BaseColorTexture" << YAML::Value
				<< static_cast<uint64_t>(values.BaseColorTexture);
			out << YAML::Key << "TilingFactor" << YAML::Value << values.TilingFactor;
			out << YAML::Key << "Metallic" << YAML::Value << values.Metallic;
			out << YAML::Key << "Roughness" << YAML::Value << values.Roughness;
			out << YAML::Key << "AlphaMode" << YAML::Value
				<< MaterialAlphaModeToString(values.AlphaMode);
			out << YAML::Key << "AlphaCutoff" << YAML::Value << values.AlphaCutoff;
			out << YAML::EndMap;
		}
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, MaterialComponent& comp)
	{
		if (node["Material"])
			comp.MaterialHandle = AssetHandle(node["Material"].as<uint64_t>());

		const YAML::Node overrides = node["Overrides"];
		if (!overrides)
			return;

		if (overrides["Mask"])
			comp.Overrides.Mask = overrides["Mask"].as<uint32_t>();
		if (overrides["BaseColor"])
			DeserializeVec4(overrides["BaseColor"], comp.Overrides.Values.BaseColor);
		if (overrides["BaseColorTexture"])
			comp.Overrides.Values.BaseColorTexture =
				AssetHandle(overrides["BaseColorTexture"].as<uint64_t>());
		if (overrides["TilingFactor"])
			comp.Overrides.Values.TilingFactor = overrides["TilingFactor"].as<float>();
		if (overrides["Metallic"])
			comp.Overrides.Values.Metallic = overrides["Metallic"].as<float>();
		if (overrides["Roughness"])
			comp.Overrides.Values.Roughness = overrides["Roughness"].as<float>();
		if (overrides["AlphaMode"])
			comp.Overrides.Values.AlphaMode = MaterialAlphaModeFromString(
				overrides["AlphaMode"].as<std::string>());
		if (overrides["AlphaCutoff"])
			comp.Overrides.Values.AlphaCutoff = glm::clamp(
				overrides["AlphaCutoff"].as<float>(), 0.0f, 1.0f);
	}
	static void SerializeComponent(YAML::Emitter& out, const TerrainComponent& comp)
	{
		const auto& spec = comp.Specification;
		const auto& noise = spec.Noise;
		out << YAML::Key << "TerrainComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Procedural" << YAML::Value << spec.Procedural;
		out << YAML::Key << "HeightMapResolution" << YAML::Value << spec.HeightMapResolution;
		out << YAML::Key << "MeshResolution" << YAML::Value << spec.MeshResolution;
		out << YAML::Key << "HeightScale" << YAML::Value << spec.HeightScale;
		out << YAML::Key << "HeightMap" << YAML::Value << static_cast<uint64_t>(spec.HeightMapHandle);
		out << YAML::Key << "RenderShader" << YAML::Value << static_cast<uint64_t>(spec.RenderShaderHandle);
		out << YAML::Key << "GenerationShader" << YAML::Value << static_cast<uint64_t>(spec.GenerationShaderHandle);
		out << YAML::Key << "Noise" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Seed" << YAML::Value << noise.Seed;
		out << YAML::Key << "Octaves" << YAML::Value << noise.Octaves;
		out << YAML::Key << "Frequency" << YAML::Value << noise.Frequency;
		out << YAML::Key << "Lacunarity" << YAML::Value << noise.Lacunarity;
		out << YAML::Key << "Persistence" << YAML::Value << noise.Persistence;
		out << YAML::Key << "DomainWarp" << YAML::Value << noise.DomainWarp;
		out << YAML::Key << "RidgeStrength" << YAML::Value << noise.RidgeStrength;
		out << YAML::Key << "ContinentScale" << YAML::Value << noise.ContinentScale;
		out << YAML::Key << "ErosionStrength" << YAML::Value << noise.ErosionStrength;
		out << YAML::Key << "DetailStrength" << YAML::Value << noise.DetailStrength;
		out << YAML::Key << "Offset" << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< noise.Offset.x << noise.Offset.y << YAML::EndSeq;
		out << YAML::EndMap << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, TerrainComponent& comp)
	{
		auto& spec = comp.Specification;
		if (node["Procedural"]) spec.Procedural = node["Procedural"].as<bool>();
		if (node["HeightMapResolution"]) spec.HeightMapResolution = node["HeightMapResolution"].as<uint32_t>();
		if (node["MeshResolution"]) spec.MeshResolution = node["MeshResolution"].as<uint32_t>();
		if (node["HeightScale"]) spec.HeightScale = node["HeightScale"].as<float>();
		if (node["HeightMap"]) spec.HeightMapHandle = AssetHandle(node["HeightMap"].as<uint64_t>());
		if (node["RenderShader"]) spec.RenderShaderHandle = AssetHandle(node["RenderShader"].as<uint64_t>());
		if (node["GenerationShader"]) spec.GenerationShaderHandle = AssetHandle(node["GenerationShader"].as<uint64_t>());
		if (const auto noiseNode = node["Noise"])
		{
			auto& noise = spec.Noise;
			if (noiseNode["Seed"]) noise.Seed = noiseNode["Seed"].as<int>();
			if (noiseNode["Octaves"]) noise.Octaves = noiseNode["Octaves"].as<int>();
			if (noiseNode["Frequency"]) noise.Frequency = noiseNode["Frequency"].as<float>();
			if (noiseNode["Lacunarity"]) noise.Lacunarity = noiseNode["Lacunarity"].as<float>();
			if (noiseNode["Persistence"]) noise.Persistence = noiseNode["Persistence"].as<float>();
			if (noiseNode["DomainWarp"]) noise.DomainWarp = noiseNode["DomainWarp"].as<float>();
			if (noiseNode["RidgeStrength"]) noise.RidgeStrength = noiseNode["RidgeStrength"].as<float>();
			if (noiseNode["ContinentScale"]) noise.ContinentScale = noiseNode["ContinentScale"].as<float>();
			if (noiseNode["ErosionStrength"]) noise.ErosionStrength = noiseNode["ErosionStrength"].as<float>();
			if (noiseNode["DetailStrength"]) noise.DetailStrength = noiseNode["DetailStrength"].as<float>();
			if (const auto offset = noiseNode["Offset"]; offset && offset.size() >= 2)
				noise.Offset = { offset[0].as<float>(), offset[1].as<float>() };
		}
		comp.Runtime.reset();
	}
	static void SerializeComponent(YAML::Emitter& out, const DirectionalLightComponent& comp)
	{
		out << YAML::Key << "DirectionalLightComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value; SerializeVec3(out, comp.Color);
		out << YAML::Key << "Intensity" << YAML::Value << comp.Intensity;
		out << YAML::Key << "AmbientIntensity" << YAML::Value << comp.AmbientIntensity;
		out << YAML::Key << "Enabled" << YAML::Value << comp.Enabled;
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, DirectionalLightComponent& comp)
	{
		if (node["Color"]) DeserializeVec3(node["Color"], comp.Color);
		if (node["Intensity"]) comp.Intensity = node["Intensity"].as<float>();
		if (node["AmbientIntensity"]) comp.AmbientIntensity = node["AmbientIntensity"].as<float>();
		if (node["Enabled"]) comp.Enabled = node["Enabled"].as<bool>();
	}

	static void SerializeComponent(YAML::Emitter& out, const PointLightComponent& comp)
	{
		out << YAML::Key << "PointLightComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Color" << YAML::Value; SerializeVec3(out, comp.Color);
		out << YAML::Key << "Intensity" << YAML::Value << comp.Intensity;
		out << YAML::Key << "Range" << YAML::Value << comp.Range;
		out << YAML::Key << "Enabled" << YAML::Value << comp.Enabled;
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, PointLightComponent& comp)
	{
		if (node["Color"]) DeserializeVec3(node["Color"], comp.Color);
		if (node["Intensity"]) comp.Intensity = node["Intensity"].as<float>();
		if (node["Range"]) comp.Range = node["Range"].as<float>();
		if (node["Enabled"]) comp.Enabled = node["Enabled"].as<bool>();
	}
	static void SerializeComponent(YAML::Emitter& out, const SkyLightComponent& comp)
	{
		out << YAML::Key << "SkyLightComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Cubemap" << YAML::Value
			<< static_cast<uint64_t>(comp.CubemapHandle);
		out << YAML::Key << "Intensity" << YAML::Value << comp.Intensity;
		out << YAML::Key << "Enabled" << YAML::Value << comp.Enabled;
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, SkyLightComponent& comp)
	{
		if (node["Cubemap"])
			comp.CubemapHandle = AssetHandle(node["Cubemap"].as<uint64_t>());
		if (node["Intensity"])
			comp.Intensity = glm::max(node["Intensity"].as<float>(), 0.0f);
		if (node["Enabled"])
			comp.Enabled = node["Enabled"].as<bool>();
	}
	static void SerializeComponent(YAML::Emitter& out, const CameraComponent& comp)
	{
		out << YAML::Key << "CameraComponent" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Primary"          << YAML::Value << comp.Primary;
		out << YAML::Key << "FixedAspectRatio" << YAML::Value << comp.FixedAspectRatio;
		out << YAML::Key << "ProjectionType"   << YAML::Value << (int)comp.Camera.GetProjectionType();
		out << YAML::Key << "OrthoSize"        << YAML::Value << comp.Camera.GetOrthographicSize();
		out << YAML::Key << "OrthoNear"        << YAML::Value << comp.Camera.GetOrthographicNearClip();
		out << YAML::Key << "OrthoFar"         << YAML::Value << comp.Camera.GetOrthographicFarClip();
		out << YAML::Key << "PerspFOV"         << YAML::Value << comp.Camera.GetPerspectiveVerticalFOV();
		out << YAML::Key << "PerspNear"        << YAML::Value << comp.Camera.GetPerspectiveNearClip();
		out << YAML::Key << "PerspFar"         << YAML::Value << comp.Camera.GetPerspectiveFarClip();
		out << YAML::EndMap;
	}

	static void DeserializeComponent(const YAML::Node& node, CameraComponent& comp)
	{
		if (node["Primary"])          comp.Primary = node["Primary"].as<bool>();
		if (node["FixedAspectRatio"]) comp.FixedAspectRatio = node["FixedAspectRatio"].as<bool>();
		if (node["ProjectionType"])   comp.Camera.SetProjectionType((SceneCamera::ProjectionType)node["ProjectionType"].as<int>());
		if (node["OrthoSize"])        comp.Camera.SetOrthographicSize(node["OrthoSize"].as<float>());
		if (node["OrthoNear"])        comp.Camera.SetOrthographicNearClip(node["OrthoNear"].as<float>());
		if (node["OrthoFar"])         comp.Camera.SetOrthographicFarClip(node["OrthoFar"].as<float>());
		if (node["PerspFOV"])         comp.Camera.SetPerspectiveVerticalFOV(node["PerspFOV"].as<float>());
		if (node["PerspNear"])        comp.Camera.SetPerspectiveNearClip(node["PerspNear"].as<float>());
		if (node["PerspFar"])         comp.Camera.SetPerspectiveFarClip(node["PerspFar"].as<float>());
	}

	// NativeScriptComponent 暂不序列化（函数指针无法持久化）

	// ============================================================
	// 场景整体序列化 / 反序列化
	// ============================================================

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << "Untitled";
		out << YAML::Key << "Version" << YAML::Value << 6;
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		m_Scene->m_Registry.view<entt::entity>().each([&](entt::entity handle) {
			Entity entity{ handle, m_Scene.get() };
			if (!entity.HasComponent<TagComponent>()) return;

			out << YAML::BeginMap;
			out << YAML::Key << "Entity" << YAML::Value
				<< static_cast<uint64_t>(entity.GetUUID());
			out << YAML::Key << "Components" << YAML::Value << YAML::BeginMap;

			if (entity.HasComponent<TagComponent>())
				SerializeComponent(out, entity.GetComponent<TagComponent>());
			if (entity.HasComponent<TransformComponent>())
				SerializeComponent(out, entity.GetComponent<TransformComponent>());
			if (entity.HasComponent<SpriteRendererComponent>())
				SerializeComponent(out, entity.GetComponent<SpriteRendererComponent>());
			if (entity.HasComponent<ModelRendererComponent>())
				SerializeComponent(out, entity.GetComponent<ModelRendererComponent>());
			if (entity.HasComponent<MaterialComponent>())
				SerializeComponent(out, entity.GetComponent<MaterialComponent>());
			if (entity.HasComponent<TerrainComponent>())
				SerializeComponent(out, entity.GetComponent<TerrainComponent>());
			if (entity.HasComponent<DirectionalLightComponent>())
				SerializeComponent(out, entity.GetComponent<DirectionalLightComponent>());
			if (entity.HasComponent<PointLightComponent>())
				SerializeComponent(out, entity.GetComponent<PointLightComponent>());
			if (entity.HasComponent<SkyLightComponent>())
				SerializeComponent(out, entity.GetComponent<SkyLightComponent>());
			if (entity.HasComponent<CameraComponent>())
				SerializeComponent(out, entity.GetComponent<CameraComponent>());

			out << YAML::EndMap; // Components
			out << YAML::EndMap; // Entity
		});

		out << YAML::EndSeq; // Entities
		out << YAML::EndMap; // Root

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		try
		{
			YAML::Node data = YAML::LoadFile(filepath);
			if (!data["Entities"] ) return false;

			const uint32_t version = data["Version"] ? data["Version"].as<uint32_t>() : 1;

			auto entities = data["Entities"];
			for (auto entityNode : entities)
			{
				if (!entityNode["Components"]) continue;

				// 从 TagComponent 获取名称，无则用默认名
				std::string name = "Entity";
				auto& comps = entityNode["Components"];
				if (comps["TagComponent"])
					name = comps["TagComponent"].as<std::string>();

				Entity entity;
				if (version >= 2 && entityNode["Entity"])
				{
					UUID uuid(entityNode["Entity"].as<uint64_t>());
					if (static_cast<uint64_t>(uuid) != 0)
						entity = m_Scene->CreateEntityWithUUID(uuid, name);
				}
				if (!entity)
					entity = m_Scene->CreateEntity(name);
				auto& tagComp = entity.GetComponent<TagComponent>();
				DeserializeComponent(comps["TagComponent"], tagComp);

				if (comps["TransformComponent"])
				{
					auto& tc = entity.GetComponent<TransformComponent>();
					DeserializeComponent(comps["TransformComponent"], tc);
				}
				if (comps["SpriteRendererComponent"])
				{
					auto& sc = entity.AddComponent<SpriteRendererComponent>();
					DeserializeComponent(comps["SpriteRendererComponent"], sc);
				}
				if (comps["ModelRendererComponent"])
				{
					auto& model = entity.AddComponent<ModelRendererComponent>();
					DeserializeComponent(comps["ModelRendererComponent"], model);
				}
				if (comps["MaterialComponent"])
				{
					auto& mc = entity.AddComponent<MaterialComponent>();
					DeserializeComponent(comps["MaterialComponent"], mc);
				}
				if (comps["TerrainComponent"])
				{
					auto& terrain = entity.AddComponent<TerrainComponent>();
					DeserializeComponent(comps["TerrainComponent"], terrain);
				}				if (comps["DirectionalLightComponent"])
				{
					auto& light = entity.AddComponent<DirectionalLightComponent>();
					DeserializeComponent(comps["DirectionalLightComponent"], light);
				}
				if (comps["PointLightComponent"])
				{
					auto& light = entity.AddComponent<PointLightComponent>();
					DeserializeComponent(comps["PointLightComponent"], light);
				}
				if (comps["SkyLightComponent"])
				{
					auto& skyLight = entity.AddComponent<SkyLightComponent>();
					DeserializeComponent(comps["SkyLightComponent"], skyLight);
				}
				if (comps["CameraComponent"])
				{
					auto& cc = entity.AddComponent<CameraComponent>();
					DeserializeComponent(comps["CameraComponent"], cc);
				}
			}

			GL_CORE_INFO("Scene deserialized: {0} entities", data["Entities"].size());
			return true;
		}
		catch (const YAML::Exception& e)
		{
			GL_CORE_ERROR("YAML parse error: {0}", e.what());
			return false;
		}
	}

}
