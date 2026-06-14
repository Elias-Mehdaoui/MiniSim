#pragma once

extern "C" {
#include <arena_allocator.h>
}

#define ALWAYS_INLINE __attribute__((always_inline)) inline

#define BORDER_H 1000
#define BORDER_W 1000
#define N_PARTICLES 100'000
#define CELL_SIDE 4
#define CELL_BY_ROW (BORDER_H / CELL_SIDE)
#define N_CELLS (CELL_BY_ROW * CELL_BY_ROW)
#define N_CHUNKS ((N_CELLS >> 6) + 1) // 62500 bits / 64 = 976,5


namespace minisim {

    struct alignas(4) Position {
        int16_t x;
        int16_t y;
    };

    struct alignas(2) Velocity {
        int8_t dx;
        int8_t dy;
    };

    struct alignas(8) Data {
        uint32_t idx; // BORDER_H * y + x; pos[datas.idx]
        uint32_t cell_id;
    };

    struct alignas(64) Particles {
        Velocity *__restrict vel{nullptr}; // 100'000 * 2 ≈ 200KB
        Velocity *__restrict tmp_vel{nullptr}; // 100'000 * 2 ≈ 200KB
        Position *__restrict pos{nullptr}; // 100'000 * 4 ≈ 400KB
        Position *__restrict tmp_pos{nullptr}; // 100'000 * 4 ≈ 400KB
        Data *__restrict datas{nullptr}; // 100'000 * 8 ≈ 800KB
        Data *__restrict tmp_datas{nullptr}; // 100'000 * 8 ≈ 800KB
        uint32_t *__restrict cells_head{nullptr}; // N_CELLS * 4 ≈ 250KB | Index of cells head in arrays
        uint64_t *__restrict active_cells{nullptr}; // N_CHUNKS *  8 = 977 * 8 ≈ 6KB | bit mapping cells head index
    };

    class alignas(64) MiniSim {
        // int32_t grid_[BORDER_H * BORDER_W]{-1}; // 1'000'000 * 4 ≈ 4MB
        Particles particles_{}; // 64 bytes
        // TOTAL ≈ 6,8 MB

    public:
        MiniSim() = default;
        ~MiniSim() = default;

        bool initialize(arena_t& arena);
        void update();
        ALWAYS_INLINE void count_sort();
        ALWAYS_INLINE void copy(uint32_t cell_id, size_t& size);
        ALWAYS_INLINE size_t join_cells(uint32_t cell_id);

        ALWAYS_INLINE Data *get_datas() {
            return particles_.datas;
        }

        ALWAYS_INLINE Position *get_pos() {
            return particles_.pos;
        }

        ALWAYS_INLINE Velocity *get_vel() {
            return particles_.vel;
        }
    };


}
