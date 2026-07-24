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
		out << YAML::Key << "Version" << YAML::Value << 2;
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
