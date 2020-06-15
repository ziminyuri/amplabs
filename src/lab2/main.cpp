// Практическая работа №2
// 1) Блочная версия транспонирования
// 2) Блочная версия матричного умножения, без использования разделяемой памяти
// 3) Блочная версия матричного умножения с использованием разделяемой памяти
// 4) Матричное умножение с "укрупненной" декомпозицией (каждый поток вычисляет элементы одной строки)

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iomanip>
#include <random>
#include <vector>

#include <Windows.h>

#include <amp.h>

#include "timer.h"

template <typename T> 
void Print(const std::vector<T>& container, unsigned int M)
{
    for (std::size_t row{}; row < M; ++row)
    {
        for (std::size_t col{}; col < M; ++col)
        {
            std::cout << std::setw(4) << container[row * M + col];
        }

        std::cout << std::endl;
    }
}

template <typename T> 
void FillRandom(std::vector<T>& container, T min, T max)
{
    std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<T> uid(min, max);
    auto roll = std::bind(uid, generator);
    std::generate(std::begin(container), std::end(container), roll);
}

template <int TileSize, typename T> 
void Transpose(std::vector<T>& matrix, int matrix_size)
{
    using std::begin, std::end;
    namespace cncr = concurrency;

    if (matrix_size % TileSize != 0)
    {
        std::cerr << L"Размер матрицы не кратен размеру тайла." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    cncr::array<T, 2> gpu_matrix(matrix_size, matrix_size, begin(matrix), end(matrix));
    cncr::array_view<T, 2> matrix_view(matrix_size, matrix_size, matrix);
    matrix_view.discard_data();

    cncr::parallel_for_each(gpu_matrix.extent.tile<TileSize, TileSize>(),
        [=, &gpu_matrix](cncr::tiled_index<TileSize, TileSize> tidx) restrict(amp)
        {
            tile_static T localData[TileSize][TileSize + 1];
            localData[tidx.local[1]][tidx.local[0]] = gpu_matrix[tidx.global];
            tidx.barrier.wait();

            auto outIdx = cncr::index<2>(tidx.tile_origin[1], tidx.tile_origin[0]) + tidx.local;
            matrix_view[outIdx] = localData[tidx.local[0]][tidx.local[1]];
        });

    matrix_view.synchronize();
}

template <int TileSize, typename T> 
std::vector<T> MultiplyWithoutShared(
    const std::vector<T>& matrix_l,
    const std::vector<T>& matrix_r,
    unsigned int matrix_size)
{
    using std::begin, std::end;
    namespace cncr = concurrency;

    if (matrix_size % TileSize != 0)
    {
        std::cerr << L"Размер матрицы не кратен размеру тайла." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::vector<T> product_matrix(matrix_size * matrix_size);

    cncr::array_view<const T, 2> a(matrix_size, matrix_size, matrix_l);
    cncr::array_view<const T, 2> b(matrix_size, matrix_size, matrix_r);
    cncr::array_view<T, 2> product(matrix_size, matrix_size, product_matrix);
    product.discard_data();

    cncr::parallel_for_each(product.extent.tile<TileSize, TileSize>(),
        [=](cncr::tiled_index<TileSize, TileSize> t_idx) restrict(amp)
        {
            auto rowGlobal = t_idx.global[0];
            auto colGlobal = t_idx.global[1];
            T sum{};

            for (unsigned int i{}; i < matrix_size; ++i)
            {
                sum += a[rowGlobal][i] * b[i][colGlobal];
            }

            product[t_idx.global] = sum;
        });

    product.synchronize();

    return product_matrix;
}

template <int TileSize, typename T> 
std::vector<T> MultiplyWithShared(
    const std::vector<T>& matrix_l,
    const std::vector<T>& matrix_r,
    unsigned int matrix_size)
{
    using std::begin, std::end;
    namespace cncr = concurrency;

    if (matrix_size % TileSize != 0)
    {
        std::cerr << L"Размер матрицы не кратен размеру тайла." << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::vector<T> product_matrix(matrix_size * matrix_size);

    cncr::array_view<const T, 2> a(matrix_size, matrix_size, matrix_l.data());
    cncr::array_view<const T, 2> b(matrix_size, matrix_size, matrix_r.data());
    cncr::array_view<T, 2> product(matrix_size, matrix_size, product_matrix.data());

    cncr::parallel_for_each(product.extent.tile<TileSize, TileSize>(),
        [=](cncr::tiled_index<TileSize, TileSize> t_idx) restrict(amp)
        {
            auto row = t_idx.local[0];
            auto col = t_idx.local[1];
            auto rowGlobal = t_idx.global[0];
            auto colGlobal = t_idx.global[1];
            T sum{};

            for (unsigned int i{}; i < matrix_size; i += TileSize)
            {
                tile_static T locA[TileSize][TileSize];
                tile_static T locB[TileSize][TileSize];
                locA[row][col] = a(rowGlobal, col + i);
                locB[row][col] = b(row + i, colGlobal);

                t_idx.barrier.wait();

                for (int k{}; k < TileSize; ++k) 
                {
                    sum += locA[row][k] * locB[k][col];
                }

                t_idx.barrier.wait();
            }

            product[t_idx.global] = sum;
        });

    product.synchronize();

    return product_matrix;
}

template <typename T> 
std::vector<T> MultiplyByRow(
    const std::vector<T>& matrix_l,
    const std::vector<T>& matrix_r,
    unsigned int matrix_size)
{
    using std::begin, std::end;
    namespace cncr = concurrency;

    std::vector<T> product_matrix(matrix_size * matrix_size);

    cncr::array_view<const T, 2> a(matrix_size, matrix_size, matrix_l.data());
    cncr::array_view<const T, 2> b(matrix_size, matrix_size, matrix_r.data());
    cncr::array_view<T, 2> product(matrix_size, matrix_size, product_matrix.data());

    product.synchronize();

    cncr::parallel_for_each(cncr::extent<1>(matrix_size), 
        [=](cncr::index<1> idx) restrict(amp) 
        {
            auto row = idx[0];

            for (unsigned int col{}; col < matrix_size; ++col) 
            {
                T sum{};
                for (unsigned int i{}; i < matrix_size; i++)
                {
                    sum += a(row, i) * b(i, col);
				}
                product(row, col) = sum;
            }
        });

    return product_matrix;
}


int main()
{
    namespace cncr = concurrency;
    auto acc = cncr::accelerator(cncr::accelerator::default_accelerator);

    if (!acc.supports_cpu_shared_memory)
    {
        std::cerr << L"Стандартный ускоритель не поддерживает разделяемую память" << std::endl;
        return EXIT_FAILURE;
    }
    
    const unsigned int matrix_size{4};

    // 1) Блочная версия транспонирования
    std::vector<int> matrix_1(matrix_size * matrix_size);
    FillRandom(matrix_1, 0, 8);
    Timer timer;
    timer.Start();
    Transpose<2>(matrix_1, matrix_size);
    timer.Stop();
    std::cout << "Function: Transpose" << std::endl;
    std::cout << "Elapsed: " << timer.Elapsed() << " ms" << std::endl << std::endl;
    //Print(matrix_1, matrix_size);

    // 2) Блочная версия матричного умножения без использования разделяемой памяти
    std::vector<int> matrix_2(matrix_size * matrix_size);
    FillRandom(matrix_2, 0, 8);
    std::vector<int> matrix_3(matrix_size * matrix_size);
    FillRandom(matrix_3, 0, 8);
    timer.Start();
    auto matrix_4 = MultiplyWithoutShared<2>(matrix_2, matrix_3, matrix_size);
    timer.Stop();
    std::cout << "Function: MultiplyWithoutShared" << std::endl;
    std::cout << "Elapsed: " << timer.Elapsed() << " ms" << std::endl << std::endl;
    //Print(matrix_4, matrix_size);

    // 3) Блочная версия матричного умножения с использованием разделяемой памяти
    std::vector<int> matrix_5(matrix_size * matrix_size);
    FillRandom(matrix_5, 0, 8);
    std::vector<int> matrix_6(matrix_size * matrix_size);
    FillRandom(matrix_6, 0, 8);
    timer.Start();
    auto matrix_7 = MultiplyWithShared<2>(matrix_5, matrix_6, matrix_size);
    timer.Stop();
    std::cout << "Function: MultiplyWithShared" << std::endl;
    std::cout << "Elapsed: " << timer.Elapsed() << " ms" << std::endl << std::endl;
    //Print(matrix_7, matrix_size);

    // 4) Матричное умножение с "укрупненной" декомпозицией (каждый поток вычисляет элементы одной строки)
    std::vector<int> matrix_8(matrix_size * matrix_size);
    FillRandom(matrix_8, 0, 8);
    std::vector<int> matrix_9(matrix_size * matrix_size);
    FillRandom(matrix_9, 0, 8);
    timer.Start();
    auto matrix_10 = MultiplyByRow(matrix_8, matrix_9, matrix_size);
    timer.Stop();
    std::cout << "Function: MultiplyByRow" << std::endl;
    std::cout << "Elapsed: " << timer.Elapsed() << " ms" << std::endl << std::endl;
    //Print(matrix_10, matrix_size);

    return 0;
}