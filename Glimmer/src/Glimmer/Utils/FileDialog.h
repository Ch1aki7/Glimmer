#pragma once
#include <string>

namespace gl::FileDialog {

	// 返回所选文件路径，取消时返回空字符串
	// filter: "Glimmer Scene\0*.glimmer\0All Files\0*.*\0"
	std::string OpenFile(const char* filter);
	std::string SaveFile(const char* filter);

}
