/*
  Apery, a USI shogi playing engine derived from Stockfish, a UCI chess playing engine.
  Copyright (C) 2004-2008 Tord Romstad (Glaurung author)
  Copyright (C) 2008-2015 Marco Costalba, Joona Kiiski, Tord Romstad
  Copyright (C) 2015-2016 Marco Costalba, Joona Kiiski, Gary Linscott, Tord Romstad
  Copyright (C) 2011-2016 Hiraoka Takuya

  Apery is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Apery is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "usi.hpp"
#include "position.hpp"
#include "move.hpp"
#include "movePicker.hpp"
#include "generateMoves.hpp"
#include "search.hpp"
#include "tt.hpp"
#include "book.hpp"
#include "thread.hpp"
#include "benchmark.hpp"
#include "learner.hpp"

namespace {
    void onThreads(Searcher* s, const USIOption&)      { s->threads.readUSIOptions(s); }
    void onHashSize(Searcher* s, const USIOption& opt) { s->tt.resize(opt); }
    void onClearHash(Searcher* s, const USIOption&)    { s->tt.clear(); }
}

bool CaseInsensitiveLess::operator () (const std::string& s1, const std::string& s2) const {
    for (size_t i = 0; i < s1.size() && i < s2.size(); ++i) {
        const int c1 = tolower(s1[i]);
        const int c2 = tolower(s2[i]);
        if (c1 != c2)
            return c1 < c2;
    }
    return s1.size() < s2.size();
}

namespace {
    // 論理的なコア数の取得
    inline int cpuCoreCount() {
        // std::thread::hardware_concurrency() は 0 を返す可能性がある。
        // HyperThreading が有効なら論理コア数だけ thread 生成した方が強い。
        return std::max(static_cast<int>(std::thread::hardware_concurrency()), 1);
    }

    class StringToPieceTypeCSA : public std::map<std::string, PieceType> {
    public:
        StringToPieceTypeCSA() {
            (*this)["FU"] = Pawn;
            (*this)["KY"] = Lance;
            (*this)["KE"] = Knight;
            (*this)["GI"] = Silver;
            (*this)["KA"] = Bishop;
            (*this)["HI"] = Rook;
            (*this)["KI"] = Gold;
            (*this)["OU"] = King;
            (*this)["TO"] = ProPawn;
            (*this)["NY"] = ProLance;
            (*this)["NK"] = ProKnight;
            (*this)["NG"] = ProSilver;
            (*this)["UM"] = Horse;
            (*this)["RY"] = Dragon;
        }
        PieceType value(const std::string& str) const {
            return this->find(str)->second;
        }
        bool isLegalString(const std::string& str) const {
            return (this->find(str) != this->end());
        }
    };
    const StringToPieceTypeCSA g_stringToPieceTypeCSA;
}

void OptionsMap::init(Searcher* s) {
    const int MaxHashMB = 1024 * 1024;
    (*this)["USI_Hash"]                    = USIOption(256, 1, MaxHashMB, onHashSize, s);
    (*this)["Clear_Hash"]                  = USIOption(onClearHash, s);
    (*this)["Book_File"]                   = USIOption("book/20150503/book.bin");
    (*this)["Eval_Dir"]                    = USIOption("20170329");
    (*this)["Best_Book_Move"]              = USIOption(false);
    (*this)["OwnBook"]                     = USIOption(true);
    (*this)["Min_Book_Ply"]                = USIOption(SHRT_MAX, 0, SHRT_MAX);
    (*this)["Max_Book_Ply"]                = USIOption(SHRT_MAX, 0, SHRT_MAX);
    (*this)["Min_Book_Score"]              = USIOption(-180, -ScoreInfinite, ScoreInfinite);
    (*this)["USI_Ponder"]                  = USIOption(true);
    (*this)["Byoyomi_Margin"]              = USIOption(500, 0, INT_MAX);
    (*this)["Time_Margin"]                 = USIOption(4500, 0, INT_MAX);
    (*this)["MultiPV"]                     = USIOption(1, 1, MaxLegalMoves);
    (*this)["Max_Random_Score_Diff"]       = USIOption(0, 0, ScoreMate0Ply);
    (*this)["Max_Random_Score_Diff_Ply"]   = USIOption(SHRT_MAX, 0, SHRT_MAX);
    (*this)["Slow_Mover_10"]               = USIOption(10, 1, 1000); // 持ち時間15分, 秒読み10秒では10, 持ち時間2時間では3にした。(sdt4)
    (*this)["Slow_Mover_16"]               = USIOption(20, 1, 1000); // 持ち時間15分, 秒読み10秒では50, 持ち時間2時間では20にした。(sdt4)
    (*this)["Slow_Mover_20"]               = USIOption(40, 1, 1000); // 持ち時間15分, 秒読み10秒では50, 持ち時間2時間では40にした。(sdt4)
    (*this)["Slow_Mover"]                  = USIOption(89, 1, 1000);
    (*this)["Draw_Ply"]                    = USIOption(256, 1, INT_MAX);
    (*this)["Move_Overhead"]               = USIOption(30, 0, 5000);
    (*this)["Minimum_Thinking_Time"]       = USIOption(20, 0, INT_MAX);
    (*this)["Threads"]                     = USIOption(cpuCoreCount(), 1, MaxThreads, onThreads, s);
#ifdef NDEBUG
    (*this)["Engine_Name"]                 = USIOption("Apery");
#else
    (*this)["Engine_Name"]                 = USIOption("Apery Debug Build");
#endif
}

USIOption::USIOption(const char* v, Fn* f, Searcher* s) :
    type_("string"), min_(0), max_(0), onChange_(f), searcher_(s)
{
    defaultValue_ = currentValue_ = v;
}

USIOption::USIOption(const bool v, Fn* f, Searcher* s) :
    type_("check"), min_(0), max_(0), onChange_(f), searcher_(s)
{
    defaultValue_ = currentValue_ = (v ? "true" : "false");
}

USIOption::USIOption(Fn* f, Searcher* s) :
    type_("button"), min_(0), max_(0), onChange_(f), searcher_(s) {}

USIOption::USIOption(const int v, const int min, const int max, Fn* f, Searcher* s)
    : type_("spin"), min_(min), max_(max), onChange_(f), searcher_(s)
{
    std::ostringstream ss;
    ss << v;
    defaultValue_ = currentValue_ = ss.str();
}

USIOption& USIOption::operator = (const std::string& v) {
    assert(!type_.empty());

    if ((type_ != "button" && v.empty())
        || (type_ == "check" && v != "true" && v != "false")
        || (type_ == "spin" && (atoi(v.c_str()) < min_ || max_ < atoi(v.c_str()))))
    {
        return *this;
    }

    if (type_ != "button")
        currentValue_ = v;

    if (onChange_ != nullptr)
        (*onChange_)(searcher_, *this);

    return *this;
}

std::ostream& operator << (std::ostream& os, const OptionsMap& om) {
    for (auto& elem : om) {
        const USIOption& o = elem.second;
        os << "\noption name " << elem.first << " type " << o.type_;
        if (o.type_ != "button")
            os << " default " << o.defaultValue_;

        if (o.type_ == "spin")
            os << " min " << o.min_ << " max " << o.max_;
    }
    return os;
}

void go(const Position& pos, std::istringstream& ssCmd) {
    LimitsType limits;
    std::string token;

    limits.startTime.restart();

    while (ssCmd >> token) {
        if      (token == "ponder"     ) limits.ponder = true;
        else if (token == "btime"      ) ssCmd >> limits.time[Black];
        else if (token == "wtime"      ) ssCmd >> limits.time[White];
        else if (token == "binc"       ) ssCmd >> limits.inc[Black];
        else if (token == "winc"       ) ssCmd >> limits.inc[White];
        else if (token == "infinite"   ) limits.infinite = true;
        else if (token == "byoyomi" || token == "movetime") ssCmd >> limits.moveTime;
        else if (token == "mate"       ) ssCmd >> limits.mate;
        else if (token == "depth"      ) ssCmd >> limits.depth;
        else if (token == "nodes"      ) ssCmd >> limits.nodes;
        else if (token == "searchmoves") {
            while (ssCmd >> token)
                limits.searchmoves.push_back(usiToMove(pos, token));
        }
    }
    if      (limits.moveTime != 0)
        limits.moveTime -= pos.searcher()->options["Byoyomi_Margin"];
    else if (pos.searcher()->options["Time_Margin"] != 0)
        limits.time[pos.turn()] -= pos.searcher()->options["Time_Margin"];
    pos.searcher()->threads.startThinking(pos, limits, pos.searcher()->states);
}

#if defined LEARN
// 学習用。通常の go 呼び出しは文字列を扱って高コストなので、大量に探索の開始、終了を行う学習では別の呼び出し方にする。
void go(const Position& pos, const Ply depth, const Move move) {
    LimitsType limits;
    limits.depth = depth;
    limits.searchmoves.push_back(move);
    pos.searcher()->threads.startThinking(pos, limits, pos.searcher()->states);
    pos.searcher()->threads.main()->waitForSearchFinished();
}
void go(const Position& pos, const Ply depth) {
    LimitsType limits;
    limits.depth = depth;
    pos.searcher()->threads.startThinking(pos, limits, pos.searcher()->states);
    pos.searcher()->threads.main()->waitForSearchFinished();
}
#endif

// 評価値 x を勝率にして返す。
// 係数 600 は Ponanza で採用しているらしい値。
inline double sigmoidWinningRate(const double x) {
    return 1.0 / (1.0 + exp(-x/600.0));
}
inline double dsigmoidWinningRate(const double x) {
    const double a = 1.0/600;
    return a * sigmoidWinningRate(x) * (1 - sigmoidWinningRate(x));
}

// 学習でqsearchだけ呼んだ時のPVを取得する為の関数。
// RootMoves が存在しない為、別の関数とする。
template <bool Undo> // 局面を戻し、moves に PV を書き込むなら true。末端の局面に移動したいだけなら false
bool extractPVFromTT(Position& pos, Move* moves, const Move bestMove) {
    StateInfo state[MaxPly+7];
    StateInfo* st = state;
    TTEntry* tte;
    Ply ply = 0;
    Move m;
    bool ttHit;

    tte = pos.csearcher()->tt.probe(pos.getKey(), ttHit);
    //if (ttHit && move16toMove(tte->move(), pos) != bestMove)
    //    return false; // 教師の手と異なる手の場合は学習しないので false。手が無い時は学習するので true
    while (ttHit
           && pos.moveIsPseudoLegal(m = move16toMove(tte->move(), pos))
           && pos.pseudoLegalMoveIsLegal<false, false>(m, pos.pinnedBB())
           && ply < MaxPly
           && (!pos.isDraw(20) || ply < 6))
    {
        if (Undo)
            *moves++ = m;
        pos.doMove(m, *st++);
        ++ply;
        tte = pos.csearcher()->tt.probe(pos.getKey(), ttHit);
    }
    if (Undo) {
        *moves++ = Move::moveNone();
        while (ply)
            pos.undoMove(*(--moves));
    }
    return true;
}

template <bool Undo>
bool qsearch(Position& pos, const u16 bestMove16) {
    //static std::atomic<int> i;
    //StateInfo st;
    Move pv[MaxPly+1];
    Move moves[MaxPly+1];
    SearchStack stack[MaxPly+7];
    SearchStack* ss = stack + 5;
    memset(ss-5, 0, 8 * sizeof(SearchStack));
    (ss-1)->staticEvalRaw.p[0][0] = (ss+0)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
    ss->pv = pv;
    // 探索の末端がrootと同じ手番に偏るのを防ぐ為に一手進めて探索してみる。
    //if ((i++ & 1) == 0) {
    //  const Move bestMove = move16toMove(Move(bestMove16), pos);
    //  pos.doMove(bestMove, st);
    //}
    if (pos.inCheck())
        pos.searcher()->qsearch<PV, true >(pos, ss, -ScoreInfinite, ScoreInfinite, Depth0);
    else
        pos.searcher()->qsearch<PV, false>(pos, ss, -ScoreInfinite, ScoreInfinite, Depth0);
    const Move bestMove = move16toMove(Move(bestMove16), pos);
    // pv 取得
    return extractPVFromTT<Undo>(pos, moves, bestMove);
}

#if defined USE_GLOBAL
#else
// 教師局面を増やす為、適当に駒を動かす。玉の移動を多めに。王手が掛かっている時は呼ばない事にする。
void randomMove(Position& pos, std::mt19937& mt) {
    StateInfo state[MaxPly+7];
    StateInfo* st = state;
    const Color us = pos.turn();
    const Color them = oppositeColor(us);
    const Square from = pos.kingSquare(us);
    std::uniform_int_distribution<int> dist(0, 1);
    switch (dist(mt)) {
    case 0: { // 玉の25近傍の移動
        ExtMove legalMoves[MaxLegalMoves]; // 玉の移動も含めた普通の合法手
        ExtMove* pms = &legalMoves[0];
        Bitboard kingToBB = pos.bbOf(us).notThisAnd(neighbor5x5Table(from));
        while (kingToBB) {
            const Square to = kingToBB.firstOneFromSQ11();
            const Move move = makeNonPromoteMove<Capture>(King, from, to, pos);
            if (pos.moveIsPseudoLegal<false>(move)
                && pos.pseudoLegalMoveIsLegal<true, false>(move, pos.pinnedBB()))
            {
                (*pms++).move = move;
            }
        }
        if (&legalMoves[0] != pms) { // 手があったなら
            std::uniform_int_distribution<int> moveDist(0, pms - &legalMoves[0] - 1);
            pos.doMove(legalMoves[moveDist(mt)].move, *st++);
            if (dist(mt)) { // 1/2 の確率で相手もランダムに指す事にする。
                MoveList<LegalAll> ml(pos);
                if (ml.size()) {
                    std::uniform_int_distribution<int> moveDist(0, ml.size()-1);
                    pos.doMove((ml.begin() + moveDist(mt))->move, *st++);
                }
            }
        }
        else
            return;
        break;
    }
    case 1: { // 玉も含めた全ての合法手
        bool moved = false;
        for (int i = 0; i < dist(mt) + 1; ++i) { // 自分だけ、または両者ランダムに1手指してみる。
            MoveList<LegalAll> ml(pos);
            if (ml.size()) {
                std::uniform_int_distribution<int> moveDist(0, ml.size()-1);
                pos.doMove((ml.begin() + moveDist(mt))->move, *st++);
                moved = true;
            }
        }
        if (!moved)
            return;
        break;
    }
    default: UNREACHABLE;
    }

    // 違法手が混ざったりするので、一旦 sfen に直して読み込み、過去の手を参照しないようにする。
    std::string sfen = pos.toSFEN();
    std::istringstream ss(sfen);
    setPosition(pos, ss);
}
// 教師局面を作成する。100万局面で34MB。
void make_teacher(std::istringstream& ssCmd) {
    std::string recordFileName;
    std::string outputFileName;
    int threadNum;
    s64 teacherNodes; // 教師局面数
    ssCmd >> recordFileName;
    ssCmd >> outputFileName;
    ssCmd >> threadNum;
    ssCmd >> teacherNodes;
    if (threadNum <= 0) {
        std::cerr << "Error: thread num = " << threadNum << std::endl;
        exit(EXIT_FAILURE);
    }
    if (teacherNodes <= 0) {
        std::cerr << "Error: teacher nodes = " << teacherNodes << std::endl;
        exit(EXIT_FAILURE);
    }
    std::vector<Searcher> searchers(threadNum);
    std::vector<Position> positions;
    for (auto& s : searchers) {
        s.init();
        const std::string options[] = {"name Threads value 1",
                                       "name MultiPV value 1",
                                       "name USI_Hash value 256",
                                       "name OwnBook value false",
                                       "name Max_Random_Score_Diff value 0"};
        for (auto& str : options) {
            std::istringstream is(str);
            s.setOption(is);
        }
        positions.emplace_back(DefaultStartPositionSFEN, s.threads.main(), s.thisptr);
    }
    std::ifstream ifs(recordFileName.c_str(), std::ifstream::in | std::ifstream::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "Error: cannot open " << recordFileName << std::endl;
        exit(EXIT_FAILURE);
    }
    const size_t entryNum = ifs.tellg() / sizeof(HuffmanCodedPos);
    std::uniform_int_distribution<s64> inputFileDist(0, entryNum-1);

    Mutex imutex;
    Mutex omutex;
    std::ofstream ofs(outputFileName.c_str(), std::ios::binary);
    if (!ofs) {
        std::cerr << "Error: cannot open " << outputFileName << std::endl;
        exit(EXIT_FAILURE);
    }
    auto func = [&omutex, &ofs, &imutex, &ifs, &inputFileDist, &teacherNodes](Position& pos, std::atomic<s64>& idx, const int threadID) {
        std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count() + threadID);
        std::uniform_real_distribution<double> doRandomMoveDist(0.0, 1.0);
        HuffmanCodedPos hcp;
        while (idx < teacherNodes) {
            {
                std::unique_lock<Mutex> lock(imutex);
                ifs.seekg(inputFileDist(mt) * sizeof(HuffmanCodedPos), std::ios_base::beg);
                ifs.read(reinterpret_cast<char*>(&hcp), sizeof(hcp));
            }
            setPosition(pos, hcp);
            randomMove(pos, mt); // 教師局面を増やす為、取得した元局面からランダムに動かしておく。
            double randomMoveRateThresh = 0.2;
            std::unordered_set<Key> keyHash;
            StateListPtr states = StateListPtr(new std::deque<StateInfo>(1));
            for (Ply ply = pos.gamePly(); ply < 400; ++ply, ++idx) { // 400 手くらいで終了しておく。
                if (!pos.inCheck() && doRandomMoveDist(mt) <= randomMoveRateThresh) { // 王手が掛かっていない局面で、randomMoveRateThresh の確率でランダムに局面を動かす。
                    randomMove(pos, mt);
                    ply = 0;
                    randomMoveRateThresh /= 2; // 局面を進めるごとに未知の局面になっていくので、ランダムに動かす確率を半分ずつ減らす。
                }
                const Key key = pos.getKey();
                if (keyHash.find(key) == std::end(keyHash))
                    keyHash.insert(key);
                else // 同一局面 2 回目で千日手判定とする。
                    break;
                pos.searcher()->alpha = -ScoreMaxEvaluate;
                pos.searcher()->beta  =  ScoreMaxEvaluate;
                go(pos, static_cast<Depth>(6));
                const Score score = pos.searcher()->threads.main()->rootMoves[0].score;
                const Move bestMove = pos.searcher()->threads.main()->rootMoves[0].pv[0];
                if (3000 < abs(score)) // 差が付いたので投了した事にする。
                    break;
                else if (!bestMove) // 勝ち宣言など
                    break;

                {
                    HuffmanCodedPosAndEval hcpe;
                    hcpe.hcp = pos.toHuffmanCodedPos();
                    auto& pv = pos.searcher()->threads.main()->rootMoves[0].pv;
                    const Color rootTurn = pos.turn();
                    StateInfo state[MaxPly+7];
                    StateInfo* st = state;
                    for (size_t i = 0; i < pv.size(); ++i)
                        pos.doMove(pv[i], *st++);
                    // evaluate() の差分計算を無効化する。
                    SearchStack ss[2];
                    ss[0].staticEvalRaw.p[0][0] = ss[1].staticEvalRaw.p[0][0] = ScoreNotEvaluated;
                    const Score eval = evaluate(pos, ss+1);
                    // root の手番から見た評価値に直す。
                    hcpe.eval = (rootTurn == pos.turn() ? eval : -eval);
                    hcpe.bestMove16 = static_cast<u16>(pv[0].value());

                    for (size_t i = pv.size(); i > 0;)
                        pos.undoMove(pv[--i]);

                    std::unique_lock<Mutex> lock(omutex);
                    ofs.write(reinterpret_cast<char*>(&hcpe), sizeof(hcpe));
                }

                states->push_back(StateInfo());
                pos.doMove(bestMove, states->back());
            }
        }
    };
    auto progressFunc = [&teacherNodes] (std::atomic<s64>& index, Timer& t) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 指定秒だけ待機し、進捗を表示する。
            const s64 madeTeacherNodes = index;
            const double progress = static_cast<double>(madeTeacherNodes) / teacherNodes;
            auto elapsed_msec = t.elapsed();
            if (progress > 0.0) // 0 除算を回避する。
                std::cout << std::fixed << "Progress: " << std::setprecision(2) << std::min(100.0, progress * 100.0)
                          << "%, Elapsed: " << elapsed_msec/1000
                          << "[s], Remaining: " << std::max<s64>(0, elapsed_msec*(1.0 - progress)/(progress*1000)) << "[s]" << std::endl;
            if (index >= teacherNodes)
                break;
        }
    };
    std::atomic<s64> index;
    index = 0;
    Timer t = Timer::currentTime();
    std::vector<std::thread> threads(threadNum);
    for (int i = 0; i < threadNum; ++i)
        threads[i] = std::thread([&positions, &index, i, &func] { func(positions[i], index, i); });
    std::thread progressThread([&index, &progressFunc, &t] { progressFunc(index, t); });
    for (int i = 0; i < threadNum; ++i)
        threads[i].join();
    progressThread.join();

    std::cout << "Made " << teacherNodes << " teacher nodes in " << t.elapsed()/1000 << " seconds." << std::endl;
}

namespace {
    // Learner とほぼ同じもの。todo: Learner と共通化する。

    using LowerDimensionedEvaluatorGradient = EvaluatorBase<std::array<std::atomic<double>, 2>,
                                                            std::array<std::atomic<double>, 2>,
                                                            std::array<std::atomic<double>, 2> >;
    using EvalBaseType = EvaluatorBase<std::array<double, 2>,
                                       std::array<double, 2>,
                                       std::array<double, 2> >;

    // 小数の評価値を round して整数に直す。
    void copyEval(Evaluator& eval, EvalBaseType& evalBase) {
#if defined _OPENMP
#pragma omp parallel
#endif
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < eval.kpps_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*eval.oneArrayKPP(i))[boardTurn] = round((*evalBase.oneArrayKPP(i))[boardTurn]);
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < eval.kkps_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*eval.oneArrayKKP(i))[boardTurn] = round((*evalBase.oneArrayKKP(i))[boardTurn]);
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < eval.kks_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*eval.oneArrayKK(i))[boardTurn] = round((*evalBase.oneArrayKK(i))[boardTurn]);
    }
    // 整数の評価値を小数に直す。
    void copyEval(EvalBaseType& evalBase, Evaluator& eval) {
#if defined _OPENMP
#pragma omp parallel
#endif
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < evalBase.kpps_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*evalBase.oneArrayKPP(i))[boardTurn] = (*eval.oneArrayKPP(i))[boardTurn];
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < evalBase.kkps_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*evalBase.oneArrayKKP(i))[boardTurn] = (*eval.oneArrayKKP(i))[boardTurn];
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < evalBase.kks_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*evalBase.oneArrayKK(i))[boardTurn] = (*eval.oneArrayKK(i))[boardTurn];
    }
    void averageEval(EvalBaseType& averagedEvalBase, EvalBaseType& evalBase) {
        constexpr double AverageDecay = 0.8; // todo: 過去のデータの重みが強すぎる可能性あり。
#if defined _OPENMP
#pragma omp parallel
#endif
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < averagedEvalBase.kpps_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*averagedEvalBase.oneArrayKPP(i))[boardTurn] = AverageDecay * (*averagedEvalBase.oneArrayKPP(i))[boardTurn] + (1.0 - AverageDecay) * (*evalBase.oneArrayKPP(i))[boardTurn];
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < averagedEvalBase.kkps_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*averagedEvalBase.oneArrayKKP(i))[boardTurn] = AverageDecay * (*averagedEvalBase.oneArrayKKP(i))[boardTurn] + (1.0 - AverageDecay) * (*evalBase.oneArrayKKP(i))[boardTurn];
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < averagedEvalBase.kks_end_index(); ++i)
            for (int boardTurn = 0; boardTurn < 2; ++boardTurn)
                (*averagedEvalBase.oneArrayKK(i))[boardTurn] = AverageDecay * (*averagedEvalBase.oneArrayKK(i))[boardTurn] + (1.0 - AverageDecay) * (*evalBase.oneArrayKK(i))[boardTurn];
    }
    constexpr double FVPenalty() { return (0.001/static_cast<double>(FVScale)); }
    // RMSProp(実質、改造してAdaGradになっている) でパラメータを更新する。
    template <typename T>
    void updateFV(std::array<T, 2>& v, const std::array<std::atomic<double>, 2>& grad, std::array<std::atomic<double>, 2>& msGrad, std::atomic<double>& max) {
        //constexpr double AttenuationRate = 0.99999;
        constexpr double UpdateParam = 100.0; // 更新用のハイパーパラメータ。大きいと不安定になり、小さいと学習が遅くなる。
        constexpr double epsilon = 0.000001; // 0除算防止の定数

        for (int i = 0; i < 2; ++i) {
            // ほぼAdaGrad
            msGrad[i] = /*AttenuationRate * */msGrad[i] + /*(1.0 - AttenuationRate) * */grad[i] * grad[i];
            const double updateStep = UpdateParam * grad[i] / sqrt(msGrad[i] + epsilon);
            v[i] += updateStep;
            const double fabsmax = fabs(updateStep);
            if (max < fabsmax)
                max = fabsmax;
        }
    }
    void updateEval(EvalBaseType& evalBase,
                    LowerDimensionedEvaluatorGradient& lowerDimentionedEvaluatorGradient,
                    LowerDimensionedEvaluatorGradient& meanSquareOfLowerDimensionedEvaluatorGradient)
    {
        std::atomic<double> max;
        max = 0.0;
#if defined _OPENMP
#pragma omp parallel
#endif
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < evalBase.kpps_end_index(); ++i)
            updateFV(*evalBase.oneArrayKPP(i), *lowerDimentionedEvaluatorGradient.oneArrayKPP(i), *meanSquareOfLowerDimensionedEvaluatorGradient.oneArrayKPP(i), max);
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < evalBase.kkps_end_index(); ++i)
            updateFV(*evalBase.oneArrayKKP(i), *lowerDimentionedEvaluatorGradient.oneArrayKKP(i), *meanSquareOfLowerDimensionedEvaluatorGradient.oneArrayKKP(i), max);
#ifdef _OPENMP
#pragma omp for
#endif
        for (size_t i = 0; i < evalBase.kks_end_index(); ++i)
            updateFV(*evalBase.oneArrayKK(i), *lowerDimentionedEvaluatorGradient.oneArrayKK(i), *meanSquareOfLowerDimensionedEvaluatorGradient.oneArrayKK(i), max);

        std::cout << "max update step : " << std::fixed << std::setprecision(2) << max << std::endl;
    }
}

constexpr s64 NodesPerIteration = 1000000; // 1回評価値を更新するのに使う教師局面数

void use_teacher(Position& pos, std::istringstream& ssCmd) {
    std::string teacherFileName;
    int threadNum;
    ssCmd >> teacherFileName;
    ssCmd >> threadNum;
    if (threadNum <= 0)
        exit(EXIT_FAILURE);
    std::vector<Searcher> searchers(threadNum);
    std::vector<Position> positions;
    // std::vector<TriangularEvaluatorGradient> だと、非常に大きな要素が要素数分メモリ上に連続する必要があり、
    // 例えメモリ量が余っていても、連続で確保出来ない場合は bad_alloc してしまうので、unordered_map にする。
    std::unordered_map<int, std::unique_ptr<TriangularEvaluatorGradient> > evaluatorGradients;
    // evaluatorGradients(threadNum) みたいにコンストラクタで確保するとスタックを使い切って落ちたので emplace_back する。
    for (int i = 0; i < threadNum; ++i)
        evaluatorGradients.emplace(i, std::move(std::unique_ptr<TriangularEvaluatorGradient>(new TriangularEvaluatorGradient)));
    for (auto& s : searchers) {
        s.init();
        const std::string options[] = {"name Threads value 1",
                                       "name MultiPV value 1",
                                       "name USI_Hash value 256",
                                       "name OwnBook value false",
                                       "name Max_Random_Score_Diff value 0"};
        for (auto& str : options) {
            std::istringstream is(str);
            s.setOption(is);
        }
        positions.emplace_back(DefaultStartPositionSFEN, s.threads.main(), s.thisptr);
    }
    if (teacherFileName == "-") // "-" なら棋譜ファイルを読み込まない。
        exit(EXIT_FAILURE);
    std::ifstream ifs(teacherFileName.c_str(), std::ios::binary);
    if (!ifs)
        exit(EXIT_FAILURE);

    Mutex mutex;
    auto func = [&mutex, &ifs](Position& pos, TriangularEvaluatorGradient& evaluatorGradient, double& loss, std::atomic<s64>& nodes) {
        SearchStack ss[2];
        HuffmanCodedPosAndEval hcpe;
        evaluatorGradient.clear();
        pos.searcher()->tt.clear();
        while (true) {
            {
                std::unique_lock<Mutex> lock(mutex);
                if (NodesPerIteration < nodes++)
                    return;
                ifs.read(reinterpret_cast<char*>(&hcpe), sizeof(hcpe));
                if (ifs.eof())
                    return;
            }
            auto setpos = [](HuffmanCodedPosAndEval& hcpe, Position& pos) {
                setPosition(pos, hcpe.hcp);
            };
            setpos(hcpe, pos);
            const Color rootColor = pos.turn();
            pos.searcher()->alpha = -ScoreMaxEvaluate;
            pos.searcher()->beta  =  ScoreMaxEvaluate;
            if (!qsearch<false>(pos, hcpe.bestMove16)) // 末端の局面に移動する。
                continue;
            // pv を辿って評価値を返す。pos は pv を辿る為に状態が変わる。
            auto pvEval = [&ss, &rootColor](Position& pos) {
                ss[0].staticEvalRaw.p[0][0] = ss[1].staticEvalRaw.p[0][0] = ScoreNotEvaluated;
                // evaluate() は手番側から見た点数なので、eval は rootColor から見た点数。
                const Score eval = (rootColor == pos.turn() ? evaluate(pos, ss+1) : -evaluate(pos, ss+1));
                return eval;
            };
            const Score eval = pvEval(pos);
            const Score teacherEval = static_cast<Score>(hcpe.eval); // root から見た評価値が入っている。
            const Color leafColor = pos.turn(); // pos は末端の局面になっている。
            // x を浅い読みの評価値、y を深い読みの評価値として、
            // 目的関数 f(x, y) は、勝率の誤差の最小化を目指す以下の式とする。
            // また、** 2 は 2 乗を表すとする。
            // f(x,y) = (sigmoidWinningRate(x) - sigmoidWinningRate(y)) ** 2
            //        = sigmoidWinningRate(x)**2 - 2*sigmoidWinningRate(x)*sigmoidWinningRate(y) + sigmoidWinningRate(y)**2
            // 浅い読みの点数を修正したいので、x について微分すると。
            // df(x,y)/dx = 2*sigmoidWinningRate(x)*dsigmoidWinningRate(x)-2*sigmoidWinningRate(y)*dsigmoidWinningRate(x)
            //            = 2*dsigmoidWinningRate(x)*(sigmoidWinningRate(x) - sigmoidWinningRate(y))
            const double dsig = 2*dsigmoidWinningRate(eval)*(sigmoidWinningRate(eval) - sigmoidWinningRate(teacherEval));
            const double tmp = sigmoidWinningRate(eval) - sigmoidWinningRate(teacherEval);
            loss += tmp * tmp;
            std::array<double, 2> dT = {{(rootColor == Black ? -dsig : dsig), (rootColor == leafColor ? -dsig : dsig)}};
            evaluatorGradient.incParam(pos, dT);
        }
    };

    auto lowerDimensionedEvaluatorGradient = std::unique_ptr<LowerDimensionedEvaluatorGradient>(new LowerDimensionedEvaluatorGradient);
    auto meanSquareOfLowerDimensionedEvaluatorGradient = std::unique_ptr<LowerDimensionedEvaluatorGradient>(new LowerDimensionedEvaluatorGradient); // 過去の gradient の mean square (二乗総和)
    auto evalBase = std::unique_ptr<EvalBaseType>(new EvalBaseType); // double で保持した評価関数の要素。相対位置などに分解して保持する。
    auto averagedEvalBase = std::unique_ptr<EvalBaseType>(new EvalBaseType); // ファイル保存する際に評価ベクトルを平均化したもの。
    auto eval = std::unique_ptr<Evaluator>(new Evaluator); // 整数化した評価関数。相対位置などに分解して保持する。
    eval->init(pos.searcher()->options["Eval_Dir"], false);
    copyEval(*evalBase, *eval); // 小数に直してコピー。
    memcpy(averagedEvalBase.get(), evalBase.get(), sizeof(EvalBaseType));
    const size_t fileSize = static_cast<size_t>(ifs.seekg(0, std::ios::end).tellg());
    ifs.clear(); // 読み込み完了をクリアする。
    ifs.seekg(0, std::ios::beg); // ストリームポインタを先頭に戻す。
    const s64 MaxNodes = fileSize / sizeof(HuffmanCodedPosAndEval);
    std::atomic<s64> nodes; // 今回のイテレーションで読み込んだ学習局面数。
    auto writeEval = [&] {
        // ファイル保存
        copyEval(*eval, *averagedEvalBase); // 平均化した物を整数の評価値にコピー
        //copyEval(*eval, *evalBase); // 平均化せずに整数の評価値にコピー
        std::cout << "write eval ... " << std::flush;
        eval->write(pos.searcher()->options["Eval_Dir"]);
        std::cout << "done" << std::endl;
    };
    // 平均化していない合成後の評価関数バイナリも出力しておく。
    auto writeSyn = [&] {
        std::ofstream((Evaluator::addSlashIfNone(pos.searcher()->options["Eval_Dir"]) + "KPP_synthesized.bin").c_str()).write((char*)Evaluator::KPP, sizeof(Evaluator::KPP));
        std::ofstream((Evaluator::addSlashIfNone(pos.searcher()->options["Eval_Dir"]) + "KKP_synthesized.bin").c_str()).write((char*)Evaluator::KKP, sizeof(Evaluator::KKP));
        std::ofstream((Evaluator::addSlashIfNone(pos.searcher()->options["Eval_Dir"]) + "KK_synthesized.bin" ).c_str()).write((char*)Evaluator::KK , sizeof(Evaluator::KK ));
    };
    Timer t;
    // 教師データ全てから学習した時点で終了する。
    for (s64 iteration = 0; NodesPerIteration * iteration + nodes <= MaxNodes; ++iteration) {
        t.restart();
        nodes = 0;
        std::cout << "iteration: " << iteration << ", nodes: " << NodesPerIteration * iteration + nodes << "/" << MaxNodes
                  << " (" << std::fixed << std::setprecision(2) << static_cast<double>(NodesPerIteration * iteration + nodes) * 100 / MaxNodes << "%)" << std::endl;
        std::vector<std::thread> threads(threadNum);
        std::vector<double> losses(threadNum, 0.0);
        for (int i = 0; i < threadNum; ++i)
            threads[i] = std::thread([&positions, i, &func, &evaluatorGradients, &losses, &nodes] { func(positions[i], *(evaluatorGradients[i]), losses[i], nodes); });
        for (int i = 0; i < threadNum; ++i)
            threads[i].join();
        if (nodes < NodesPerIteration)
            break; // パラメータ更新するにはデータが足りなかったので、パラメータ更新せずに終了する。

        for (size_t size = 1; size < (size_t)threadNum; ++size)
            *(evaluatorGradients[0]) += *(evaluatorGradients[size]); // 複数スレッドで個別に保持していた gradients を [0] の要素に集約する。
        lowerDimensionedEvaluatorGradient->clear();
        lowerDimension(*lowerDimensionedEvaluatorGradient, *(evaluatorGradients[0]));

        updateEval(*evalBase, *lowerDimensionedEvaluatorGradient, *meanSquareOfLowerDimensionedEvaluatorGradient);
        averageEval(*averagedEvalBase, *evalBase); // 平均化する。
        if (iteration < 10) // 最初は値の変動が大きいので適当に変動させないでおく。
            memset(&(*evalBase), 0, sizeof(EvalBaseType));
        if (iteration % 100 == 0) {
            writeEval();
            writeSyn();
        }
        copyEval(*eval, *evalBase); // 整数の評価値にコピー
        eval->init(pos.searcher()->options["Eval_Dir"], false, false); // 探索で使う評価関数の更新
        g_evalTable.clear(); // 評価関数のハッシュテーブルも更新しないと、これまで探索した評価値と矛盾が生じる。
        std::cout << "iteration elapsed: " << t.elapsed() / 1000 << "[sec]" << std::endl;
        std::cout << "loss: " << std::accumulate(std::begin(losses), std::end(losses), 0.0) << std::endl;
        printEvalTable(SQ88, f_gold + SQ78, f_gold, false);
    }
    writeEval();
    writeSyn();
}

// 教師データが壊れていないかチェックする。
// todo: 教師データがたまに壊れる原因を調べる。
void check_teacher(std::istringstream& ssCmd) {
    std::string teacherFileName;
    int threadNum;
    ssCmd >> teacherFileName;
    ssCmd >> threadNum;
    if (threadNum <= 0)
        exit(EXIT_FAILURE);
    std::vector<Searcher> searchers(threadNum);
    std::vector<Position> positions;
    for (auto& s : searchers) {
        s.init();
        positions.emplace_back(DefaultStartPositionSFEN, s.threads.main(), s.thisptr);
    }
    std::ifstream ifs(teacherFileName.c_str(), std::ios::binary);
    if (!ifs)
        exit(EXIT_FAILURE);
    Mutex mutex;
    auto func = [&mutex, &ifs](Position& pos) {
        HuffmanCodedPosAndEval hcpe;
        while (true) {
            {
                std::unique_lock<Mutex> lock(mutex);
                ifs.read(reinterpret_cast<char*>(&hcpe), sizeof(hcpe));
                if (ifs.eof())
                    return;
            }
            if (!setPosition(pos, hcpe.hcp))
                exit(EXIT_FAILURE);
        }
    };
    std::vector<std::thread> threads(threadNum);
    for (int i = 0; i < threadNum; ++i)
        threads[i] = std::thread([&positions, i, &func] { func(positions[i]); });
    for (int i = 0; i < threadNum; ++i)
        threads[i].join();
    exit(EXIT_SUCCESS);
}
#endif

Move usiToMoveBody(const Position& pos, const std::string& moveStr) {
    Move move;
    if (g_charToPieceUSI.isLegalChar(moveStr[0])) {
        // drop
        const PieceType ptTo = pieceToPieceType(g_charToPieceUSI.value(moveStr[0]));
        if (moveStr[1] != '*')
            return Move::moveNone();
        const File toFile = charUSIToFile(moveStr[2]);
        const Rank toRank = charUSIToRank(moveStr[3]);
        if (!isInSquare(toFile, toRank))
            return Move::moveNone();
        const Square to = makeSquare(toFile, toRank);
        move = makeDropMove(ptTo, to);
    }
    else {
        const File fromFile = charUSIToFile(moveStr[0]);
        const Rank fromRank = charUSIToRank(moveStr[1]);
        if (!isInSquare(fromFile, fromRank))
            return Move::moveNone();
        const Square from = makeSquare(fromFile, fromRank);
        const File toFile = charUSIToFile(moveStr[2]);
        const Rank toRank = charUSIToRank(moveStr[3]);
        if (!isInSquare(toFile, toRank))
            return Move::moveNone();
        const Square to = makeSquare(toFile, toRank);
        if (moveStr[4] == '\0')
            move = makeNonPromoteMove<Capture>(pieceToPieceType(pos.piece(from)), from, to, pos);
        else if (moveStr[4] == '+') {
            if (moveStr[5] != '\0')
                return Move::moveNone();
            move = makePromoteMove<Capture>(pieceToPieceType(pos.piece(from)), from, to, pos);
        }
        else
            return Move::moveNone();
    }

    if (pos.moveIsPseudoLegal<false>(move)
        && pos.pseudoLegalMoveIsLegal<false, false>(move, pos.pinnedBB()))
    {
        return move;
    }
    return Move::moveNone();
}
#if !defined NDEBUG
// for debug
Move usiToMoveDebug(const Position& pos, const std::string& moveStr) {
    for (MoveList<LegalAll> ml(pos); !ml.end(); ++ml) {
        if (moveStr == ml.move().toUSI())
            return ml.move();
    }
    return Move::moveNone();
}
Move csaToMoveDebug(const Position& pos, const std::string& moveStr) {
    for (MoveList<LegalAll> ml(pos); !ml.end(); ++ml) {
        if (moveStr == ml.move().toCSA())
            return ml.move();
    }
    return Move::moveNone();
}
#endif
Move usiToMove(const Position& pos, const std::string& moveStr) {
    const Move move = usiToMoveBody(pos, moveStr);
    assert(move == usiToMoveDebug(pos, moveStr));
    return move;
}

Move csaToMoveBody(const Position& pos, const std::string& moveStr) {
    if (moveStr.size() != 6)
        return Move::moveNone();
    const File toFile = charCSAToFile(moveStr[2]);
    const Rank toRank = charCSAToRank(moveStr[3]);
    if (!isInSquare(toFile, toRank))
        return Move::moveNone();
    const Square to = makeSquare(toFile, toRank);
    const std::string ptToString(moveStr.begin() + 4, moveStr.end());
    if (!g_stringToPieceTypeCSA.isLegalString(ptToString))
        return Move::moveNone();
    const PieceType ptTo = g_stringToPieceTypeCSA.value(ptToString);
    Move move;
    if (moveStr[0] == '0' && moveStr[1] == '0')
        // drop
        move = makeDropMove(ptTo, to);
    else {
        const File fromFile = charCSAToFile(moveStr[0]);
        const Rank fromRank = charCSAToRank(moveStr[1]);
        if (!isInSquare(fromFile, fromRank))
            return Move::moveNone();
        const Square from = makeSquare(fromFile, fromRank);
        PieceType ptFrom = pieceToPieceType(pos.piece(from));
        if (ptFrom == ptTo)
            // non promote
            move = makeNonPromoteMove<Capture>(ptFrom, from, to, pos);
        else if (ptFrom + PTPromote == ptTo)
            // promote
            move = makePromoteMove<Capture>(ptFrom, from, to, pos);
        else
            return Move::moveNone();
    }

    if (pos.moveIsPseudoLegal<false>(move)
        && pos.pseudoLegalMoveIsLegal<false, false>(move, pos.pinnedBB()))
    {
        return move;
    }
    return Move::moveNone();
}
Move csaToMove(const Position& pos, const std::string& moveStr) {
    const Move move = csaToMoveBody(pos, moveStr);
    assert(move == csaToMoveDebug(pos, moveStr));
    return move;
}

void setPosition(Position& pos, std::istringstream& ssCmd) {
    std::string token;
    std::string sfen;

    ssCmd >> token;

    if (token == "startpos") {
        sfen = DefaultStartPositionSFEN;
        ssCmd >> token; // "moves" が入力されるはず。
    }
    else if (token == "sfen") {
        while (ssCmd >> token && token != "moves")
            sfen += token + " ";
    }
    else
        return;

    pos.set(sfen, pos.searcher()->threads.main());
    pos.searcher()->states = StateListPtr(new std::deque<StateInfo>(1));

    Ply currentPly = pos.gamePly();
    while (ssCmd >> token) {
        const Move move = usiToMove(pos, token);
        if (!move) break;
        pos.searcher()->states->push_back(StateInfo());
        pos.doMove(move, pos.searcher()->states->back());
        ++currentPly;
    }
    pos.setStartPosPly(currentPly);
}

bool setPosition(Position& pos, const HuffmanCodedPos& hcp) {
    return pos.set(hcp, pos.searcher()->threads.main());
}

void Searcher::setOption(std::istringstream& ssCmd) {
    std::string token;
    std::string name;
    std::string value;

    ssCmd >> token; // "name" が入力されるはず。

    ssCmd >> name;
    // " " が含まれた名前も扱う。
    while (ssCmd >> token && token != "value")
        name += " " + token;

    ssCmd >> value;
    // " " が含まれた値も扱う。
    while (ssCmd >> token)
        value += " " + token;

    if (!options.isLegalOption(name))
        std::cout << "No such option: " << name << std::endl;
    else
        options[name] = value;
}

#if !defined MINIMUL
// for debug
// 指し手生成の速度を計測
void measureGenerateMoves(const Position& pos) {
    pos.print();

    ExtMove legalMoves[MaxLegalMoves];
    for (int i = 0; i < MaxLegalMoves; ++i) legalMoves[i].move = moveNone();
    ExtMove* pms = &legalMoves[0];
    const u64 num = 5000000;
    Timer t = Timer::currentTime();
    if (pos.inCheck()) {
        for (u64 i = 0; i < num; ++i) {
            pms = &legalMoves[0];
            pms = generateMoves<Evasion>(pms, pos);
        }
    }
    else {
        for (u64 i = 0; i < num; ++i) {
            pms = &legalMoves[0];
            pms = generateMoves<CapturePlusPro>(pms, pos);
            pms = generateMoves<NonCaptureMinusPro>(pms, pos);
            pms = generateMoves<Drop>(pms, pos);
//          pms = generateMoves<PseudoLegal>(pms, pos);
//          pms = generateMoves<Legal>(pms, pos);
        }
    }
    const int elapsed = t.elapsed();
    std::cout << "elapsed = " << elapsed << " [msec]" << std::endl;
    if (elapsed != 0)
        std::cout << "times/s = " << num * 1000 / elapsed << " [times/sec]" << std::endl;
    const ptrdiff_t count = pms - &legalMoves[0];
    std::cout << "num of moves = " << count << std::endl;
    for (int i = 0; i < count; ++i)
        std::cout << legalMoves[i].move.toCSA() << ", ";
    std::cout << std::endl;
}
#endif

void Searcher::doUSICommandLoop(int argc, char* argv[]) {
    bool evalTableIsRead = false;
    Position pos(DefaultStartPositionSFEN, threads.main(), thisptr);

    std::string cmd;
    std::string token;

    for (int i = 1; i < argc; ++i)
        cmd += std::string(argv[i]) + " ";

    do {
        if (argc == 1 && !std::getline(std::cin, cmd))
            cmd = "quit";

        std::istringstream ssCmd(cmd);

        ssCmd >> std::skipws >> token;

        if (token == "quit" || token == "stop" || token == "ponderhit" || token == "gameover") {
            if (token != "ponderhit" || signals.stopOnPonderHit) {
                signals.stop = true;
                threads.main()->startSearching(true);
            }
            else
                limits.ponder = false;
            if (token == "ponderhit" && limits.moveTime != 0)
                limits.moveTime += timeManager.elapsed();
        }
        else if (token == "go"       ) go(pos, ssCmd);
        else if (token == "position" ) setPosition(pos, ssCmd);
        else if (token == "usinewgame"); // isready で準備は出来たので、対局開始時に特にする事はない。
        else if (token == "usi"      ) SYNCCOUT << "id name " << std::string(options["Engine_Name"])
                                                << "\nid author Hiraoka Takuya"
                                                << "\n" << options
                                                << "\nusiok" << SYNCENDL;
        else if (token == "isready"  ) { // 対局開始前の準備。
            tt.clear();
            threads.main()->previousScore = ScoreInfinite;
            if (!evalTableIsRead) {
                // 一時オブジェクトを生成して Evaluator::init() を呼んだ直後にオブジェクトを破棄する。
                // 評価関数の次元下げをしたデータを格納する分のメモリが無駄な為、
                std::unique_ptr<Evaluator>(new Evaluator)->init(options["Eval_Dir"], true);
                evalTableIsRead = true;
            }
            SYNCCOUT << "readyok" << SYNCENDL;
        }
        else if (token == "setoption") setOption(ssCmd);
        else if (token == "write_eval") { // 対局で使う為の評価関数バイナリをファイルに書き出す。
            if (!evalTableIsRead)
                std::unique_ptr<Evaluator>(new Evaluator)->init(options["Eval_Dir"], true);
            Evaluator::writeSynthesized(options["Eval_Dir"]);
        }
#if defined LEARN
        else if (token == "l"        ) {
            auto learner = std::unique_ptr<Learner>(new Learner);
            learner->learn(pos, ssCmd);
        }
        else if (token == "make_teacher") {
            if (!evalTableIsRead) {
                std::unique_ptr<Evaluator>(new Evaluator)->init(options["Eval_Dir"], true);
                evalTableIsRead = true;
            }
            make_teacher(ssCmd);
        }
        else if (token == "use_teacher") {
            if (!evalTableIsRead) {
                std::unique_ptr<Evaluator>(new Evaluator)->init(options["Eval_Dir"], true);
                evalTableIsRead = true;
            }
            use_teacher(pos, ssCmd);
        }
        else if (token == "check_teacher") {
            check_teacher(ssCmd);
        }
        else if (token == "print"    ) printEvalTable(SQ88, f_gold + SQ78, f_gold, false);
#endif
#if !defined MINIMUL
        // 以下、デバッグ用
        else if (token == "bench"    ) {
            if (!evalTableIsRead) {
                std::unique_ptr<Evaluator>(new Evaluator)->init(options["Eval_Dir"], true);
                evalTableIsRead = true;
            }
            benchmark(pos);
        }
        else if (token == "key"      ) SYNCCOUT << pos.getKey() << SYNCENDL;
        else if (token == "tosfen"   ) SYNCCOUT << pos.toSFEN() << SYNCENDL;
        else if (token == "eval"     ) std::cout << evaluateUnUseDiff(pos) / FVScale << std::endl;
        else if (token == "d"        ) pos.print();
        else if (token == "s"        ) measureGenerateMoves(pos);
        else if (token == "t"        ) std::cout << pos.mateMoveIn1Ply().toCSA() << std::endl;
        else if (token == "b"        ) makeBook(pos, ssCmd);
#endif
        else                           SYNCCOUT << "unknown command: " << cmd << SYNCENDL;
    } while (token != "quit" && argc == 1);

    threads.main()->waitForSearchFinished();
}
