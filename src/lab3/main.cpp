// Практическая работа №3
// 1) Редукция без блочной декомпозиции
// 2) Редукция без блочной декомпозиции c окном
// 3) Блочный алгоритм с расхождением
// 4) Блочный каскадный алгоритм


#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>
#include <numeric>
#include <utility>
#include <functional>
#include <string>

#include <Windows.h>

#include <amp.h>

#include "timer.h"

template <typename T> 
void FillRandom(std::vector<T>& container, T min, T max)
{
    std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<T> uid(min, max);
    auto roll = std::bind(uid, generator);
    std::generate(std::begin(container), std::end(container), roll);
}


template <typename T> 
void Reduction(std::vector<T>& container)
{
    namespace cncr = concurrency;

    cncr::array_view<T, 1> array(container.size(), container);

    for (unsigned int stride = container.size() / 2; stride >= 1; stride /= 2)
    {
        cncr::parallel_for_each(cncr::extent<1>(stride), 
            [=](cncr::index<1> idx) restrict(amp) {
                array[idx] += array[idx[0] + stride];
            });

        array.synchronize();

        if (stride % 2 != 0 && stride != 1)
        {
            array[0] += array[stride - 1];
        }
    }
}

template <typename T, std::size_t WindowSize> 
void WindowReduction(std::vector<T>& container)
{
    namespace cncr = concurrency;

    cncr::array_view<T, 1> array(container.size(), container);

    for (unsigned int stride = container.size() / WindowSize; stride >= 1; stride /= WindowSize)
    {
        cncr::parallel_for_each(cncr::extent<1>(stride), 
            [=](cncr::index<1> idx) restrict(amp) {
                for (int i{1}; i < WindowSize; ++i)
                {
                    array[idx] += array[idx[0] + i * stride];
                }
            });

        array.synchronize();

        if (stride % WindowSize != 0 && stride != 1)
        {
            array[0] += array[stride - 1];
        }
    }
}

template <typename T, std::size_t TileSize> 
void DivergenceReduction(std::vector<T>& container)
{
    namespace cncr = concurrency;

    cncr::array_view<T, 1> array(container.size(), container);

    auto size = container.size();

    while (size / TileSize >= TileSize) 
    {
        cncr::parallel_for_each(array.extent.tile<TileSize>(), 
            [=](cncr::tiled_index<TileSize> ind) restrict(amp) 
            {
                tile_static int shared_array[TileSize];
                shared_array[ind.local[0]] = array(ind.global[0]);
                ind.barrier.wait();

                int stride = 1;
                while (stride < TileSize) 
                {
                    if (ind.local[0] % (2 * stride) == 0 && ind.local[0] + stride < TileSize) 
                    {
                        shared_array[ind.local[0]] += shared_array[ind.local[0] + stride];
                    }
                    stride *= 2;

                    ind.barrier.wait();
                }

                if (ind.local[0] == 0) 
                {
                    array[ind.tile[0]] = shared_array[ind.local[0]];
                }

                ind.barrier.wait();
            });

        array.synchronize();
        size /= TileSize;
    }

    for (int i = 1; i < size; i++) array[0] += array[i];

    array.synchronize();

}

template <typename T, std::size_t WindowSize, std::size_t TileSize> 
void CascadeReduction(std::vector<T>& container)
{
    namespace cncr = concurrency;

    cncr::array_view<T, 1> array(container.size(), container);

    cncr::parallel_for_each(cncr::extent<1>(container.size() / WindowSize).tile<TileSize>(),
        [=](cncr::tiled_index<TileSize> tidx) restrict(amp) 
        {
            tile_static int local[TileSize];

            int localIdx = tidx.local[0];
            int globalIdx = tidx.global[0] * WindowSize;

            int localSum = array[globalIdx];

            for (int i = 1; i < WindowSize; i++) 
            {
                localSum += array[globalIdx + i];
            }

            local[localIdx] = localSum;
            tidx.barrier.wait();

            for (int step = TileSize / 2; step > 0; step /= 2) 
            {
                if (localIdx < step)
                    local[localIdx] += local[localIdx + step];
                tidx.barrier.wait();
            }

            if (localIdx == 0)
            {
                array[globalIdx] = local[localIdx];
            }
        });

    array.synchronize();

    int numberOfLocalSums = container.size() / TileSize / WindowSize;

    int sum = 0;
    for (int i = 0; i < numberOfLocalSums; i++) {
        sum += array[i * TileSize * WindowSize];
    }

    array[0] = sum;
}

int main()
{
    using ValueType = int;
    using namespace std;

    const vector<pair<string, function<void(vector<ValueType>&)>>> functions{
        {"Reduction", Reduction<ValueType>},
        {"WindowReduction", WindowReduction<ValueType, 2>},
        {"DivergenceReduction", DivergenceReduction<ValueType, 2>},
        {"CascadeReduction", CascadeReduction<ValueType, 5, 2>}
    };

    const size_t array_size{ 1024 };
    vector<ValueType> container(array_size);

    for (const auto&[name, f] : functions) 
    {
        FillRandom(container, -100, 100);
        Timer timer;
        timer.Start();
        f(container);
        timer.Stop();
        cout << "Function: " << name << endl;
        cout << "Size: " << container.size() << endl;
        cout << "Result: " << container[0] << endl;
        cout << "Elapsed: " << timer.Elapsed() << " ms" << endl << endl;
    }

    return 0;
}
