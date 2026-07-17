#include "glpch.h"
#include "Glimmer/Utils/FileDialog.h"
#include "Glimmer/Core/Application.h"

#include <commdlg.h>
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace gl::FileDialog {

	static std::string ShowDialog(const char* filter, const char* defaultExt, bool save)
	{
		// 获取原生 HWND（用于模态化对话框）
		HWND hwnd = nullptr;
		auto* native = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (native)
			hwnd = glfwGetWin32Window(native);

		char filePath[MAX_PATH] = { 0 };

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(OPENFILENAMEA);
		ofn.hwndOwner = hwnd;
		ofn.lpstrFilter = filter;
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrDefExt = defaultExt;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

		BOOL result;
		if (save)
			result = GetSaveFileNameA(&ofn);
		else
			result = GetOpenFileNameA(&ofn);

		return result ? std::string(filePath) : std::string();
	}

	std::string OpenFile(const char* filter)
	{
		return ShowDialog(filter, "glimmer", false);
	}

	std::string SaveFile(const char* filter)
	{
		return ShowDialog(filter, "glimmer", true);
	}

}
