#include "gui.h"
#include "timer.h"

using namespace std;
using namespace services;

double life_check(const FunctionSignature func, size_t size, int runs) {
	const auto shadow_size = size + 2; //Длина в строке вместе с теневыми
	const auto data_size = shadow_size * shadow_size; // Количество ячеек
	unique_ptr<vector<PixelData>> a(new vector<PixelData>(data_size));
	unique_ptr<vector<PixelData>> b(new vector<PixelData>(data_size));
	auto arr = *a;

	// Заполнили нулями или единицами
	for (size_t i = 0; i < data_size; i++)
		arr[i] = rand() & 1;


	double acc_time = 0.0;
	for (size_t i = 0; i < runs; i++) {
		Timer t;
		t.Start();
		func(arr, static_cast<int>(shadow_size), *b, 10);
		t.Stop();
		acc_time += t.Elapsed();
	}
	return acc_time / static_cast<double>(runs);
}

void bench() {
	srand(static_cast<unsigned int>(time(0)));
	const int runs = 1;
	const vector<int> sizes{ 2048, 4096, 8192 };
	//life_check(sequental, 1024, 1);
	life_check(sequental, 10, 1);
	const vector<tuple<string, FunctionSignature>> to_check = {
		{"seq", sequental},
		{"parallel", parallel},
		{"parallel_branchless", parallel_branchless},
		{"parallel_branchless_const", parallel_branchless_const},
		{"parallel_branchless_shared", parallel_branchless_shared<8>},
	};
	for (const auto& check : to_check) {
		cout << "Function: " << std::get<0>(check) << endl;
		for (const auto size : sizes) {
			wcout << "Size: " << size << endl;
			wcout << "Elapsed: " << life_check(std::get<1>(check), size, runs) << " ms" << endl;
		}
			
	}
}

int main() {
	bench();
	GUI app;
	app.start();
	return 0;
}
