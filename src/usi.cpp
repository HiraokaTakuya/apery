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
	void onHashSize(Searcher* s, const USIOption& opt) { s->tt.setSize(opt); }
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
	(*this)["USI_Hash"]                    = USIOption(256, 1, 65536, onHashSize, s);
	(*this)["Clear_Hash"]                  = USIOption(onClearHash, s);
	(*this)["Book_File"]                   = USIOption("book/20150503/book.bin");
	(*this)["Best_Book_Move"]              = USIOption(false);
	(*this)["OwnBook"]                     = USIOption(true);
	(*this)["Min_Book_Ply"]                = USIOption(SHRT_MAX, 0, SHRT_MAX);
	(*this)["Max_Book_Ply"]                = USIOption(SHRT_MAX, 0, SHRT_MAX);
	(*this)["Min_Book_Score"]              = USIOption(-180, -ScoreInfinite, ScoreInfinite);
	(*this)["USI_Ponder"]                  = USIOption(true);
	(*this)["Byoyomi_Margin"]              = USIOption(500, 0, INT_MAX);
	(*this)["Inc_Margin"]                  = USIOption(4500, 0, INT_MAX);
	(*this)["MultiPV"]                     = USIOption(1, 1, MaxLegalMoves);
	(*this)["Max_Random_Score_Diff"]       = USIOption(0, 0, ScoreMate0Ply);
	(*this)["Max_Random_Score_Diff_Ply"]   = USIOption(SHRT_MAX, 0, SHRT_MAX);
	(*this)["Slow_Mover"]                  = USIOption(100, 10, 1000);
	(*this)["Minimum_Thinking_Time"]       = USIOption(1500, 0, INT_MAX);
	(*this)["Threads"]                     = USIOption(cpuCoreCount(), 1, MaxThreads, onThreads, s);
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
		else if (token == "binc"       ) ssCmd >> limits.increment[Black];
		else if (token == "winc"       ) ssCmd >> limits.increment[White];
		else if (token == "infinite"   ) limits.infinite = true;
		else if (token == "byoyomi" || token == "movetime") ssCmd >> limits.moveTime;
		else if (token == "depth"      ) { ssCmd >> limits.depth; }
		else if (token == "nodes"      ) { ssCmd >> limits.nodes; }
		else if (token == "searchmoves") {
			while (ssCmd >> token)
				limits.searchmoves.push_back(usiToMove(pos, token));
		}
	}
	if      (limits.moveTime != 0)
		limits.moveTime -= pos.searcher()->options["Byoyomi_Margin"];
	else if (limits.increment[pos.turn()] != 0)
		limits.time[pos.turn()] -= pos.searcher()->options["Inc_Margin"];
	pos.searcher()->threads.startThinking(pos, limits, pos.searcher()->usiSetUpStates);
}

#if defined LEARN
// 学習用。通常の go 呼び出しは文字列を扱って高コストなので、大量に探索の開始、終了を行う学習では別の呼び出し方にする。
void go(const Position& pos, const Ply depth, const Move move) {
	LimitsType limits;
	limits.depth = depth;
	limits.searchmoves.push_back(move);
	pos.searcher()->threads.startThinking(pos, limits, pos.searcher()->usiSetUpStates);
	pos.searcher()->threads.mainThread()->waitForSearchFinished();
}
void go(const Position& pos, const Ply depth) {
	LimitsType limits;
	limits.depth = depth;
	pos.searcher()->threads.startThinking(pos, limits, pos.searcher()->usiSetUpStates);
	pos.searcher()->threads.mainThread()->waitForSearchFinished();
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
void extractPVFromTT(Position& pos, Move* moves) {
	StateInfo state[MaxPlyPlus4];
	StateInfo* st = state;
	TTEntry* tte;
	Ply ply = 0;
	Move m;
	bool ttHit;

	tte = pos.csearcher()->tt.probe(pos.getKey(), ttHit);
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
}

template <bool Undo>
void qsearch(Position& pos, Move moves[MaxPlyPlus4]) {
	SearchStack ss[MaxPlyPlus4];
	memset(ss, 0, 5 * sizeof(SearchStack));
	ss->staticEvalRaw.p[0][0] = (ss+1)->staticEvalRaw.p[0][0] = (ss+2)->staticEvalRaw.p[0][0] = ScoreNotEvaluated;
	if (pos.inCheck())
		pos.searcher()->qsearch<PV, true >(pos, ss+2, -ScoreInfinite, ScoreInfinite, Depth0);
	else
		pos.searcher()->qsearch<PV, false>(pos, ss+2, -ScoreInfinite, ScoreInfinite, Depth0);
	// pv 取得
	extractPVFromTT<Undo>(pos, moves);
}

#if defined USE_GLOBAL
#else
// 教師局面を増やす為、適当に駒を動かす。玉の移動を多めに。王手が掛かっている時は呼ばない事にする。
void randomMove(Position& pos, std::mt19937& mt) {
	assert(!pos.inCheck());
	StateInfo state[MaxPlyPlus4];
	StateInfo* st = state;
	const Color us = pos.turn();
	const Color them = oppositeColor(us);
	const Square from = pos.kingSquare(us);
	std::uniform_int_distribution<int> dist(0, 1);
	switch (dist(mt)) {
	case 0: { // 玉の25近傍の移動
		MoveStack legalMoves[MaxLegalMoves]; // 玉の移動も含めた普通の合法手
		MoveStack* pms = &legalMoves[0];
		Bitboard kingToBB = pos.bbOf(us).notThisAnd(neighbor5x5Table(from));
		while (kingToBB.isNot0()) {
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
		positions.emplace_back(DefaultStartPositionSFEN, s.threads.mainThread(), s.thisptr);
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
			if (!pos.inCheck())
				randomMove(pos, mt); // 教師局面を増やす為、取得した元局面からランダムに動かしておく。
			double randomMoveRateThresh = 0.2;
			std::unordered_set<Key> keyHash;
			StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
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
				const Score score = pos.searcher()->threads.mainThread()->rootMoves[0].score_;
				const Move bestMove = pos.searcher()->threads.mainThread()->rootMoves[0].pv_[0];
				if (2000 < abs(score)) // 差が付いたので投了した事にする。
					break;
				else if (bestMove.isNone()) // 勝ち宣言など
					break;

				{
					HuffmanCodedPosAndEval hcpe;
					hcpe.hcp = pos.toHuffmanCodedPos();
					auto& pv = pos.searcher()->threads.mainThread()->rootMoves[0].pv_;
					Ply tmpPly = 0;
					const Color rootTurn = pos.turn();
					StateInfo state[MaxPlyPlus4];
					StateInfo* st = state;
					while (!pv[tmpPly].isNone())
						pos.doMove(pv[tmpPly++], *st++);
					// evaluate() の差分計算を無効化する。
					SearchStack ss[2];
					ss[0].staticEvalRaw.p[0][0] = ss[1].staticEvalRaw.p[0][0] = ScoreNotEvaluated;
					const Score eval = evaluate(pos, ss+1);
					// root の手番から見た評価値に直す。
					hcpe.eval = (rootTurn == pos.turn() ? eval : -eval);

					while (tmpPly)
						pos.undoMove(pv[--tmpPly]);

					std::unique_lock<Mutex> lock(omutex);
					ofs.write(reinterpret_cast<char*>(&hcpe), sizeof(hcpe));
				}

				setUpStates->push(StateInfo());
				pos.doMove(bestMove, setUpStates->top());
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

	using EvalBaseType = EvaluaterBase<std::array<std::atomic<float>, 2>,
									   std::array<std::atomic<float>, 2>,
									   std::array<std::atomic<float>, 2> >;

	constexpr double FVPenalty() { return (0.2/static_cast<double>(FVScale)); }
	template <bool UsePenalty, typename T>
	void updateFV(std::array<T, 2>& v, const std::array<std::atomic<float>, 2>& dvRef) {
		const u64 updateMask = 3;
		static MT64bit mt64(std::chrono::system_clock::now().time_since_epoch().count());

		std::array<float, 2> dv = {dvRef[0].load(), dvRef[1].load()};
		const int step = count1s(mt64() & updateMask);
		for (int i = 0; i < 2; ++i) {
			if (UsePenalty) {
				if      (0 < v[i]) dv[i] -= static_cast<float>(FVPenalty());
				else if (v[i] < 0) dv[i] += static_cast<float>(FVPenalty());
			}

			// T が enum だと 0 になることがある。
			// enum のときは、std::numeric_limits<std::underlying_type<T>::type>::max() などを使う。
			static_assert(std::numeric_limits<T>::max() != 0, "");
			static_assert(std::numeric_limits<T>::min() != 0, "");
			if      (0.0 <= dv[i] && v[i] <= std::numeric_limits<T>::max() - step) v[i] += step;
			else if (dv[i] <= 0.0 && std::numeric_limits<T>::min() + step <= v[i]) v[i] -= step;
		}
	}
	template <bool UsePenalty>
	void updateEval(Evaluater& eval, EvalBaseType& evalBase, const std::string& dirName, const bool writeBase = true) {
		for (size_t i = 0; i < eval.kpps_end_index(); ++i)
			updateFV<UsePenalty>(*eval.oneArrayKPP(i), *evalBase.oneArrayKPP(i));
		for (size_t i = 0; i < eval.kkps_end_index(); ++i)
			updateFV<UsePenalty>(*eval.oneArrayKKP(i), *evalBase.oneArrayKKP(i));
		for (size_t i = 0; i < eval.kks_end_index(); ++i)
			updateFV<UsePenalty>(*eval.oneArrayKK(i), *evalBase.oneArrayKK(i));

		// 学習しないパラメータがある時は、一旦 write() で学習しているパラメータだけ書きこんで、再度読み込む事で、
		// updateFV()で学習しないパラメータに入ったノイズを無くす。
		if (writeBase)
			eval.write(dirName);
		eval.init(dirName, false, writeBase);
		g_evalTable.clear();
	}
}
void use_teacher(Position& /*pos*/, std::istringstream& ssCmd) {
	std::string teacherFileName;
	int threadNum;
	int stepNum;
	ssCmd >> teacherFileName;
	ssCmd >> threadNum;
	ssCmd >> stepNum;
	if (threadNum <= 0)
		exit(EXIT_FAILURE);
	std::vector<Searcher> searchers(threadNum);
	std::vector<Position> positions;
	std::vector<RawEvaluater> rawEvaluaters;
	// rawEvaluaters(threadNum) みたいにコンストラクタで確保するとスタックを使い切って落ちたので emplace_back する。
	for (int i = 0; i < threadNum; ++i)
		rawEvaluaters.emplace_back();
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
		positions.emplace_back(DefaultStartPositionSFEN, s.threads.mainThread(), s.thisptr);
	}
	if (teacherFileName == "-") // "-" なら棋譜ファイルを読み込まない。
		exit(EXIT_FAILURE);
	std::ifstream ifs(teacherFileName.c_str(), std::ios::binary);
	if (!ifs)
		exit(EXIT_FAILURE);

	Mutex mutex;
	auto func = [&mutex, &ifs](Position& pos, RawEvaluater& rawEvaluater, double& dsigSumNorm) {
		Move moves[MaxPlyPlus4];
		SearchStack ss[2];
		HuffmanCodedPosAndEval hcpe;
		rawEvaluater.clear();
		pos.searcher()->tt.clear();
		while (true) {
			{
				std::unique_lock<Mutex> lock(mutex);
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
			qsearch<false>(pos, moves); // 末端の局面に移動する。
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
			dsigSumNorm += fabs(dsig);
			std::array<double, 2> dT = {{(rootColor == Black ? -dsig : dsig), (rootColor == leafColor ? -dsig : dsig)}};
			rawEvaluater.incParam(pos, dT);
		}
	};

	auto evalBase = std::unique_ptr<EvalBaseType>(new EvalBaseType);
	auto eval = std::unique_ptr<Evaluater>(new Evaluater);
	eval->init(Evaluater::evalDir, false);
	Timer t;
	for (int step = 1; step <= stepNum; ++step) {
		t.restart();
		std::cout << "step " << step << "/" << stepNum << " " << std::flush;
		ifs.clear(); // 読み込み完了をクリアする。
		ifs.seekg(0, std::ios::beg); // ストリームポインタを先頭に戻す。
		std::vector<std::thread> threads(threadNum);
		std::vector<double> dsigSumNorms(threadNum, 0.0);
		for (int i = 0; i < threadNum; ++i)
			threads[i] = std::thread([&positions, i, &func, &rawEvaluaters, &dsigSumNorms] { func(positions[i], rawEvaluaters[i], dsigSumNorms[i]); });
		for (int i = 0; i < threadNum; ++i)
			threads[i].join();

		for (size_t size = 1; size < rawEvaluaters.size(); ++size)
			rawEvaluaters[0] += rawEvaluaters[size];
		evalBase->clear();
		lowerDimension(*evalBase, rawEvaluaters[0]);
		std::cout << "update eval ... " << std::flush;
		updateEval<true>(*eval, *evalBase, Evaluater::evalDir, true);
		std::cout << "done" << std::endl;
		std::cout << "step elapsed: " << t.elapsed() / 1000 << "[sec]" << std::endl;
		std::cout << "dsigSumNorm = " << std::accumulate(std::begin(dsigSumNorms), std::end(dsigSumNorms), 0.0) << std::endl;
		printEvalTable(SQ88, f_gold + SQ78, f_gold, false);
	}
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

	pos.set(sfen, pos.searcher()->threads.mainThread());
	pos.searcher()->usiSetUpStates = StateStackPtr(new std::stack<StateInfo>());

	Ply currentPly = pos.gamePly();
	while (ssCmd >> token) {
		const Move move = usiToMove(pos, token);
		if (move.isNone()) break;
		pos.searcher()->usiSetUpStates->push(StateInfo());
		pos.doMove(move, pos.searcher()->usiSetUpStates->top());
		++currentPly;
	}
	pos.setStartPosPly(currentPly);
}

void setPosition(Position& pos, const HuffmanCodedPos& hcp) {
	pos.set(hcp, pos.searcher()->threads.mainThread());
	pos.searcher()->usiSetUpStates = StateStackPtr(new std::stack<StateInfo>());
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

	MoveStack legalMoves[MaxLegalMoves];
	for (int i = 0; i < MaxLegalMoves; ++i) legalMoves[i].move = moveNone();
	MoveStack* pms = &legalMoves[0];
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
//			pms = generateMoves<PseudoLegal>(pms, pos);
//			pms = generateMoves<Legal>(pms, pos);
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

#ifdef NDEBUG
const std::string MyName = "Apery";
#else
const std::string MyName = "Apery Debug Build";
#endif

void Searcher::doUSICommandLoop(int argc, char* argv[]) {
	bool evalTableIsRead = false;
	Position pos(DefaultStartPositionSFEN, threads.mainThread(), thisptr);

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
				threads.mainThread()->startSearching(true);
			}
			else
				limits.ponder = false;
			if (token == "ponderhit" && limits.moveTime != 0)
				limits.moveTime += timeManager.elapsed();
		}
		else if (token == "go"       ) go(pos, ssCmd);
		else if (token == "position" ) setPosition(pos, ssCmd);
		else if (token == "usinewgame"); // isready で準備は出来たので、対局開始時に特にする事はない。
		else if (token == "usi"      ) SYNCCOUT << "id name " << MyName
												<< "\nid author Hiraoka Takuya"
												<< "\n" << options
												<< "\nusiok" << SYNCENDL;
		else if (token == "isready"  ) { // 対局開始前の準備。
			tt.clear();
			threads.mainThread()->previousScore = ScoreInfinite;
			if (!evalTableIsRead) {
				// 一時オブジェクトを生成して Evaluater::init() を呼んだ直後にオブジェクトを破棄する。
				// 評価関数の次元下げをしたデータを格納する分のメモリが無駄な為、
				std::unique_ptr<Evaluater>(new Evaluater)->init(Evaluater::evalDir, true);
				evalTableIsRead = true;
			}
			SYNCCOUT << "readyok" << SYNCENDL;
		}
		else if (token == "setoption") setOption(ssCmd);
		else if (token == "write_eval") { // 対局で使う為の評価関数バイナリをファイルに書き出す。
			if (!evalTableIsRead)
				std::unique_ptr<Evaluater>(new Evaluater)->init(Evaluater::evalDir, true);
			Evaluater::writeSynthesized(Evaluater::evalDir);
		}
#if defined LEARN
		else if (token == "l"        ) {
			auto learner = std::unique_ptr<Learner>(new Learner);
			learner->learn(pos, ssCmd);
		}
		else if (token == "make_teacher") {
			if (!evalTableIsRead) {
				std::unique_ptr<Evaluater>(new Evaluater)->init(Evaluater::evalDir, true);
				evalTableIsRead = true;
			}
			make_teacher(ssCmd);
		}
		else if (token == "use_teacher") {
			if (!evalTableIsRead) {
				std::unique_ptr<Evaluater>(new Evaluater)->init(Evaluater::evalDir, true);
				evalTableIsRead = true;
			}
			use_teacher(pos, ssCmd);
		}
#endif
#if !defined MINIMUL
		// 以下、デバッグ用
		else if (token == "bench"    ) benchmark(pos);
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

	threads.mainThread()->waitForSearchFinished();
}
