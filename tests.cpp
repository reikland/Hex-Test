#include "engine.hpp"
#include <cassert>
#include <iostream>

using namespace hex;

static void test_coord_parsing(){
    Engine e; e.reset(11);
    assert(e.coord_to_idx("a1") >= 0);
    assert(e.coord_to_idx("k11") >= 0);
    assert(e.coord_to_idx("A5") >= 0);

    assert(e.coord_to_idx("") == -1);
    assert(e.coord_to_idx("1") == -1);
    assert(e.coord_to_idx("@1") == -1);
    assert(e.coord_to_idx("a0") == -1);
    assert(e.coord_to_idx("a12") == -1);
    assert(e.coord_to_idx("aa1") == -1);
    assert(e.coord_to_idx("a1b") == -1);
}

static void test_play_legality(){
    Engine e; e.reset(5);
    int idx = e.coord_to_idx("b2");
    assert(idx >= 0);
    assert(e.play_move(idx));
    assert(!e.play_move(idx));
}

int main(){
    test_coord_parsing();
    test_play_legality();
    std::cout << "All tests passed" << std::endl;
    return 0;
}
