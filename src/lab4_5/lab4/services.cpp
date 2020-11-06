#include "services.h"

void services::sequental(PixelVector& state, int field_size, PixelVector& state_double, int iter) {
	// Указатель на массив со стартовыми значениями
	auto data = &(state[0]);  


	auto new_data = &(state_double[0]);
	const auto size = state.size();

	for (size_t i = 0; i < size; i++)
		new_data[i] = data[i];

	const auto n = field_size - 2;
	const auto t = field_size - 1;

	// С какого элемента начинается последняя строка
	const auto lastrow = field_size * t;

	//С какого элемента начинается предпоследняя строка
	const auto prelastrow = lastrow - field_size;

	for (size_t i = 0; i < iter; i++) {
		for (size_t y = 0; y < field_size; y++) {
			const auto row_idx = y * field_size;
			new_data[row_idx] = new_data[row_idx + n];
			new_data[row_idx + t] = new_data[row_idx + 1];
		}
		for (size_t x = 0; x < field_size; x++) {
			new_data[x] = new_data[prelastrow + x];
			new_data[lastrow + x] = new_data[field_size + x];
		}
		for (int y = 1; y < t; y++) {
			const auto y0 = (y - 1) * field_size;
			const auto y1 = y0 + field_size;
			const auto y2 = y1 + field_size;
			for (int x = 1; x < t; x++) {
				const auto x0 = x - 1;
				const auto x2 = x + 1;
				const int sum = new_data[y0 + x0] + new_data[y0 + x] + new_data[y0 + x2] + new_data[y1 + x0] + new_data[y1 + x2] + new_data[y2 + x0]
					+ new_data[y2 + x] + new_data[y2 + x2];

				const auto current_element = x + y1;
				assert(current_element < size);
				if (sum == 3)
					data[current_element] = 1;
				else if (sum == 2)
					data[current_element] = new_data[current_element];
				else
					data[current_element] = 0;
			}
		}

		//std::swap(data, new_data);
	}
}

void services::parallel(PixelVector& state, int field_size, PixelVector& state_double, int iter) {
	
	using namespace concurrency;

	array_view<PixelData, 2> data_out(field_size, field_size, &(state[0]));
	data_out.discard_data();  

	array_view<PixelData, 2> new_buffer(field_size, field_size, &(state_double[0]));
	new_buffer.discard_data(); 

	copy(state.cbegin(), state.cend(), new_buffer);

	const auto n = field_size - 2;
	const auto t = field_size - 1;

	for (size_t i = 0; i < iter; i++) {
		parallel_for_each(extent<1>(field_size),
			[=](index<1> idx) restrict(amp)
			{
				const auto row_idx = idx[0];
				new_buffer(0, row_idx) = new_buffer(n, row_idx);
				new_buffer(t, row_idx) = new_buffer(1, row_idx);
			});
		parallel_for_each(extent<1>(field_size), [=](index<1> idx) restrict(amp)
			{
				const auto x = idx[0];
				new_buffer(x, 0) = new_buffer(x, n);
				new_buffer(x, t) = new_buffer(x, 1);
			});
		parallel_for_each(extent<1>(n), [=](index<1> idx) restrict(amp)
			{
				const auto y0 = idx[0];
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
					if (sum == 3)
						data_out(x, y1) = 1;
					else if (sum == 2)
						data_out(x, y1) = new_buffer(x, y1);
					else
						data_out(x, y1) = 0;
				}
			});
		std::swap(data_out, new_buffer);
	}
	data_out.synchronize();
}

void services::parallel_branchless(PixelVector& state, int field_size, PixelVector& state_double, int iter) {
	using namespace concurrency;
	array_view<PixelData, 2> data_out(field_size, field_size, &(state[0]));
	data_out.discard_data();
	array_view<PixelData, 2> new_buffer(field_size, field_size, &(state_double[0]));
	new_buffer.discard_data();
	copy(state.cbegin(), state.cend(), new_buffer);
	const PixelData rules[18] = {
		0, 0, 0, 1, 0, 0, 0, 0, 0, // dead
		0, 0, 1, 1, 0, 0, 0, 0, 0 // alive
	};
	array_view<const PixelData, 2> rules_in(9, 2, rules);
	const auto n = field_size - 2;
	const auto t = field_size - 1;
	for (size_t i = 0; i < iter; i++) {
		parallel_for_each(extent<1>(field_size),
			[=](index<1> idx) restrict(amp)
			{
				const auto row_idx = idx[0];
				new_buffer(0, row_idx) = new_buffer(n, row_idx);
				new_buffer(t, row_idx) = new_buffer(1, row_idx);
			});
		parallel_for_each(extent<1>(field_size), [=](index<1> idx) restrict(amp)
			{
				const auto x = idx[0];
				new_buffer(x, 0) = new_buffer(x, n);
				new_buffer(x, t) = new_buffer(x, 1);
			});
		parallel_for_each(extent<1>(n), [=](index<1> idx) restrict(amp)
			{
				const auto y0 = idx[0];
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
					data_out(x, y1) = rules_in(new_buffer(x, y1), sum);
				}
			});
		std::swap(data_out, new_buffer);
	}
	data_out.synchronize();
}

void services::parallel_branchless_const(PixelVector& state, int field_size, PixelVector& state_double, int iter) {
	using namespace concurrency;

	array_view<PixelData, 2> data_out(field_size, field_size, &(state[0]));
	data_out.discard_data();

	array_view<PixelData, 2> new_buffer(field_size, field_size, &(state_double[0]));
	new_buffer.discard_data();

	copy(state.cbegin(), state.cend(), new_buffer);

	const auto n = field_size - 2;
	const auto t = field_size - 1;

	SharedRules r;
	for (size_t i = 0; i < iter; i++) {
		parallel_for_each(extent<1>(field_size),
			[=](index<1> idx) restrict(amp)
			{
				const auto row_idx = idx[0];
				new_buffer(0, row_idx) = new_buffer(n, row_idx);
				new_buffer(t, row_idx) = new_buffer(1, row_idx);
			});
		parallel_for_each(extent<1>(field_size), [=](index<1> idx) restrict(amp)
			{
				const auto x = idx[0];
				new_buffer(x, 0) = new_buffer(x, n);
				new_buffer(x, t) = new_buffer(x, 1);
			});
		parallel_for_each(extent<1>(n), [=](index<1> idx) restrict(amp)
			{
				const auto y0 = idx[0];
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
					data_out(x, y1) = r.rules[new_buffer(x, y1) * 9 + sum];
				}
			});
		std::swap(data_out, new_buffer);
	}
	data_out.synchronize();
}
