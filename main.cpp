#include "engine.hpp"
#include <iostream>
#include <sstream>

using namespace hex;

int main(){
    Engine engine;
    std::cout << "Hex engine ready. Commands: size N | new | play a1 | gen [ms] | print | turn | quit" << std::endl;
    std::string line;
    while(std::cout << "> " && std::getline(std::cin,line)){
        std::stringstream ss(line); std::string cmd; ss>>cmd; if(cmd.empty()) continue;
        for(auto &ch: cmd) ch=std::tolower(ch);
        if(cmd=="quit"||cmd=="exit") break;
        else if(cmd=="size"){ int n; if(ss>>n && n>=5 && n<=13){ engine.reset(n); std::cout << "Board set to "<<n<<std::endl; } else std::cout << "Invalid size"<<std::endl; }
        else if(cmd=="new"){ engine.reset(); std::cout << "New game."<<std::endl; }
        else if(cmd=="print"){ engine.print(std::cout); }
        else if(cmd=="turn"){ std::cout << (engine.to_move()==WHITE?"White (top-bottom)":"Black (left-right)") << " to move"<<std::endl; }
        else if(cmd=="play"){ std::string c; ss>>c; if(c.empty()){ std::cout << "Usage: play a1"<<std::endl; continue;} if(engine.play_coord(c)) std::cout << "Played "<<c<<std::endl; else std::cout << "Illegal"<<std::endl; }
        else if(cmd=="gen"){ int ms; if(!(ss>>ms)) ms=0; int mv=engine.genmove(ms); if(mv<0) { std::cout << "No move"<<std::endl; continue;} std::cout << "Engine plays "<<engine.idx_to_coord(mv)<<" (winrate "<<engine.last_stats().root_winrate*100<<"%)"<<std::endl; }
        else { std::cout << "Unknown command"<<std::endl; }
        Player w; if(engine.is_terminal(&w)){ std::cout << (w==WHITE?"White":"Black") << " wins!"<<std::endl; engine.print(std::cout); }
    }
    return 0;
}
