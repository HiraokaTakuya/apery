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

OptionsMap g_options;

namespace {
	void onThreads(const USIOption&)      { g_threads.readUSIOptions(); }
	void onHashSize(const USIOption& opt) { Searcher::tt.setSize(opt); }
	void onClearHash(const USIOption&)    { Searcher::tt.clear(); }
}

bool CaseInsensitiveLess::operator () (const std::string& s1, const std::string& s2) const {
	for (size_t i = 0; i < s1.size() && i < s2.size(); ++i) {
		const int c1 = tolower(s1[i]);
		const int c2 = tolower(s2[i]);

		if (c1 != c2) {
			return c1 < c2;
		}
	}
	return s1.size() < s2.size();
}

namespace {
	// 論理的なコア数の取得
	inline int cpuCoreCount() {
		// std::thread::hardware_concurrency() は 0 を返す可能性がある。
		return std::max(static_cast<int>(std::thread::hardware_concurrency()), 1);
	}

	StateStackPtr SetUpStates;

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

OptionsMap::OptionsMap() {
	const int cpus = cpuCoreCount();
	const int minSplitDepth = (cpus < 6 ? 4 : (cpus < 8 ? 5 : 7));
	(*this)["Use_Search_Log"]              = USIOption(false);
	(*this)["USI_Hash"]                    = USIOption(32, 1, 65536, onHashSize);
	(*this)["Clear_Hash"]                  = USIOption(onClearHash);
	(*this)["Book_File"]                   = USIOption("../bin/book.bin");
	(*this)["Inaniwa_Book_File"]           = USIOption("../bin/inaniwabook.bin");
	(*this)["Inaniwa_OwnBook"]             = USIOption(false);
	(*this)["Inaniwa_Random"]              = USIOption(0, 0, 100);
	(*this)["Best_Book_Move"]              = USIOption(false);
	(*this)["OwnBook"]                     = USIOption(true);
	(*this)["Min_Book_Ply"]                = USIOption(SHRT_MAX, 0, SHRT_MAX);
	(*this)["Max_Book_Ply"]                = USIOption(SHRT_MAX, 0, SHRT_MAX);
	(*this)["Min_Book_Score"]              = USIOption(-180, -ScoreInfinite, ScoreInfinite);
	(*this)["USI_Ponder"]                  = USIOption(true);
	(*this)["MultiPV"]                     = USIOption(1, 1, 500);
	(*this)["Skill_Level"]                 = USIOption(20, 0, 20);
	(*this)["Max_Random_Score_Diff"]       = USIOption(0, 0, ScoreMate0Ply);
	(*this)["Max_Random_Score_Diff_Ply"]   = USIOption(40, SHRT_MIN, SHRT_MAX);
	(*this)["Emergency_Move_Horizon"]      = USIOption(40, 0, 50);
	(*this)["Emergency_Base_Time"]         = USIOption(200, 0, 30000);
	(*this)["Emergency_Move_Time"]         = USIOption(70, 0, 5000);
	(*this)["Slow_Mover"]                  = USIOption(100, 10, 1000);
	(*this)["Minimum_Thinking_Time"]       = USIOption(1500, 0, INT_MAX);
	(*this)["Min_Split_Depth"]             = USIOption(minSplitDepth, 4, 12, onThreads);
	(*this)["Max_Threads_per_Split_Point"] = USIOption(5, 4, 8, onThreads);
	(*this)["Threads"]                     = USIOption(cpus, 1, MaxThreads, onThreads);
	(*this)["Use_Sleeping_Threads"]        = USIOption(true);
}

USIOption::USIOption(const char* v, Fn* f) : type_("string"), min_(0), max_(0), idx_(g_options.size()), onChange_(f) {
	defaultValue_ = currentValue_ = v;
}

USIOption::USIOption(const bool v, Fn* f) : type_("check"), min_(0), max_(0), idx_(g_options.size()), onChange_(f) {
	defaultValue_ = currentValue_ = (v ? "true" : "false");
}

USIOption::USIOption(Fn* f) : type_("button"), min_(0), max_(0), idx_(g_options.size()), onChange_(f) {}

USIOption::USIOption(const int v, const int min, const int max, Fn* f)
	: type_("spin"), min_(min), max_(max), idx_(g_options.size()), onChange_(f)
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

	if (type_ != "button") {
		currentValue_ = v;
	}

	if (onChange_ != nullptr) {
		(*onChange_)(*this);
	}

	return *this;
}

std::ostream& operator << (std::ostream& os, const OptionsMap& om) {
	for (size_t idx = 0; idx < om.size(); ++idx) {
		for (OptionsMap::const_iterator it = om.begin(); it != om.end(); ++it) {
			if (it->second.idx_ == idx) {
				const USIOption& o = it->second;
				os << "\noption name " << it->first << " type " << o.type_;

				if (o.type_ != "button") {
					os << " default " << o.defaultValue_;
				}

				if (o.type_ == "spin") {
					os << " min " << o.min_ << " max " << o.max_;
				}

				break;
			}
		}
	}
	return os;
}

void go(const Position& pos, std::istringstream& ssCmd) {
	LimitsType limits;
	std::vector<Move> searchMoves;
	std::string token;

	while (ssCmd >> token) {
		if      (token == "ponder"  ) { limits.ponder = true; }
		else if (token == "btime"   ) { ssCmd >> limits.time[Black]; }
		else if (token == "wtime"   ) { ssCmd >> limits.time[White]; }
		else if (token == "infinite") { limits.infinite = true; }
		else if (token == "byoyomi" || token == "movetime") {
			// btime wtime の後に byoyomi が来る前提になっているので良くない。
			ssCmd >> limits.moveTime;
			if (limits.moveTime != 0) { limits.moveTime -= 500; }
		}
	}
	Searcher::searchMoves = searchMoves;
	g_threads.startThinking(pos, limits, searchMoves, std::move(SetUpStates));
}

Move usiToMoveBody(const Position& pos, const std::string& moveStr) {
	Move move;
	if (g_charToPieceUSI.isLegalChar(moveStr[0])) {
		// drop
		const PieceType ptTo = pieceToPieceType(g_charToPieceUSI.value(moveStr[0]));
		if (moveStr[1] != '*') {
			return Move::moveNone();
		}
		const File toFile = charUSIToFile(moveStr[2]);
		const Rank toRank = charUSIToRank(moveStr[3]);
		if (!isInSquare(toFile, toRank)) {
			return Move::moveNone();
		}
		const Square to = makeSquare(toFile, toRank);
		move = makeDropMove(ptTo, to);
	}
	else {
		const File fromFile = charUSIToFile(moveStr[0]);
		const Rank fromRank = charUSIToRank(moveStr[1]);
		if (!isInSquare(fromFile, fromRank)) {
			return Move::moveNone();
		}
		const Square from = makeSquare(fromFile, fromRank);
		const File toFile = charUSIToFile(moveStr[2]);
		const Rank toRank = charUSIToRank(moveStr[3]);
		if (!isInSquare(toFile, toRank)) {
			return Move::moveNone();
		}
		const Square to = makeSquare(toFile, toRank);
		if (moveStr[4] == '\0') {
			move = makeNonPromoteMove<Capture>(pieceToPieceType(pos.piece(from)), from, to, pos);
		}
		else if (moveStr[4] == '+') {
			if (moveStr[5] != '\0') {
				return Move::moveNone();
			}
			move = makePromoteMove<Capture>(pieceToPieceType(pos.piece(from)), from, to, pos);
		}
		else {
			return Move::moveNone();
		}
	}

	if (pos.moveIsPseudoLegal(move, true)
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
		if (moveStr == ml.move().toUSI()) {
			return ml.move();
		}
	}
	return Move::moveNone();
}
Move csaToMoveDebug(const Position& pos, const std::string& moveStr) {
	for (MoveList<LegalAll> ml(pos); !ml.end(); ++ml) {
		if (moveStr == ml.move().toCSA()) {
			return ml.move();
		}
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
	if (moveStr.size() != 6) {
		return Move::moveNone();
	}
	const File toFile = charCSAToFile(moveStr[2]);
	const Rank toRank = charCSAToRank(moveStr[3]);
	if (!isInSquare(toFile, toRank)) {
		return Move::moveNone();
	}
	const Square to = makeSquare(toFile, toRank);
	const std::string ptToString(moveStr.begin() + 4, moveStr.end());
	if (!g_stringToPieceTypeCSA.isLegalString(ptToString)) {
		return Move::moveNone();
	}
	const PieceType ptTo = g_stringToPieceTypeCSA.value(ptToString);
	Move move;
	if (moveStr[0] == '0' && moveStr[1] == '0') {
		// drop
		move = makeDropMove(ptTo, to);
	}
	else {
		const File fromFile = charCSAToFile(moveStr[0]);
		const Rank fromRank = charCSAToRank(moveStr[1]);
		if (!isInSquare(fromFile, fromRank)) {
			return Move::moveNone();
		}
		const Square from = makeSquare(fromFile, fromRank);
		PieceType ptFrom = pieceToPieceType(pos.piece(from));
		if (ptFrom == ptTo) {
			// non promote
			move = makeNonPromoteMove<Capture>(ptFrom, from, to, pos);
		}
		else if (ptFrom + PTPromote == ptTo) {
			// promote
			move = makePromoteMove<Capture>(ptFrom, from, to, pos);
		}
		else {
			return Move::moveNone();
		}
	}

	if (pos.moveIsPseudoLegal(move, true)
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
		while (ssCmd >> token && token != "moves") {
			sfen += token + " ";
		}
	}
	else {
		return;
	}

	pos.set(sfen, g_threads.mainThread());
	SetUpStates = StateStackPtr(new std::stack<StateInfo>());

	Ply currentPly = pos.gamePly();
	while (ssCmd >> token) {
		const Move move = usiToMove(pos, token);
		if (move.isNone()) break;
		SetUpStates->push(StateInfo());
		pos.doMove(move, SetUpStates->top());
		++currentPly;
	}
	pos.setStartPosPly(currentPly);
}

void setOption(std::istringstream& ssCmd) {
	std::string token;
	std::string name;
	std::string value;

	ssCmd >> token; // "name" が入力されるはず。

	ssCmd >> name;
	// " " が含まれた名前も扱う。
	while (ssCmd >> token && token != "value") {
		name += " " + token;
	}

	ssCmd >> value;
	// " " が含まれた値も扱う。
	while (ssCmd >> token) {
		value += " " + token;
	}

	if (!g_options.isLegalOption(name)) {
		std::cout << "No such option: " << name << std::endl;
	}
	else if (value.empty()) {
		g_options[name] = true;
	}
	else {
		g_options[name] = value;
	}
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
	Time t = Time::currentTime();
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
	if (elapsed != 0) {
		std::cout << "times/s = " << num * 1000 / elapsed << " [times/sec]" << std::endl;
	}
	const ptrdiff_t count = pms - &legalMoves[0];
	std::cout << "num of moves = " << count << std::endl;
	for (int i = 0; i < count; ++i) {
		std::cout << legalMoves[i].move.toCSA() << ", ";
	}
	std::cout << std::endl;
}
#endif

#ifdef NDEBUG
const std::string MyName = "Apery";
#else
const std::string MyName = "Apery Debug Build";
#endif

void doUSICommandLoop(int argc, char* argv[]) {
	Position pos(DefaultStartPositionSFEN, g_threads.mainThread());

	std::string cmd;
	std::string token;

#if defined LEARN
	boost::mpi::environment  env(argc, argv);
	boost::mpi::communicator world;
	if (world.rank() != 0) {
		learn(pos, env, world);
		return;
	}
#endif

	for (int i = 1; i < argc; ++i)
		cmd += std::string(argv[i]) + " ";

	do {
		if (argc == 1)
			std::getline(std::cin, cmd);

		std::istringstream ssCmd(cmd);

		ssCmd >> std::skipws >> token;

		if (token == "quit" || token == "stop" || token == "ponderhit" || token == "gameover") {
			if (token != "ponderhit" || Searcher::signals.stopOnPonderHit) {
				Searcher::signals.stop = true;
				g_threads.mainThread()->notifyOne();
			}
			else {
				Searcher::limits.ponder = false;
			}
			if (token == "ponderhit" && Searcher::limits.moveTime != 0) {
				Searcher::limits.moveTime += Searcher::searchTimer.elapsed();
			}
		}
		else if (token == "usinewgame") {
			Searcher::tt.clear();
#if defined INANIWA_SHIFT
			g_inaniwaFlag = NotInaniwa;
#endif
			g_inaniwaGame = false;
			if (g_options["Inaniwa_OwnBook"]) {
				std::uniform_int_distribution<int> dist(0, 99); // 0 から 99 までの 100種類をランダムに。
				if (dist(g_mt64bit) < g_options["Inaniwa_Random"]) {
					g_inaniwaGame = true;
				}
			}
			for (int i = 0; i < 100; ++i) g_randomTimeSeed(); // 最初は乱数に偏りがあるかも。少し回しておく。
		}
		else if (token == "usi"      ) { SYNCCOUT << "id name " << MyName
												  << "\nid author Hiraoka Takuya"
												  << "\n" << g_options
												  << "\nusiok" << SYNCENDL; }
		else if (token == "go"       ) { go(pos, ssCmd); }
		else if (token == "isready"  ) { SYNCCOUT << "readyok" << SYNCENDL; }
		else if (token == "position" ) { setPosition(pos, ssCmd); }
		else if (token == "setoption") { setOption(ssCmd); }
#if defined LEARN
		else if (token == "l"        ) { learn(pos, env, world); }
#endif
#if !defined MINIMUL
		// 以下、デバッグ用
		else if (token == "bench"    ) { benchmark(pos); }
		else if (token == "d"        ) { pos.print(); }
		else if (token == "s"        ) { measureGenerateMoves(pos); }
		else if (token == "t"        ) { std::cout << pos.mateMoveIn1Ply().toCSA() << std::endl; }
		else if (token == "b"        ) { makeBookCSA1Line(pos); }
		else if (token == "i"        ) { makeBookCSA1Line(pos, true); }
		else if (token == "bsfen"    ) { makeBook(pos); }
#endif
		else                           { SYNCCOUT << "unknown command: " << cmd << SYNCENDL; }
	} while (token != "quit" && argc == 1);

	g_threads.waitForThinkFinished();
}
