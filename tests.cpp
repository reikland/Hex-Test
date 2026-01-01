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

static void test_genmove_on_nearly_full_board(){
    Engine e; e.reset(2);
    // Arrange a position with one empty cell and no winner yet.
    assert(e.play_coord("a1")); // Black
    assert(e.play_coord("b1")); // White
    assert(e.play_coord("a2")); // Black

    int mv = e.genmove(10, 1);
    assert(mv == e.coord_to_idx("b2"));
    assert(e.at(mv) != EMPTY);
}

static void test_genmove_when_no_moves_available(){
    Engine e; e.reset(2);
    assert(e.play_coord("a1"));
    assert(e.play_coord("b1"));
    assert(e.play_coord("a2"));
    assert(e.play_coord("b2"));

    int mv = e.genmove(10, 1);
    assert(mv == -1);
}

int main(){
    test_coord_parsing();
    test_play_legality();
    test_genmove_on_nearly_full_board();
    test_genmove_when_no_moves_available();
    std::cout << "All tests passed" << std::endl;
    return 0;
}
