#include <MiniSim.hpp>

#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace minisim {
    ALWAYS_INLINE uint32_t position_to_cell_idx(uint16_t x, uint16_t y) noexcept {
        x = x >> 2;
        y = y >> 2;
        return CELL_BY_ROW * y + x;
    }

    ALWAYS_INLINE Position sub_pos(const Position& A, const Position& B) {
        const int16_t x = A.x - B.x;
        const int16_t y = A.y - B.y;
        return {x, y};
    }

    ALWAYS_INLINE Velocity sub_vel(const Velocity& A, const Velocity& B) {
        const int8_t dx = A.dx - B.dx;
        const int8_t dy = A.dy - B.dy;
        return {dx, dy};
    }

    ALWAYS_INLINE Position add_pos(Position& A, Position& B) {
        int16_t x = A.x + B.x;
        int16_t y = A.y + B.y;
        return {x, y};
    }

    ALWAYS_INLINE uint32_t get_dist_sqr(const Position& C) {
        return C.x * C.x + C.y * C.y; // C = A - B | C^2 =
    }

    bool MiniSim::initialize(arena_t &arena) {
        constexpr size_t size_data = N_PARTICLES * alignof(Data);
        constexpr size_t size_pos = N_PARTICLES * alignof(Position);
        constexpr size_t size_vel = N_PARTICLES * alignof(Velocity);
        constexpr size_t size_cell_heads = N_PARTICLES * sizeof(uint32_t);
        constexpr size_t size_bitset = N_CHUNKS * sizeof(uint64_t);
        constexpr size_t size_tmp = size_data + size_pos + size_vel;
        constexpr size_t total_size = size_data + size_pos + size_vel + size_tmp + size_cell_heads + size_bitset + sizeof(MiniSim);

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
        particles_.cells_head = static_cast<uint32_t *>(arena_allocate_aligned(&arena, size_cell_heads, sizeof(uint32_t)));
        particles_.active_cells = static_cast<uint64_t *>(arena_allocate_aligned(&arena, size_bitset, sizeof(uint64_t)));
        if (!particles_.datas || !particles_.pos || !particles_.vel || !particles_.tmp_datas || !particles_.tmp_pos || !particles_.tmp_vel || !particles_.cells_head || !particles_.active_cells) [[unlikely]] {
            std::cerr << "[Error] nullptr on components" << std::endl;
            return false;
        }

        std::memset(particles_.vel, 0, total_size - sizeof(MiniSim));
        return true;
    }

    ALWAYS_INLINE void MiniSim::count_sort() {
        constexpr uint32_t max_val = N_CELLS;
        uint32_t *count = particles_.cells_head; // using cells_head as tmp since we will rewrite it just after

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

        memset(particles_.cells_head, 0, N_CELLS * sizeof(int32_t));
        memset(particles_.active_cells, 0, N_CHUNKS * sizeof(uint32_t));

        uint32_t j = 0;
        while (j < N_PARTICLES) {
            uint32_t curr_cell_id = particles_.datas[j].cell_id;
            particles_.cells_head[curr_cell_id] = j;
            particles_.active_cells[curr_cell_id >> 6] |= 1ULL << (curr_cell_id & 63);
            while (j < N_PARTICLES && particles_.datas[j].cell_id == curr_cell_id) {
                particles_.datas[j] = particles_.tmp_datas[j];
                particles_.pos[j] = particles_.tmp_pos[j];
                particles_.vel[j] = particles_.tmp_vel[j];
                j++;
            }
        }
    }

    void MiniSim::update() {
        for (size_t i = 0; i < N_PARTICLES; ++i) { // pos += vec
            particles_.pos[i].x += particles_.vel[i].dx;
            particles_.pos[i].x = std::clamp<int16_t>(particles_.pos[i].x, 0, BORDER_W);
            particles_.vel[i].dx *= (particles_.pos[i].x == 0 || particles_.pos[i].x - 1000 == 0) ? -0.80 : 1;

            particles_.pos[i].y += particles_.vel[i].dy;
            particles_.pos[i].y = std::clamp<int16_t>(particles_.pos[i].y, 0, BORDER_H);
            particles_.vel[i].dy *= (particles_.pos[i].y == 0 || particles_.pos[i].y - 1000 == 0) ? -0.80 : 1;

            particles_.datas[i].cell_id = position_to_cell_idx(particles_.pos[i].x, particles_.pos[i].y);
            particles_.cells_head[i] = 0;
        }

        count_sort(); // sort
        for (int32_t chunk = 0; chunk < N_CHUNKS; ++chunk) {
            uint64_t word = particles_.active_cells[chunk];
            while (word != 0) {
                const uint8_t bit = std::countl_zero(word);
                const size_t idx = chunk * 64 + bit;
                const uint32_t curr_cell = particles_.cells_head[idx];
                size_t size = join_cells(curr_cell);

                for (size_t j = 0; j < size; ++j) {
                    Position& A = particles_.tmp_pos[j];
                    Velocity& A_vel = particles_.tmp_vel[j];
                    for (size_t k = j + 1; k < size; ++k) {
                        Position& B = particles_.tmp_pos[k];
                        Velocity& B_vel = particles_.tmp_vel[k];

                        Position N;
                        N = sub_pos(B, A); // normal unit vector
                        uint32_t dist_sqr = get_dist_sqr(N);
                        const auto overlap = (dist_sqr < 16);

                        auto dist = static_cast<uint32_t>(std::sqrt(dist_sqr));
                        dist = std::max(dist, static_cast<uint32_t>(1));
                        int16_t depth = 4 - dist; // (ra + rb) = 2 + 2 = 4

                        N.x = (N.x / dist) * overlap; // check if the circles overlaps, if overlap == 0 all the beyond operations will result to zero = no update
                        N.y = (N.y / dist) * overlap;

                        uint16_t p_x = (N.x * depth) >> 1; // penetration vector
                        uint16_t p_y = (N.y * depth) >> 1;

                        A.x += p_x;
                        A.y += p_y;
                        B.x -= p_x;
                        B.y -= p_y;

                        Velocity V_n;
                        V_n = sub_vel(A_vel, B_vel);
                        V_n.dx *= N.x;
                        V_n.dy *= N.y;

                        uint8_t bias = depth >> 2; // assuming delta t = 1 and beta = 0.25
                        bias = bias >> 1;

                        const int16_t impulse = -(V_n.dx + V_n.dy) + bias; // assuming e=1 for elastic rebound

                        A_vel.dx += impulse * N.x;
                        A_vel.dy += impulse * N.y;
                        B_vel.dx -= impulse * N.x;
                        B_vel.dy -= impulse * N.y;
                    }
                }

                while (size > 0) {
                    --size;
                    uint32_t i = particles_.tmp_datas[size].idx;
                    particles_.datas[i] = particles_.tmp_datas[size];
                    particles_.pos[i] = particles_.tmp_pos[size];
                    particles_.vel[i] = particles_.tmp_vel[size];
                }

                word &= (word - 1);
            }
        }
    }

    ALWAYS_INLINE void MiniSim::copy(uint32_t cell_id, size_t& size) {
        uint32_t idx = particles_.cells_head[cell_id];
        while (particles_.datas[idx].cell_id == cell_id) {
            particles_.datas[idx].idx = idx;
            particles_.tmp_datas[size] = particles_.datas[idx];
            particles_.tmp_pos[size] = particles_.pos[idx];
            particles_.tmp_vel[size] = particles_.vel[idx];
            ++size;
            ++idx;
        }
    };

    // method to join contiguous particle in board into contiguous array (regroup by cell pos and vel into tmp_pos and tmp_vel) for a cell
    ALWAYS_INLINE size_t MiniSim::join_cells(uint32_t cell_id) {
        const uint32_t col = cell_id % CELL_BY_ROW;
        const uint32_t row = cell_id / CELL_BY_ROW;
        size_t size = 0;

        uint32_t r_cell_idx = cell_id + 1; // right
        uint32_t b_cell_idx = cell_id + CELL_BY_ROW; // bottom
        uint32_t rb_cell_idx = cell_id + CELL_BY_ROW + 1; // right bottom
        uint32_t lb_cell_idx = cell_id + CELL_BY_ROW - 1; // left bottom

        auto check = [&](uint32_t curr_cell, bool cond){
            uint8_t cell_valid = (particles_.active_cells[curr_cell >> 6] >> (curr_cell & 63) & 1ULL) & 1;
            cell_valid &= cond;
            return cell_valid;
        };

        uint8_t r_valid = check(r_cell_idx, col < CELL_BY_ROW - 1);
        uint8_t b_valid = check(r_cell_idx, row < CELL_BY_ROW - 1);
        uint8_t rb_valid = check(r_cell_idx, col < CELL_BY_ROW - 1 && row < CELL_BY_ROW - 1);
        uint8_t lb_valid = check(r_cell_idx, col > 0 && row < CELL_BY_ROW - 1);

        if (r_valid)
            copy(r_cell_idx, size);
        if (b_valid)
            copy(b_cell_idx, size);
        if (rb_valid)
            copy(rb_cell_idx, size);
        if (lb_valid)
            copy(lb_cell_idx, size);

        return size;
        // 250 = MAP_WIDTH / CELL_WIDTH
        // [pos[cellhead[n - 251], n - 250, n - 249]
        // [n - 1,   n,       n + 1]
        // [n + 251, n + 250, n + 249]
    }

}

