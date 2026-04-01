#pragma once

#include "Glimmer/Core/Core.h"
#include "Glimmer/Events/Event.h"
#include "Glimmer/Core/Timestep.h"

namespace gl {

    class Layer {
    public:
        Layer(const std::string& name = "Layer");
        virtual ~Layer();

        virtual void OnAttach() {}    // 当图层被推入引擎时调用（类似 Start）
        virtual void OnDetach() {}    // 当图层被移除时调用
        virtual void OnUpdate(Timestep ts) {}
        virtual void OnEvent(Event& event) {} // 当事件发生时调用
        virtual void OnImGuiRender() {} // 【新增】：专门用于画 UI 的函数
        inline const std::string& GetName() const { return m_DebugName; }
    protected:
        std::string m_DebugName; // 用于调试的名字
    };

}
