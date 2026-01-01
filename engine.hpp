#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iosfwd>
#include <memory>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace hex {

enum Player { EMPTY = 0, WHITE = 1, BLACK = 2 };

struct EngineConfig {
    int board_size = 11;
    int default_ms = 3000;
    int default_sims = 0;
    uint64_t seed = 0xC0FFEE;
    double cpuct = 1.5;
    double prior_temp = 1.0;
    double dirichlet_alpha = 0.15;
    double dirichlet_eps = 0.20;
    bool use_root_noise = true;
    bool use_transposition = true;
    int tt_max_nodes = 250000;
    int rollout_max_plies = 400;
    int rollout_candidates = 20;
    double w_path = 1.0, w_block = 0.8;
    double w_bridge = 1.2, w_ladder = 1.1, w_ladder_break = 1.0, w_ziggurat = 0.7;
    double w_center = 0.15, w_adj = 0.25;
};

class Engine {
public:
    explicit Engine(const EngineConfig& cfg = EngineConfig());
    void reset(int board_size = -1);
    int size() const;
    Player to_move() const;
    bool play_move(int idx);
    bool play_coord(const std::string& s);
    int genmove(int ms = 0, int sims = 0);
    std::string idx_to_coord(int idx) const;
    int coord_to_idx(const std::string& s) const;
    Player at(int idx) const;
    bool is_terminal(Player* winner = nullptr) const;
    void print(std::ostream& os) const;
    struct SearchStats { int iters = 0; int playouts = 0; double root_winrate = 0; };
    SearchStats last_stats() const;
private:
    struct Node;
    EngineConfig cfg_;
    int N_;
    std::vector<Player> board_;
    Player to_move_;
    uint64_t zobrist_hash_;
    std::vector<uint64_t> zobrist_board_;
    uint64_t zobrist_side_;
    mutable std::mt19937_64 rng_;
    // union-find per player
    struct UF { std::vector<int> p, r; void init(int n); int find(int); int find_const(int) const; void unite(int,int); bool connected(int,int) const; };
    UF uf_w_, uf_b_;
    int virt_w_top_, virt_w_bot_, virt_b_left_, virt_b_right_;
    // helpers
    bool on_board(int r, int c) const;
    int idx(int r, int c) const;
    std::pair<int,int> rc(int index) const;
    void update_uf(int pos, Player pl);
    void rebuild_uf();
    double adjacency_bonus(int pos, Player pl) const;
    double center_bias(int pos) const;
    double bridge_bonus(int pos, Player pl) const;
    double ladder_bonus(int pos, Player pl) const;
    double ladder_breaker_bonus(int pos, Player pl) const;
    double ziggurat_bonus(int pos, Player pl) const;
    struct PathInfo { std::vector<int> dist_self, dist_opp; int best_self=0, best_opp=0; };
    PathInfo compute_paths(Player pl) const;
    double path_improve(int pos, Player pl, const PathInfo& pi) const;
    double block_opp(int pos, Player pl, const PathInfo& pi) const;
    double heuristic_value(Player pl) const;
    double value_estimate(Player pl) const;
    std::vector<int> legal_moves() const;
    // MCTS
    struct Edge { int move=-1; Node* child=nullptr; double P=0; int N=0; double W=0; double Q=0; };
    struct Node {
        uint64_t hash=0; Player player=EMPTY; bool expanded=false; bool terminal=false; Player winner=EMPTY;
        std::vector<Edge> edges; int N=0; double W=0; double Q=0; double prior_v=0; // prior_v used during backprop
    };
    std::vector<std::unique_ptr<Node>> pool_;
    std::unordered_map<uint64_t, Node*> tt_;
    Node* root_ = nullptr;
    SearchStats last_stats_;
    Node* new_node();
    Node* get_node(uint64_t h);
    void maybe_evict_tt();
    void apply_dirichlet(Node* n);
    double rollout(Player pl);
    int rollout_move(Player pl);
    double policy_prior(Player pl, const PathInfo& pi, std::vector<int>& moves, std::vector<double>& priors);
    double puct_score(const Node* parent, const Edge& e) const;
    double simulate(Node* node, int depth);
};

} // namespace hex
