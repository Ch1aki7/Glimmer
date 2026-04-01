// Glimmer/src/Glimmer.h
#pragma once

// 供客户端（Sandbox）使用的总头文件
#include "Glimmer/Application.h"
#include "Glimmer/Layer.h"
#include "Glimmer/Log.h"
#include "Glimmer/Core/Timestep.h"

#include "Glimmer/Input.h"
#include "Glimmer/KeyCodes.h"
#include "Glimmer/MouseButtonCodes.h"

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
#include "Glimmer/EntryPoint.h"

#include <imgui.h> // 方便在 Layer 里写 UI
