// Glimmer/src/Glimmer.h
#pragma once

// 供客户端（Sandbox）使用的总头文件
#include "Glimmer/Core/Application.h"
#include "Glimmer/Core/Layer.h"
#include "Glimmer/Core/Log.h"
#include "Glimmer/Core/Timestep.h"

#include "Glimmer/Core/Input.h"
#include "Glimmer/Core/KeyCodes.h"
#include "Glimmer/Core/MouseButtonCodes.h"

// --- 渲染器部分 ---
#include "Glimmer/Renderer/Renderer.h"
#include "Glimmer/Renderer/RenderCommand.h"
#include "Glimmer/Renderer/Buffer.h"
#include "Glimmer/Renderer/Shader.h"
#include "Glimmer/Renderer/VertexArray.h"
#include "Glimmer/Renderer/OrthographicCamera.h"
#include "Glimmer/Renderer/OrthographicCameraController.h"
#include "Glimmer/Renderer/Texture.h"

// --- 入口点 ---
#include "Glimmer/Core/EntryPoint.h"

#include <imgui.h> // 方便在 Layer 里写 UI
