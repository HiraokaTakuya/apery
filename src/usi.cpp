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
	void onEvalDir(Searcher*, const USIOption& opt)    {
		std::unique_ptr<Evaluater>(new Evaluater)->init(opt, true);
	}
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
	(*this)["Eval_Dir"]                    = USIOption("20160307", onEvalDir);
	(*this)["Write_Synthesized_Eval"]      = USIOption(false);
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

#if defined USE_GLOBAL
#else
// 教師局面を増やす為、適当に駒を動かす。玉の移動を多めに。王手が掛かっている時は呼ばない事にする。
// 駒を動かした場合は true, 何もしなかった場合は false を返す。
bool randomMove(Position& pos, std::mt19937& mt) {
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
			return false;
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
			return false;
		break;
	}
	default: UNREACHABLE;
	}

	// 違法手が混ざったりするので、一旦 sfen に直して読み込み、過去の手を参照しないようにする。
	std::string sfen = pos.toSFEN();
	std::istringstream ss(sfen);
	setPosition(pos, ss);
}
void make_teacher(std::istringstream& ssCmd) {
	std::string recordFileName;
	std::string outputFileName;
	int threadNum;
	ssCmd >> recordFileName;
	ssCmd >> outputFileName;
	ssCmd >> threadNum;
	if (threadNum <= 0)
		exit(EXIT_FAILURE);
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
	if (recordFileName == "-") // "-" なら棋譜ファイルを読み込まない。
		exit(EXIT_FAILURE);
	std::ifstream ifs(recordFileName.c_str(), std::ios::binary);
	if (!ifs)
		exit(EXIT_FAILURE);
	std::string sfen;
	std::vector<std::string> sfens;
	while (std::getline(ifs, sfen))
		sfens.emplace_back(sfen);

	Mutex mutex;
	std::ofstream ofs(outputFileName.c_str(), std::ios::binary);
	std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
	std::shuffle(std::begin(sfens), std::end(sfens), mt);
	auto func = [&mutex, &ofs, &sfens](Position& pos, std::atomic<s64>& idx) {
		std::mt19937 mt(std::chrono::system_clock::now().time_since_epoch().count());
		std::uniform_int_distribution<int> doRandomMoveDist(0, 4);
		for (s64 i = idx++; i < static_cast<s64>(sfens.size()); i = idx++) {
			if (i >= static_cast<s64>(sfens.size()))
				return;
			std::istringstream ss(sfens[i]);
			setPosition(pos, ss);
			randomMove(pos, mt); // 教師局面を増やす為、取得した元局面からランダムに動かしておく。
			std::unordered_set<Key> keyHash;
			StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
			for (Ply ply = pos.gamePly(); ply < 400; ++ply) { // 400 手くらいで終了しておく。
				if (!pos.inCheck() && doRandomMoveDist(mt) <= 1) { // 王手が掛かっていない局面で、20% の確率でランダムに局面を動かす。
					randomMove(pos, mt);
					ply = 0;
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
					std::ostringstream oss;
					oss << pos.toSFEN(ply) << "\n";
					auto& pv = pos.searcher()->threads.mainThread()->rootMoves[0].pv_;
					Ply tmpPly = 0;
					StateInfo state[MaxPlyPlus4];
					StateInfo* st = state;
					while (!pv[tmpPly].isNone()) {
						oss << pv[tmpPly].toUSI() << " ";
						pos.doMove(pv[tmpPly++], *st++);
					}
					oss << "\n";
					// evaluate() の差分計算を無効化する。
					SearchStack ss[2];
					ss[0].staticEvalRaw.p[0][0] = ss[1].staticEvalRaw.p[0][0] = ScoreNotEvaluated;
					const Score eval = evaluate(pos, ss+1);
					oss << pos.toSFEN(ply + tmpPly) << "\n";
					oss << eval << "\n";

					while (tmpPly)
						pos.undoMove(pv[--tmpPly]);

					std::unique_lock<Mutex> lock(mutex);
					ofs << oss.str();
				}

				setUpStates->push(StateInfo());
				pos.doMove(bestMove, setUpStates->top());
			}
		}
	};
	std::atomic<s64> index;
	index = 0;
	std::vector<std::thread> threads(threadNum);
	for (int i = 0; i < threadNum; ++i)
		threads[i] = std::thread([&positions, &index, i, &func] { func(positions[i], index); });
	for (int i = 0; i < threadNum; ++i)
		threads[i].join();
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
void use_teacher(Position& pos, std::istringstream& ssCmd) {
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
		SearchStack ss[2];
		std::string sfen;
		std::string sfenLeaf;
		std::string pvStr;
		std::string evalLeafStr;
		std::string token;
		rawEvaluater.clear();
		pos.searcher()->tt.clear();
		while (true) {
			{
				std::unique_lock<Mutex> lock(mutex);
				if (std::getline(ifs, sfen)) { // 4行で1組としている。
					std::getline(ifs, pvStr);
					std::getline(ifs, sfenLeaf);
					std::getline(ifs, evalLeafStr);
				}
				else
					return;
			}
			auto setpos = [](const std::string sfenStr, Position& pos) {
				std::istringstream iss(sfenStr);
				setPosition(pos, iss);
			};
			setpos(sfen, pos);
			const Color rootColor = pos.turn();
			std::istringstream issPV(pvStr);
			issPV >> token;
			const Move teacherMove = usiToMove(pos, token);
			pos.searcher()->alpha = -ScoreMaxEvaluate;
			pos.searcher()->beta  =  ScoreMaxEvaluate;
			go(pos, static_cast<Depth>(1));
			const Move shallowSearchedMove = pos.searcher()->threads.mainThread()->rootMoves[0].pv_[0];
			if (shallowSearchedMove == teacherMove) // 教師と同じ手を指せたら学習しない。
				continue;
			// pv を辿って評価値を返す。pos は pv を辿る為に状態が変わる。
			auto pvEval = [&ss](Position& pos) {
				auto& pv = pos.searcher()->threads.mainThread()->rootMoves[0].pv_;
				const Color rootColor = pos.turn();
				Ply ply = 0;
				StateInfo state[MaxPlyPlus4];
				StateInfo* st = state;
				while (!pv[ply].isNone())
					pos.doMove(pv[ply++], *st++);
				ss[0].staticEvalRaw.p[0][0] = ss[1].staticEvalRaw.p[0][0] = ScoreNotEvaluated;
				const Score eval = (rootColor == pos.turn() ? evaluate(pos, ss+1) : -evaluate(pos, ss+1));
				return eval;
			};
			const Score eval = pvEval(pos);

			// 教師手で探索する。
			setpos(sfen, pos);
			pos.searcher()->alpha = -ScoreMaxEvaluate;
			pos.searcher()->beta  =  ScoreMaxEvaluate;
			go(pos, static_cast<Depth>(1), teacherMove);
			const Score teacherEval = pvEval(pos);

			auto diff = eval - teacherEval;
			const double dsig = dsigmoid(diff);
			dsigSumNorm += fabs(dsig);
			std::array<double, 2> dT = {{(rootColor == Black ? -dsig : dsig), (rootColor == pos.turn() ? -dsig : dsig)}};
			rawEvaluater.incParam(pos, dT);

			setpos(sfenLeaf, pos);
			dT[0] = -dT[0];
			dT[1] = (pos.turn() == rootColor ? -dT[1] : dT[1]);
			rawEvaluater.incParam(pos, dT);
		}
	};

	auto evalBase = std::unique_ptr<EvalBaseType>(new EvalBaseType);
	auto eval = std::unique_ptr<Evaluater>(new Evaluater);
	eval->init(pos.searcher()->options["Eval_Dir"], false);
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
		updateEval<true>(*eval, *evalBase, pos.searcher()->options["Eval_Dir"], true);
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
		else if (token == "usinewgame") {
			tt.clear();
			threads.mainThread()->previousScore = ScoreInfinite;
			for (int i = 0; i < 100; ++i) g_randomTimeSeed(); // 最初は乱数に偏りがあるかも。少し回しておく。
		}
		else if (token == "usi"      ) SYNCCOUT << "id name " << MyName
												<< "\nid author Hiraoka Takuya"
												<< "\n" << options
												<< "\nusiok" << SYNCENDL;
		else if (token == "go"       ) go(pos, ssCmd);
		else if (token == "isready"  ) SYNCCOUT << "readyok" << SYNCENDL;
		else if (token == "position" ) setPosition(pos, ssCmd);
		else if (token == "setoption") setOption(ssCmd);
#if defined LEARN
		else if (token == "l"        ) {
			auto learner = std::unique_ptr<Learner>(new Learner);
			learner->learn(pos, ssCmd);
		}
		else if (token == "make_teacher") make_teacher(ssCmd);
		else if (token == "use_teacher") use_teacher(pos, ssCmd);
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

	if (options["Write_Synthesized_Eval"])
		Evaluater::writeSynthesized(options["Eval_Dir"]);
}
