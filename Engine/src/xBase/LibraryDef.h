#pragma once

// 导入导出接口 定义
#ifdef TRVLM_EXPORTS
#define TRAVEL_DLL _declspec(dllexport)
#else
#define TRAVEL_DLL _declspec(dllimport)
#endif
