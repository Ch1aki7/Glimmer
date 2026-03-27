#pragma once
#include "Glimmer/Layer.h"

namespace gl {
    class ImGuiLayer : public Layer {
    public:
        ImGuiLayer();
        ~ImGuiLayer();

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnUpdate() override;
        virtual void OnEvent(Event& event) override;

        virtual void OnImGuiRender() override;

        void Begin(); // 每帧开始前呼叫
        void End();   // 每帧结束后呼叫
    private:
        float m_Time = 0.0f;
    };
}