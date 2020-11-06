#pragma once
#include "stdafx.h"

using PixelData = int;
using PixelVector = std::vector<PixelData>;

struct SharedRules {
	const int rules[18] = {
		 0, 0, 0, 1, 0, 0, 0, 0, 0, // dead
		 0, 0, 1, 1, 0, 0, 0, 0, 0 // alive
	};
};

namespace services {
	using FunctionSignature = void (*)(PixelVector& state, int field_size, PixelVector& state_double, int iter);
	void sequental(PixelVector& state, int field_size, PixelVector& state_double, int iter);
	void parallel(PixelVector& state, int field_size, PixelVector& state_double, int iter);
	void parallel_branchless(PixelVector& state, int field_size, PixelVector& state_double, int iter);
	void parallel_branchless_const(PixelVector& state, int field_size, PixelVector& state_double, int iter);
	template <int tile_size>
	void parallel_branchless_shared(PixelVector& state, int field_size, PixelVector& state_double, int iter) {
		using namespace concurrency;
		array_view<PixelData, 2> data_out(field_size, field_size, &(state[0]));
		data_out.discard_data();
		array_view<PixelData, 2> new_buffer(field_size, field_size, &(state_double[0]));
		new_buffer.discard_data();
		copy(state.cbegin(), state.cend(), new_buffer);
		const auto n = field_size - 2;
		const auto t = field_size - 1;
		const int rules[18] = {
			0, 0, 0, 1, 0, 0, 0, 0, 0, // dead
			0, 0, 1, 1, 0, 0, 0, 0, 0 // alive
		};
		for (size_t i = 0; i < iter; i++) {
			concurrency::parallel_for_each(
				concurrency::extent<1>(field_size),
				[=](index<1> idx) restrict(amp)
				{
					const auto row_idx = idx[0];
					new_buffer(0, row_idx) = new_buffer(n, row_idx);
					new_buffer(t, row_idx) = new_buffer(1, row_idx);
				});
			concurrency::parallel_for_each(
				concurrency::extent<1>(field_size),
				[=](index<1> idx) restrict(amp)
				{
					const auto x = idx[0];
					new_buffer(x, 0) = new_buffer(x, n);
					new_buffer(x, t) = new_buffer(x, 1);
				});
			concurrency::parallel_for_each(
				concurrency::extent<1>(n).tile<tile_size>(),
				[=](concurrency::tiled_index<tile_size> tidx) restrict(amp)
				{
					const auto y0 = tidx.global[0];
					const auto y1 = y0 + 1;
					const auto y2 = y1 + 1;
					for (int x = 1; x < t; x++)
					{
						const auto xl = x - 1;
						const auto xr = x + 1;
						const int sum = new_buffer(xl, y1)
							+ new_buffer(xr, y1)
							+ new_buffer(xl, y0)
							+ new_buffer(x, y0)
							+ new_buffer(xr, y0)
							+ new_buffer(xl, y2)
							+ new_buffer(x, y2)
							+ new_buffer(xr, y2);
						data_out(x, y1) = rules[new_buffer(x, y1) * 9 + sum];
					}
				});
			std::swap(data_out, new_buffer);
		}
		data_out.synchronize();
	}
};