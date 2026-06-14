#include <chrono>
#include <iostream>
#include <new>
#include <random>

#include <MiniSim.hpp>

#include "arena_allocator.h"

using namespace minisim;

int main() {
	std::ios::sync_with_stdio(false);
	std::cin.tie(nullptr);

	constexpr size_t size_data = N_PARTICLES * alignof(Data);
	constexpr size_t size_pos = N_PARTICLES * alignof(Position);
	constexpr size_t size_vel = N_PARTICLES * alignof(Velocity);
	constexpr size_t size_cell_heads = N_PARTICLES * sizeof(int32_t);
	constexpr size_t size_tmp = size_data + size_pos + size_vel;
	constexpr size_t total_size = size_data + size_pos + size_vel + size_tmp + size_cell_heads + sizeof(MiniSim);

	arena_t *arena = arena_create(total_size);
	if (!arena) [[unlikely]] {
		std::cerr << "Failed to create arena.\n";
		return EXIT_FAILURE;
	}

	void *minisim_p = arena_allocate_aligned(arena, sizeof(MiniSim), alignof(MiniSim));
	if (!minisim_p) [[unlikely]] {
		std::cout << "Failed to allocate minisim memory.\n";
		arena_destroy(arena);
		return EXIT_FAILURE;
	}

	MiniSim *minisim = new (minisim_p) MiniSim();
	if (!minisim->initialize(*arena)) [[unlikely]] {
		std::cout << "MiniSim initialization failed.\n";
		arena_destroy(arena);
		return EXIT_FAILURE;
	}

	auto *__restrict position = minisim->get_pos();
	auto *__restrict velocity = minisim->get_vel();
	auto *__restrict datas = minisim->get_datas();

	std::default_random_engine generator;
	std::uniform_int_distribution<int16_t> randPosition(250, 750);
	std::uniform_int_distribution<int8_t> randVelocity(-10, 10);

	for (size_t i = 0; i < N_PARTICLES; ++i) {
		position[i].x = randPosition(generator);
		position[i].y = randPosition(generator);
		velocity[i].dx = randVelocity(generator);
		velocity[i].dy = randVelocity(generator);
		datas[i].idx = i;
	}

	constexpr int iterations = 1'000; // 1'000 iterations
	const auto start_time = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < iterations; ++i) {
		minisim->update();
	}

	const auto end_time = std::chrono::high_resolution_clock::now();
	const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	const float update_per_s = static_cast<float>(iterations) / static_cast<float>(duration_ms);

	std::cout << "[STATS] Updated " << iterations << " times in " << duration_ms << " ms." << std::endl;
	std::cout << "[STATS] " << update_per_s << " update/ms " << std::endl;

	arena_destroy(arena);
	return EXIT_SUCCESS;
}