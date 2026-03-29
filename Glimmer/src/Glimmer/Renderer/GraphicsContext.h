#pragma once

namespace gl {

    // 这是一个纯虚接口，定义了所有图形 API 上下文必须具备的功能
    class GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual void Init() = 0;        // 初始化（加载驱动函数指针）
        virtual void SwapBuffers() = 0; // 交换缓冲区（将画面呈现到屏幕）
    };

}