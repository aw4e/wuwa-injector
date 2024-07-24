#include "injector.h"

#include <manual-map.h>
#include <load-library.h>

bool InjectDLL(HANDLE hProc, const std::string& filepath)
{
#ifdef MANUAL_MAP
	bool result = ManualMapDLL(hProc, filepath);
#else
	bool result = LoadLibraryDLL(hProc, filepath);
#endif
	return result;
}