#include "gui.h"
#include "timer.h"

using namespace std;
using namespace services;

double heat_map_check(const FunctionSignature func, size_t size, int runs) {

	const auto data_size = size * size;

	unique_ptr<vector<PixelData>> in(new vector<PixelData>(data_size));
	unique_ptr<vector<PixelData>> out(new vector<PixelData>(data_size));
	unique_ptr<vector<PixelData>> heat(new vector<PixelData>(data_size));

	auto& a_vec = *in;
	auto& heat_vec = *out;

	for (size_t i = 0; i < data_size; i++)
		heat_vec[i] = (rand() & 1) * 100;

	double acc_time = 0.0;
	for (size_t i = 0; i < runs; i++) {
		Timer t;
		t.Start();
		func(*in, *out, *heat, static_cast<int>(size), 2, 10);
		t.Stop();
		acc_time += t.Elapsed();
	}
	return acc_time / static_cast<double>(runs);
}

void counting() {
	srand(static_cast<unsigned int>(time(0)));
	const int runs = 2;
	const vector<int> sizes{ 2048, 4096, 8192 };
	const vector<tuple<string, FunctionSignature>> to_check = {
		{"sequental", sequental},
		{"global", global},
		{"textured", textured}
	};
	//heat_map_check(global, 64, 1);
	for (const auto& check : to_check) {
		cout << "Function: " << std::get<0>(check) << endl;
		
		for (const auto size : sizes) {
			wcout << "Size: " << size << endl;
			wcout << "Elapsed: " << heat_map_check(std::get<1>(check), size, runs) << endl;
		}
	}
}

int main() {
	counting();
	GUI app;
	app.start();
	return 0;
}
