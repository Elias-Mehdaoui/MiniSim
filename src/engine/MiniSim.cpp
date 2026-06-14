#include <MiniSim.hpp>

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace minisim {
    ALWAYS_INLINE int32_t position_to_cell_idx(uint16_t x, uint16_t y) noexcept {
        x = x >> 2;
        y = y >> 2;
        return CELL_BY_ROW * y + x;
    }

    ALWAYS_INLINE Position sub_pos(const Position& A, const Position& B) {
        const int16_t x = A.x - B.x;
        const int16_t y = A.y - B.y;
        return {x, y};
    }

    ALWAYS_INLINE void normalize(Position& C, const uint32_t dist) {
        C.x /= dist;
        C.y /= dist;
    }

    ALWAYS_INLINE Position add_pos(Position& A, Position& B) {
        int16_t x = A.x + B.x;
        int16_t y = A.y + B.y;
        return {x, y};
    }

    ALWAYS_INLINE uint32_t get_dist_sqr(const Position& C) {
        return C.x * C.x + C.y * C.y; // C = A - B | C^2 =
    }

    ALWAYS_INLINE int32_t MiniSim::get_next_cell(int32_t cell_id) {
        ++cell_id;
        while (cell_id < N_CELLS && particles_.cells_head[cell_id] < 0) {
            cell_id++;
        }
        if (cell_id >= N_CELLS) [[unlikely]] {
            return -1;
        }
        return cell_id;
    }

    ALWAYS_INLINE void MiniSim::count_sort() {
        constexpr uint32_t max_val = N_CELLS;
        auto *count = reinterpret_cast<uint32_t *>(particles_.cells_head); // using cells_head as tmp since we will rewrite it just after

        for (uint32_t i = 0; i < N_PARTICLES; ++i) {
            uint32_t idx = particles_.datas[i].cell_id;
            count[idx] += 1;
        }

        for (uint32_t i = 1; i <= max_val; ++i) {
            count[i] += count[i - 1];
        }

        for (int32_t i = N_PARTICLES - 1; i >= 0; --i) {
            uint32_t idx = --count[particles_.datas[i].cell_id];

            particles_.tmp_datas[idx] = particles_.datas[i];
            particles_.tmp_vel[idx] = particles_.vel[i];
            particles_.tmp_pos[idx] = particles_.pos[i];
        }

        memset(particles_.cells_head, -1, N_CELLS * sizeof(int32_t));
        uint32_t j = 0;
        while (j < N_PARTICLES) {
            int32_t curr_cell_id = particles_.datas[j].cell_id;
            particles_.cells_head[curr_cell_id] = j;
            while (j < N_PARTICLES && particles_.datas[j].cell_id == curr_cell_id) {
                particles_.datas[j] = particles_.tmp_datas[j];
                particles_.pos[j] = particles_.tmp_pos[j];
                particles_.vel[j] = particles_.tmp_vel[j];
                j++;
            }
        }

    }

    bool MiniSim::initialize(arena_t &arena) {
        constexpr size_t size_data = N_PARTICLES * alignof(Data);
        constexpr size_t size_pos = N_PARTICLES * alignof(Position);
        constexpr size_t size_vel = N_PARTICLES * alignof(Velocity);
        constexpr size_t size_cell_heads = N_PARTICLES * sizeof(int32_t);
        constexpr size_t size_tmp = size_data + size_pos + size_vel;
        constexpr size_t total_size = size_data + size_pos + size_vel + size_tmp + size_cell_heads + sizeof(MiniSim);

        if (arena_get_capacity(&arena) < total_size) [[unlikely]] {
            std::cerr << "[Error] arena must be greater than or equal to " << total_size << std::endl;
            return false;
        }

        particles_.vel = static_cast<Velocity *>(arena_allocate_aligned(&arena, size_vel, alignof(Velocity)));
        particles_.tmp_vel = static_cast<Velocity *>(arena_allocate_aligned(&arena, size_vel, alignof(Velocity)));
        particles_.pos = static_cast<Position *>(arena_allocate_aligned(&arena, size_pos, alignof(Position)));
        particles_.tmp_pos = static_cast<Position *>(arena_allocate_aligned(&arena, size_pos, alignof(Position)));
        particles_.datas = static_cast<Data *>(arena_allocate_aligned(&arena, size_data, alignof(Data)));
        particles_.tmp_datas = static_cast<Data *>(arena_allocate_aligned(&arena, size_data, alignof(Data)));
        particles_.cells_head = static_cast<int32_t *>(arena_allocate_aligned(&arena, size_cell_heads, sizeof(int32_t)));
        if (!particles_.datas || !particles_.pos || !particles_.vel || !particles_.tmp_datas || !particles_.tmp_pos || !particles_.tmp_vel || !particles_.cells_head) [[unlikely]] {
            std::cerr << "[Error] nullptr on components" << std::endl;
            return false;
        }

        std::memset(particles_.vel, 0, total_size - sizeof(MiniSim));
        return true;
    }

    void MiniSim::update() {
        for (size_t i = 0; i < N_PARTICLES; ++i) { // pos += vec
            particles_.pos[i].x += particles_.vel[i].dx;
            particles_.pos[i].x = std::clamp<int16_t>(particles_.pos[i].x, 0, BORDER_W);
            particles_.vel[i].dx *= particles_.pos[i].x == 0 || !(particles_.pos[i].x - 1000) ? -0.80 : 1;

            particles_.pos[i].y += particles_.vel[i].dy;
            particles_.pos[i].y = std::clamp<int16_t>(particles_.pos[i].y, 0, BORDER_H);
            particles_.vel[i].dy *= particles_.pos[i].y == 0 || !(particles_.pos[i].y - 1000) ? -0.80 : 1;

            particles_.datas[i].cell_id = position_to_cell_idx(particles_.pos[i].x, particles_.pos[i].y);
            particles_.cells_head[i] = 0;
        }

        count_sort(); // sort
        int32_t curr_cell = 0;
        while (curr_cell != -1) {
            size_t size = join_cells(curr_cell);
            for (size_t j = 0; j < size; ++j) {
                Position& A = particles_.tmp_pos[j];
                Velocity& A_vel = particles_.tmp_vel[j];
                for (size_t k = j + 1; k < size; ++k) {
                    Position& B = particles_.tmp_pos[k];
                    Velocity& B_vel = particles_.tmp_vel[k];

                    Position C = sub_pos(A, B);
                    uint32_t dist_sqr = get_dist_sqr(C);
                    bool overlap = (dist_sqr < 16);
                    auto dist = static_cast<uint32_t>(std::sqrt(dist_sqr));
                    dist = std::max(dist, static_cast<uint32_t>(1));

                    Position midpoint{};
                    midpoint.x = (A.x + B.x) >> 1;
                    midpoint.y = (A.y + B.y) >> 1;

                    normalize(C, dist);
                    A.x = overlap ? midpoint.x + 2 * C.x : A.x;
                    A.y = overlap ? midpoint.y + 2 * C.y : A.y;
                    B.x = overlap ? midpoint.x + 2 * -(C.x) : B.x;
                    B.y = overlap ? midpoint.y + 2 * -(C.y) : B.y;

                    A_vel.dx = overlap ? B_vel.dx : A_vel.dx;
                    A_vel.dy = overlap ? B_vel.dy : A_vel.dy;
                    B_vel.dx = overlap ? A_vel.dx : B_vel.dx;
                    B_vel.dy = overlap ? A_vel.dy : B_vel.dy;
                }

            }
            for (size_t j = 0; j < size; ++j) {
                uint32_t idx = particles_.tmp_datas[j].idx;
                particles_.datas[idx] = particles_.tmp_datas[j];
                particles_.pos[idx] = particles_.tmp_pos[j];
                particles_.vel[idx] = particles_.tmp_vel[j];
            }

            curr_cell = get_next_cell(curr_cell);
        }

        // for (int i = 0; i < N_PARTICLES; ++i) {
        //     grid_[BORDER_W * particles_.pos[i].y + particles_.pos[i].x] = i;
        // }
    }

    ALWAYS_INLINE size_t MiniSim::copy(int32_t cell_id, size_t size) {
        int32_t idx = particles_.cells_head[cell_id];
        if (idx == -1)
            return 0;
        while (particles_.datas[idx].cell_id == cell_id) {
            particles_.datas[idx].idx = idx;
            particles_.tmp_datas[size] = particles_.datas[idx];
            particles_.tmp_pos[size] = particles_.pos[idx];
            particles_.tmp_vel[size] = particles_.vel[idx];
            ++size;
            ++idx;
        }
        return size;
    };

    // method to join contiguous particle in board into contiguous array (regroup by cell pos and vel into tmp_pos and tmp_vel) for a cell
    ALWAYS_INLINE size_t MiniSim::join_cells(uint32_t cell_id) {
        const uint32_t col = cell_id % CELL_BY_ROW;
        const uint32_t row = cell_id / CELL_BY_ROW;
        size_t size = 0;

        if (col < CELL_BY_ROW - 1) {
            size = copy(+1, size);
        }
        if (row < CELL_BY_ROW - 1) {
            size = copy(CELL_BY_ROW, size);
        }
        if (col > 0 && row < CELL_BY_ROW - 1) {
            size = copy(CELL_BY_ROW - 1, size);
        }
        if (col < CELL_BY_ROW - 1 && row < CELL_BY_ROW - 1) {
            size = copy(CELL_BY_ROW + 1, size);
        }

        return size;
        // 250 = MAP_WIDTH / CELL_WIDTH
        // [pos[cellhead[n - 251], n - 250, n - 249]
        // [n - 1,   n,       n + 1]
        // [n + 251, n + 250, n + 249]
    }

}

