#include <gtest/gtest.h>

#include <MiniSim.hpp>

extern "C" {
#include <arena_allocator.h>
}

// using namespace minisim;
//
// class MiniSimTest : public testing::Test {
// protected:
//     arena_t *arena_{nullptr};
//     MiniSim *minisim_memory_{nullptr};
//
//     void SetUp() override {
//         size_t size_particles = N_PARTICLES * alignof(Particle) ;
//         size_t size_cells = N_CELLS * alignof(Cell);
//         size_t total_size = size_particles + size_cells + alignof(MiniSim);
//
//         arena_ = arena_create(total_size);
//         minisim_memory_ = static_cast<MiniSim *>(arena_allocate_aligned(arena_, sizeof(MiniSim), alignof(MiniSim)));
//     }
//
//     void TearDown() override {
//         arena_destroy(arena_);
//     }
// };
//
// TEST_F(MiniSimTest, TestSort) {
//     bool success = minisim_memory_->initialize(*arena_);
//     ASSERT_NE(success, false);
//
//     auto *particles = minisim_memory_->get_particles();

// }