#pragma once

// ���뵼���ӿ� ����
#ifdef TRVLM_EXPORTS
#define TRAVEL_DLL _declspec(dllexport)
#else
#define TRAVEL_DLL _declspec(dllimport)
#endif
