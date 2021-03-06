#pragma once

#include "stream.h"

#include <memory>

typedef struct HWND__ *HWND;

namespace micro_profiler
{
	std::auto_ptr<write_stream> create_file(HWND hparent, const std::wstring &default_name);
	std::auto_ptr<read_stream> open_file(HWND hparent, std::wstring& path);
}
