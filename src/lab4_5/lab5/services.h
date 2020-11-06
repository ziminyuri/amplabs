#pragma once
#include "stdafx.h"

using PixelData = int;
using PixelVector = std::vector<PixelData>;
namespace services {
	using FunctionSignature = void (*)(PixelVector& input_v, PixelVector& output_v, PixelVector& source_v, int field_size, int k, int iter);
	void sequental(PixelVector& input_v, PixelVector& output_v, PixelVector& source_v, int field_size, int k, int iter);
	void global(PixelVector& input, PixelVector& output, PixelVector& sources, int field_size, int k, int iter);
	void textured(PixelVector& input, PixelVector& output, PixelVector& sources, int field_size, int k, int iter);
};