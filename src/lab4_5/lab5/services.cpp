#include "services.h"

void services::sequental(PixelVector& input_v, PixelVector& output_v, PixelVector& source_v, int field_size, int k, int iter) {

	auto input = &(input_v[0]);
	auto output = &(output_v[0]);
	const auto source = &(source_v[0]);

	const auto n = field_size - 1;

	for (size_t i = 0; i < iter; i++) {

		for (int y = 0; y < field_size; y++) {
			const auto y0 = (y - 1) * field_size;
			const auto y1 = y0 + field_size;
			const auto y2 = y1 + field_size;

			for (int x = 0; x < field_size; x++) {
				const auto top = y0 + x;
				const auto bottom = y2 + x;
				const auto center = y1 + x;
				const auto right = center + 1;
				const auto left = center - 1;
				auto sum = 0;
				if (x != 0) 
					sum += input[left];
				if (x != n) 
					sum += input[right];
				if (y != 0) 
					sum += input[top];
				if (y != n) 
					sum += input[bottom];
				output[center] = input[center] + (sum - 4 * input[center]) * k;
			}
		}
		for (int y = 0; y < field_size; y++) {
			const auto y1 = y * field_size;
			for (int x = 0; x < field_size; x++) {
				const auto center = y1 + x;
				if (source[center] != 0) {
					output[center] = source[center];
				}
			}
		}
		std::swap(input, output);
	}
	if ((iter & 1) == 1)
		std::swap(input, output);
}

void services::global(PixelVector& input_v, PixelVector& output_v, PixelVector& source_v, int field_size, int k, int iter) {
	using namespace concurrency;

	const auto input = &(input_v[0]);
	const auto output = &(output_v[0]);
	const auto source = &(source_v[0]);

	const auto n = field_size - 1;

	array_view<PixelData, 2> in(field_size, field_size, input);
	array_view<PixelData, 2> out(field_size, field_size, output);
	out.discard_data();
	array_view<const PixelData, 2> heat(field_size, field_size, source);

	for (size_t i = 0; i < iter; i++) {
		parallel_for_each(out.extent, [=](index<2>idx)restrict(amp)
			{
				const auto x = idx[0];
				const auto y = idx[1];
				auto neighbours_sum = 0;
				if (x != 0) neighbours_sum += in(x - 1, y);
				if (x != n) neighbours_sum += in(x + 1, y);
				if (y != 0) neighbours_sum += in(x, y - 1);
				if (y != n) neighbours_sum += in(x, y + 1);
				out[idx] = in[idx] + (neighbours_sum - 4 * in[idx]) * k;
			}
		);
		parallel_for_each(out.extent, [=](index<2>idx)restrict(amp)
			{
				if (heat[idx] != 0)
					out[idx] = heat[idx];
			}
		);
		std::swap(in, out);
	}
	if ((iter & 1) == 1)
		std::swap(in, out);
	out.synchronize();
}

void services::textured(PixelVector& input_v, PixelVector& output_v, PixelVector& source_v, int field_size, int k, int iter) {
	using namespace concurrency;
	const auto output = &(output_v[0]);
	const auto n = field_size - 1;
	graphics::texture<PixelData, 2> in(field_size, field_size, input_v.cbegin(), input_v.cend()); 
	graphics::texture<PixelData, 2> out(field_size, field_size);
	graphics::texture<PixelData, 2> heat(field_size, field_size, source_v.cbegin(), source_v.cend());
	for (size_t i = 0; i < iter; i++) {
		parallel_for_each(out.extent, [=, &in, &out](index<2>idx)restrict(amp)
			{
				const auto x = idx[0];
				const auto y = idx[1];
				auto neighbours_sum = 0;
				if (x != 0) neighbours_sum += in(x - 1, y);
				if (x != n) neighbours_sum += in(x + 1, y);
				if (y != 0) neighbours_sum += in(x, y - 1);
				if (y != n) neighbours_sum += in(x, y + 1);
				out.set(idx, in(idx) + (neighbours_sum - 4 * in(idx)) * k);
			}
		);
		parallel_for_each(out.extent, [=, &heat, &out](index<2>idx) restrict(amp)
			{
				if (heat(idx) != 0)
					out.set(idx, heat(idx));
			}
		);
		std::swap(in, out);
	}
	if ((iter & 1) == 1)
		std::swap(in, out);
	graphics::copy(out, output, static_cast<unsigned int>(output_v.size() * sizeof(PixelData)));
}
