// Glimmer/src/Glimmer.h
#pragma once

// 供客户端（Sandbox）使用的总头文件
#include "Glimmer/Core/Application.h"
#include "Glimmer/Core/Layer.h"
#include "Glimmer/Core/Log.h"
#include "Glimmer/Core/Timestep.h"
#include "Glimmer/Core/UUID.h"

#include "Glimmer/Core/Input.h"
#include "Glimmer/Core/KeyCodes.h"
#include "Glimmer/Core/MouseButtonCodes.h"

#include "Glimmer/Asset/Asset.h"
#include "Glimmer/Asset/AssetManager.h"

// --- 渲染器部分 ---
#include "Glimmer/Renderer/Renderer.h"
#include "Glimmer/Renderer/Renderer2D.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Renderer/OrthographicCamera.h"
#include "Glimmer/Renderer/OrthographicCameraController.h"
#include "Glimmer/Renderer/PerspectiveCamera.h"
#include "Glimmer/Renderer/Texture.h"
#include "Glimmer/Renderer/Model.h"
#include "Glimmer/Renderer/Mesh.h"
#include "Glimmer/Renderer/FrameBuffer.h"

#include <imgui.h> // 方便在 Layer 里写 UI

// --- ECS部分 ---
#include "Glimmer/Scene/Scene.h"
#include "Glimmer/Scene/Entity.h"
#include "Glimmer/Scene/ScriptableEntity.h"
#include "Glimmer/Scene/Components.h"
