#include "engine.hpp"

namespace hex {
namespace {
constexpr int INF = 1e9;
uint64_t splitmix64(uint64_t x){
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
}

void Engine::UF::init(int n){ p.resize(n); r.assign(n,0); std::iota(p.begin(), p.end(),0); hist.clear();}
int Engine::UF::find(int x) const { while(p[x]!=x) x=p[x]; return x; }
int Engine::UF::find_const_impl(int x) const { while(p[x]!=x) x=p[x]; return x; }
bool Engine::UF::unite(int a,int b){ a=find(a); b=find(b); if(a==b){ hist.push_back({-1,0,-1}); return false; } if(r[a]<r[b]) std::swap(a,b); hist.push_back({b, r[a]==r[b], a}); p[b]=a; if(r[a]==r[b]) r[a]++; return true; }
size_t Engine::UF::snapshot() const { return hist.size(); }
void Engine::UF::rollback(size_t snap){ while(hist.size()>snap){ auto ch=hist.back(); hist.pop_back(); if(ch.b==-1) continue; if(ch.ra) r[ch.a]--; p[ch.b]=ch.b; } }
bool Engine::UF::connected(int a,int b) const { return find_const_impl(a)==find_const_impl(b); }

Engine::Engine(const EngineConfig& cfg):cfg_(cfg),N_(cfg.board_size),to_move_(BLACK),zobrist_hash_(0),rng_(cfg.seed){
    reset(N_);
}

void Engine::reset(int board_size){
    if(board_size>0) N_=board_size; else N_=cfg_.board_size;
    board_.assign(N_*N_, EMPTY);
    to_move_=BLACK;
    size_t table = N_*N_*2;
    zobrist_board_.resize(table);
    uint64_t seed = cfg_.seed+123;
    for(size_t i=0;i<table;i++) zobrist_board_[i]=splitmix64(seed+i);
    zobrist_side_=splitmix64(seed+table+1);
    zobrist_hash_=0;
    uf_w_.init(N_*N_+2); uf_b_.init(N_*N_+2);
    virt_w_top_=N_*N_; virt_w_bot_=N_*N_+1;
    virt_b_left_=N_*N_; virt_b_right_=N_*N_+1;
    pool_.clear(); tt_.clear(); root_=nullptr; last_stats_=SearchStats{};
}

int Engine::size() const { return N_; }
Player Engine::to_move() const { return to_move_; }

bool Engine::on_board(int r,int c) const { return r>=0 && c>=0 && r<N_ && c<N_; }
int Engine::idx(int r,int c) const { return r*N_+c; }
std::pair<int,int> Engine::rc(int index) const { return {index/N_, index%N_}; }

Player Engine::at(int i) const { return board_[i]; }

void Engine::update_uf(int pos, Player pl){
    auto [r,c]=rc(pos);
    UF &uf = (pl==WHITE)?uf_w_:uf_b_;
    int v1=-1,v2=-1;
    if(pl==WHITE){ v1=virt_w_top_; v2=virt_w_bot_; if(r==0) uf.unite(pos,v1); if(r==N_-1) uf.unite(pos,v2); }
    else { v1=virt_b_left_; v2=virt_b_right_; if(c==0) uf.unite(pos,v1); if(c==N_-1) uf.unite(pos,v2); }
    static const int dr[6]={-1,-1,0,0,1,1};
    static const int dc[6]={0,1,-1,1,-1,0};
    for(int k=0;k<6;k++){
        int nr=r+dr[k], nc=c+dc[k];
        if(on_board(nr,nc)){
            int ni=idx(nr,nc);
            if(board_[ni]==pl) uf.unite(pos,ni);
        }
    }
}

bool Engine::play_move(int pos){
    if(pos<0 || pos>=N_*N_ || board_[pos]!=EMPTY) return false;
    Player pl=to_move_;
    board_[pos]=pl;
    zobrist_hash_ ^= zobrist_board_[pos*2 + (pl==WHITE?0:1)];
    zobrist_hash_ ^= zobrist_side_;
    to_move_ = (pl==WHITE?BLACK:WHITE);
    update_uf(pos,pl);
    return true;
}

void Engine::make_move(int pos, MoveHist& h){
    h.pos=pos; h.pl=to_move_; h.hash=zobrist_hash_;
    h.snap_w=uf_w_.snapshot(); h.snap_b=uf_b_.snapshot();
    play_move(pos);
}

void Engine::undo_move(const MoveHist& h){
    board_[h.pos]=EMPTY; to_move_=h.pl; zobrist_hash_=h.hash;
    uf_w_.rollback(h.snap_w); uf_b_.rollback(h.snap_b);
}

bool Engine::play_coord(const std::string& s){ int id=coord_to_idx(s); if(id<0) return false; return play_move(id);} 

std::string Engine::idx_to_coord(int id) const{
    auto [r,c]=rc(id);
    std::string res; res.push_back('a'+c); res+=std::to_string(r+1); return res;
}

int Engine::coord_to_idx(const std::string& s) const{
    if(s.size()<2) return -1;
    int c=std::tolower(s[0])-'a';
    int r=0; bool digit=false;
    for(size_t i=1;i<s.size();++i){ if(!std::isdigit(static_cast<unsigned char>(s[i]))) return -1; digit=true; r = r*10 + (s[i]-'0'); }
    if(!digit) return -1; r-=1;
    if(!on_board(r,c)) return -1; return idx(r,c);
}

bool Engine::is_terminal(Player* winner) const{
    bool w = uf_w_.connected(virt_w_top_, virt_w_bot_);
    bool b = uf_b_.connected(virt_b_left_, virt_b_right_);
    if(winner){ if(w) *winner=WHITE; else if(b) *winner=BLACK; else *winner=EMPTY; }
    return w||b;
}

void Engine::print(std::ostream& os) const{
    os << "  "; for(int c=0;c<N_;++c) os << char('a'+c) << ' '; os << '\n';
    for(int r=0;r<N_;++r){
        os << std::setw(r+2) << std::setfill(' ') << r+1 << ' ';
        for(int c=0;c<N_;++c){ char ch='.'; if(board_[idx(r,c)]==WHITE) ch='W'; else if(board_[idx(r,c)]==BLACK) ch='B'; os<<ch<<' '; }
        os << r+1 << '\n';
    }
    os << "  "; for(int c=0;c<N_;++c) os << char('a'+c) << ' '; os << '\n';
}

Engine::SearchStats Engine::last_stats() const { return last_stats_; }

// helper features

double Engine::adjacency_bonus(int pos, Player pl) const{
    static const int dr[6]={-1,-1,0,0,1,1};
    static const int dc[6]={0,1,-1,1,-1,0};
    auto [r,c]=rc(pos); int friendly=0, opp=0;
    for(int k=0;k<6;k++){
        int nr=r+dr[k], nc=c+dc[k]; if(on_board(nr,nc)){
            Player p=board_[idx(nr,nc)]; if(p==pl) friendly++; else if(p!=EMPTY) opp++; }
    }
    return friendly*0.6 + opp*0.1;
}

double Engine::center_bias(int pos) const{
    auto [r,c]=rc(pos); double cr=(N_-1)/2.0; double cc=(N_-1)/2.0; double d=std::hypot(r-cr,c-cc); double maxd=std::hypot(cr,cc); return 1.0 - d/(maxd+1e-6);
}

// pattern helpers
static const std::array<std::pair<int,int>,6> neigh{{{-1,0},{-1,1},{0,-1},{0,1},{1,-1},{1,0}}};

double Engine::bridge_bonus(int pos, Player pl) const{
    auto [r,c]=rc(pos); double score=0; // check two-step diagonals
    const std::array<std::array<std::pair<int,int>,2>,6> endpoints{{ {{{-1,0},{0,1}}}, {{{0,1},{1,0}}}, {{{1,0},{0,-1}}}, {{{0,-1},{-1,0}}}, {{{-1,1},{1,0}}}, {{{1,-1},{-1,0}}} }};
    Player opp = pl==WHITE?BLACK:WHITE;
    for(auto ep: endpoints){
        int r1=r+ep[0].first, c1=c+ep[0].second; int r2=r+ep[1].first, c2=c+ep[1].second;
        int rb=r+ep[0].first+ep[1].first, cb=c+ep[0].second+ep[1].second;
        if(on_board(r1,c1)&&on_board(r2,c2)){
            int id1=idx(r1,c1), id2=idx(r2,c2);
            bool other_bridge=on_board(rb,cb) && board_[idx(rb,cb)]==EMPTY;
            if(board_[id1]==pl && board_[id2]==pl && other_bridge) score += 1.2; // strong bridge completion
            else if(board_[id1]==pl && board_[id2]==pl) score += 0.8;
            else if(other_bridge && ((board_[id1]==pl && board_[id2]==EMPTY)||(board_[id2]==pl && board_[id1]==EMPTY))) score += 0.4;
            if(board_[id1]==opp && board_[id2]==opp && other_bridge) score += 0.5; // contest opponent bridge
        }
    }
    return score;
}

double Engine::ziggurat_bonus(int pos, Player pl) const{
    auto [r,c]=rc(pos); double s=0; // stair-step extension
    const std::array<std::array<std::pair<int,int>,2>,6> steps{{ {{{-1,0},{0,1}}}, {{{0,1},{1,0}}}, {{{1,0},{0,-1}}}, {{{0,-1},{-1,0}}}, {{{-1,1},{1,0}}}, {{{1,-1},{-1,0}}} }};
    for(auto &st: steps){
        int r1=r+st[0].first, c1=c+st[0].second; int r2=r+st[1].first, c2=c+st[1].second;
        if(on_board(r1,c1)&&on_board(r2,c2)){
            Player p1=board_[idx(r1,c1)], p2=board_[idx(r2,c2)];
            if(p1==pl && p2==pl) s+=1.0;
            else if((p1==pl||p2==pl)&&(p1==EMPTY||p2==EMPTY)) s+=0.3;
        }
    }
    return s;
}

double Engine::ladder_bonus(int pos, Player pl) const{
    auto [r,c]=rc(pos); double s=0; Player opp=pl==WHITE?BLACK:WHITE;
    for(int i=0;i<6;i++){
        auto d1=neigh[i]; auto d2=neigh[(i+1)%6];
        int r1=r+d1.first, c1=c+d1.second; int r2=r+d2.first, c2=c+d2.second;
        int re=r+d1.first+d2.first, ce=c+d1.second+d2.second;
        if(on_board(r1,c1)&&on_board(r2,c2)&&on_board(re,ce)){
            Player p1=board_[idx(r1,c1)], p2=board_[idx(r2,c2)];
            if(board_[idx(re,ce)]==EMPTY && ((p1==pl && p2==opp)||(p2==pl && p1==opp))) s+=0.6;
        }
    }
    if(pl==WHITE){ if(r==0||r==N_-1) s+=0.3; }
    else { if(c==0||c==N_-1) s+=0.3; }
    return s;
}

double Engine::ladder_breaker_bonus(int pos, Player pl) const{
    Player opp=pl==WHITE?BLACK:WHITE; auto [r,c]=rc(pos); double s=0;
    for(int i=0;i<6;i++){
        auto d1=neigh[i]; auto d2=neigh[(i+1)%6];
        int ro=r+d1.first, co=c+d1.second; int chase=r+d1.first+d2.first, chasec=c+d1.second+d2.second;
        if(on_board(ro,co)&&on_board(chase,chasec)){
            if(board_[idx(ro,co)]==opp && board_[idx(chase,chasec)]==opp && board_[pos]==EMPTY) s+=0.7;
        }
    }
    return s;
}

Engine::PathInfo Engine::compute_paths(Player pl) const{
    auto cell_cost=[&](Player p,int id){ if(board_[id]==p) return 0; if(board_[id]==EMPTY) return 1; return INF; };
    auto dijkstra=[&](Player p, bool source_side){
        std::vector<int> dist(N_*N_, INF);
        using PI=std::pair<int,int>;
        std::priority_queue<PI,std::vector<PI>,std::greater<PI>> pq;
        if(p==WHITE){
            if(source_side){ for(int c=0;c<N_;++c){ int id=idx(0,c); int w=cell_cost(p,id); dist[id]=w; pq.push({w,id}); } }
            else { for(int c=0;c<N_;++c){ int id=idx(N_-1,c); int w=cell_cost(p,id); dist[id]=w; pq.push({w,id}); } }
        } else {
            if(source_side){ for(int r=0;r<N_;++r){ int id=idx(r,0); int w=cell_cost(p,id); dist[id]=w; pq.push({w,id}); } }
            else { for(int r=0;r<N_;++r){ int id=idx(r,N_-1); int w=cell_cost(p,id); dist[id]=w; pq.push({w,id}); } }
        }
        while(!pq.empty()){
            auto [d,u]=pq.top(); pq.pop(); if(d!=dist[u]) continue; auto [ur,uc]=rc(u);
            for(auto n: neigh){ int vr=ur+n.first, vc=uc+n.second; if(!on_board(vr,vc)) continue; int v=idx(vr,vc); int cost=cell_cost(p,v); if(dist[v]>d+cost){ dist[v]=d+cost; pq.push({dist[v],v}); }}
        }
        return dist;
    };
    Player opp = pl==WHITE?BLACK:WHITE;
    auto ds_from = dijkstra(pl,true); auto ds_to = dijkstra(pl,false);
    auto do_from = dijkstra(opp,true); auto do_to = dijkstra(opp,false);
    PathInfo pi; pi.dist_self_from=ds_from; pi.dist_self_to=ds_to; pi.dist_opp_from=do_from; pi.dist_opp_to=do_to; pi.best_self=INF; pi.best_opp=INF;
    if(pl==WHITE){ for(int c=0;c<N_;++c){ pi.best_self=std::min(pi.best_self, ds_from[idx(N_-1,c)]); } }
    else { for(int r=0;r<N_;++r){ pi.best_self=std::min(pi.best_self, ds_from[idx(r,N_-1)]); } }
    if(opp==WHITE){ for(int c=0;c<N_;++c){ pi.best_opp=std::min(pi.best_opp, do_from[idx(N_-1,c)]); } }
    else { for(int r=0;r<N_;++r){ pi.best_opp=std::min(pi.best_opp, do_from[idx(r,N_-1)]); } }
    return pi;
}

double Engine::path_improve(int pos, Player pl, const PathInfo& pi) const{
    if(board_[pos]!=EMPTY) return 0; int cur=pi.best_self; int ds=pi.dist_self_from[pos]; int dt=pi.dist_self_to[pos]; if(ds>=INF||dt>=INF||cur>=INF) return 0; int cost=1; int through=ds+dt-cost; double d=cur-through; return d>0?d:0; }

double Engine::block_opp(int pos, Player pl, const PathInfo& pi) const{
    if(board_[pos]!=EMPTY) return 0; int cur=pi.best_opp; int ds=pi.dist_opp_from[pos]; int dt=pi.dist_opp_to[pos]; if(ds>=INF||dt>=INF||cur>=INF) return 0; int cost=1; int through=ds+dt-cost; double d=through-cur; return d>0?d:0; }

double Engine::heuristic_value(Player pl) const{
    PathInfo pi=compute_paths(pl); if(pi.best_self>=INF && pi.best_opp>=INF) return 0; int bs=pi.best_self>=INF?N_*4:pi.best_self; int bo=pi.best_opp>=INF?N_*4:pi.best_opp; double raw = (double)(bo - bs); return std::tanh(raw/2.0); }

double Engine::value_estimate(Player pl) const{ double h=heuristic_value(pl); // slight pattern term
    double pat=0; auto moves=legal_moves(); for(int m: moves){ pat = std::max(pat, adjacency_bonus(m,pl)*0.05); }
    double v=h+pat; if(v>1) v=1; if(v<-1) v=-1; return v; }

std::vector<int> Engine::legal_moves() const{
    std::vector<int> res; res.reserve(N_*N_);
    for(int i=0;i<N_*N_;++i) if(board_[i]==EMPTY) res.push_back(i);
    return res;
}

Engine::Node* Engine::new_node(){ pool_.push_back(std::make_unique<Node>()); return pool_.back().get(); }

Engine::Node* Engine::get_node(uint64_t h){ if(!cfg_.use_transposition) return nullptr; auto it=tt_.find(h); if(it!=tt_.end()) return it->second; return nullptr; }

void Engine::maybe_evict_tt(){ if((int)tt_.size()<=cfg_.tt_max_nodes) return; // simple eviction by removing random entries
    std::uniform_int_distribution<size_t> dist(0, tt_.size()-1); size_t nrem = tt_.size() - cfg_.tt_max_nodes;
    for(size_t k=0;k<nrem;k++){
        auto it=tt_.begin(); std::advance(it, dist(rng_) % tt_.size()); tt_.erase(it);
    }
}

void Engine::apply_dirichlet(Node* n){ if(n->edges.empty()) return; std::gamma_distribution<double> gamma(cfg_.dirichlet_alpha,1.0); std::vector<double> noise(n->edges.size()); double sum=0; for(double &v: noise){ v=gamma(rng_); sum+=v; } for(double &v: noise) v/=sum; for(size_t i=0;i<n->edges.size();++i){ n->edges[i].P = (1-cfg_.dirichlet_eps)*n->edges[i].P + cfg_.dirichlet_eps*noise[i]; } }

double Engine::policy_prior(Player pl, const PathInfo& pi, std::vector<int>& moves, std::vector<double>& priors){
    moves=legal_moves(); priors.clear(); priors.reserve(moves.size()); double maxs=-1e9; std::vector<double> scores;
    for(int m: moves){ double s=0; s+=cfg_.w_path*path_improve(m,pl,pi); s+=cfg_.w_block*block_opp(m,pl,pi); s+=cfg_.w_bridge*bridge_bonus(m,pl); s+=cfg_.w_ladder*ladder_bonus(m,pl); s+=cfg_.w_ladder_break*ladder_breaker_bonus(m,pl); s+=cfg_.w_ziggurat*ziggurat_bonus(m,pl); s+=cfg_.w_adj*adjacency_bonus(m,pl); s+=cfg_.w_center*center_bias(m); scores.push_back(s); maxs=std::max(maxs,s); }
    double sum=0; for(double s: scores){ double p=std::exp((s-maxs)/cfg_.prior_temp); priors.push_back(p); sum+=p; }
    for(double &p: priors) p/=sum+1e-12; return sum;
}

double Engine::puct_score(const Node* parent, const Edge& e) const{
    double pbias = cfg_.cpuct * e.P * std::sqrt(std::max(1,parent->N)) / (1 + e.N);
    return e.Q + pbias;
}

int Engine::rollout_move(Player pl){
    auto moves=legal_moves(); if(moves.empty()) return -1; std::vector<double> scores; scores.reserve(moves.size()); double maxs=-1e9;
    for(int m: moves){ double s=adjacency_bonus(m,pl)*0.9 + bridge_bonus(m,pl)*0.6 + center_bias(m)*0.2; scores.push_back(s); maxs=std::max(maxs,s); }
    std::vector<int> order(moves.size()); std::iota(order.begin(), order.end(),0);
    std::partial_sort(order.begin(), order.begin()+std::min(cfg_.rollout_candidates,(int)order.size()), order.end(), [&](int a,int b){ return scores[a]>scores[b]; });
    int k=std::min(cfg_.rollout_candidates,(int)order.size()); double sum=0; std::vector<double> probs(k);
    const double temp=0.8;
    for(int i=0;i<k;i++){ probs[i]=std::exp((scores[order[i]]-maxs)/temp); sum+=probs[i]; }
    std::uniform_real_distribution<double> dist(0,sum); double r=dist(rng_); for(int i=0;i<k;i++){ if(r<=probs[i]) return moves[order[i]]; r-=probs[i]; }
    return moves[order.back()];
}

double Engine::rollout(Player pl){
    last_stats_.playouts++;
    int plies=0; Player winner; std::vector<MoveHist> hist;
    while(plies<cfg_.rollout_max_plies){
        if(is_terminal(&winner)) break;
        int m=rollout_move(to_move_);
        if(m<0) break;
        MoveHist h; make_move(m,h); hist.push_back(h); plies++;
    }
    double val;
    if(is_terminal(&winner)) val = (winner==pl)?1:-1; else val = heuristic_value(pl);
    for(auto it=hist.rbegin(); it!=hist.rend(); ++it) undo_move(*it);
    return val;
}

double Engine::simulate(Node* node, int depth){
    Player pl=node->player;
    Player winner;
    if(is_terminal(&winner)){
        node->terminal=true; node->winner=winner; double v = (winner==pl)?1:-1; node->N++; node->W+=v; node->Q=node->W/node->N; return v;
    }
    if(depth>cfg_.rollout_max_plies/2) return value_estimate(pl);
    if(!node->expanded){
        PathInfo pi=compute_paths(pl); std::vector<int> moves; std::vector<double> priors; policy_prior(pl,pi,moves,priors);
        node->edges.resize(moves.size());
        for(size_t i=0;i<moves.size();++i){ node->edges[i].move=moves[i]; node->edges[i].P=priors[i]; }
        node->expanded=true;
        if(node==root_ && cfg_.use_root_noise) apply_dirichlet(node);
        double v_heur=heuristic_value(pl); double v_roll=rollout(pl); double v=0.65*v_roll+0.35*v_heur;
        node->N++; node->W+=v; node->Q=node->W/node->N; return v;
    }
    // selection
    Edge* best=nullptr; double best_score=-1e9; for(auto &e: node->edges){ double s=puct_score(node,e); if(s>best_score){ best_score=s; best=&e; }}
    if(!best) return 0;
    Node* child=best->child;
    MoveHist h; make_move(best->move,h);
    if(!child){ uint64_t hkey=zobrist_hash_; child=get_node(hkey); if(!child){ child=new_node(); child->hash=hkey; child->player=to_move_; if(cfg_.use_transposition) { tt_[hkey]=child; maybe_evict_tt(); } }
        best->child=child; if(child->N>0){ best->N=child->N; best->W=child->W; best->Q=child->Q; }}
    double v = -simulate(child, depth+1);
    undo_move(h);
    best->N++; best->W+=v; best->Q=best->W/std::max(1,best->N);
    node->N++; node->W+=v; node->Q=node->W/node->N;
    return v;
}

int Engine::genmove(int ms, int sims){
    if(ms<=0) ms=cfg_.default_ms; if(sims<=0) sims=cfg_.default_sims;
    root_ = get_node(zobrist_hash_); if(!root_){ root_=new_node(); root_->hash=zobrist_hash_; root_->player=to_move_; if(cfg_.use_transposition) { tt_[zobrist_hash_]=root_; maybe_evict_tt(); } }
    auto start=std::chrono::steady_clock::now(); last_stats_=SearchStats{};
    int it=0;
    if(root_->edges.empty()){ simulate(root_,0); it=1; }
    while(true){
        if(sims>0 && it>=sims) break;
        auto now=std::chrono::steady_clock::now(); if(ms>0){ double elapsed=std::chrono::duration<double,std::milli>(now-start).count(); if(elapsed>ms) break; }
        simulate(root_,0); it++;
    }
    if(root_->edges.empty()) return -1;
    // choose move
    Edge* best=nullptr; for(auto &e: root_->edges){ if(!best || e.N>best->N) best=&e; }
    int move = best?best->move:-1;
    double winrate = best && root_->N>0 ? 0.5*(best->Q+1) : 0.5;
    last_stats_.iters=it; last_stats_.root_winrate=winrate;
    if(move>=0) play_move(move);
    // shift root
    if(best && best->child){ root_=best->child; } else { root_=nullptr; }
    return move;
}

} // namespace hex
