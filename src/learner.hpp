#ifndef APERY_LEARNER_HPP
#define APERY_LEARNER_HPP

#include "position.hpp"
#include "thread.hpp"
#include "evaluate.hpp"

#if defined LEARN

#if 0
#define PRINT_PV
#endif

struct LearnEvaluater : public EvaluaterBase<float, float, float> {
	float kpp_raw[SquareNum][fe_end][fe_end];
	float kkp_raw[SquareNum][SquareNum][fe_end];
	float kk_raw[SquareNum][SquareNum];

	void incParam(const Position& pos, const double dinc) {
		const Square sq_bk = pos.kingSquare(Black);
		const Square sq_wk = pos.kingSquare(White);
		const int* list0 = pos.cplist0();
		const int* list1 = pos.cplist1();
		const float f = dinc / FVScale; // same as Bonanza

		kk_raw[sq_bk][sq_wk] += f;
		for (int i = 0; i < pos.nlist(); ++i) {
			const int k0 = list0[i];
			const int k1 = list1[i];
			for (int j = 0; j < i; ++j) {
				const int l0 = list0[j];
				const int l1 = list1[j];
				kpp_raw[sq_bk         ][k0][l0] += f;
				kpp_raw[inverse(sq_wk)][k1][l1] -= f;
			}
			kkp_raw[sq_bk][sq_wk][k0] += f;
		}
	}

	// kpp_raw, kkp_raw, kk_raw の値を低次元の要素に与える。
	void lowerDimension() {
#define FOO(indices, oneArray, sum)										\
		for (auto indexAndWeight : indices) {							\
			if (indexAndWeight.first == std::numeric_limits<ptrdiff_t>::max()) break; \
			if (0 <= indexAndWeight.first) oneArray[ indexAndWeight.first] += sum; \
			else                           oneArray[-indexAndWeight.first] -= sum; \
		}

		// KPP
		{
			std::pair<ptrdiff_t, int> indices[KPPIndicesMax];
			for (Square ksq = I9; ksq < SquareNum; ++ksq) {
				for (int i = 0; i < fe_end; ++i) {
					for (int j = 0; j < fe_end; ++j) {
						kppIndices(indices, ksq, i, j);
						FOO(indices, oneArrayKPP, kpp_raw[ksq][i][j]);
					}
				}
			}
		}
		// KKP
		{
			std::pair<ptrdiff_t, int> indices[KKPIndicesMax];
			for (Square ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
				for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
					for (int i = 0; i < fe_end; ++i) {
						kkpIndices(indices, ksq0, ksq1, i);
						FOO(indices, oneArrayKKP, kkp_raw[ksq0][ksq1][i]);
					}
				}
			}
		}
		// KK
		{
			std::pair<ptrdiff_t, int> indices[KKIndicesMax];
			for (Square ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
				for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
					kkIndices(indices, ksq0, ksq1);
					FOO(indices, oneArrayKK, kk_raw[ksq0][ksq1]);
				}
			}
		}
#undef FOO
	}

	void clear() { memset(this, 0, sizeof(*this)); } // float 型とかだと規格的に 0 は保証されなかった気がするが実用上問題ないだろう。
};

LearnEvaluater& operator += (LearnEvaluater& lhs, LearnEvaluater& rhs) {
	for (auto lit = &(***std::begin(lhs.kpp_raw)), rit = &(***std::begin(rhs.kpp_raw)); lit != &(***std::end(lhs.kpp_raw)); ++lit, ++rit)
		*lit += *rit;
	for (auto lit = &(***std::begin(lhs.kkp_raw)), rit = &(***std::begin(rhs.kkp_raw)); lit != &(***std::end(lhs.kkp_raw)); ++lit, ++rit)
		*lit += *rit;
	for (auto lit = &(** std::begin(lhs.kk_raw )), rit = &(** std::begin(rhs.kk_raw )); lit != &(** std::end(lhs.kk_raw )); ++lit, ++rit)
		*lit += *rit;

	return lhs;
}

struct Parse2Data {
	LearnEvaluater params;

	void clear() {
		params.clear();
	}
};

// 以下のようなフォーマットが入力される。
// <棋譜番号> <日付> <先手名> <後手名> <0:引き分け, 1:先手勝ち, 2:後手勝ち> <総手数> <棋戦名前> <戦形>
// <CSA1行形式の指し手>
//
// (例)
// 1 2003/09/08 羽生善治 谷川浩司 2 126 王位戦 その他の戦型
// 7776FU3334FU2726FU4132KI
struct BookMoveData {
	std::string player; // その手を指した人
	std::string date; // 対局日
	std::vector<Move> pvBuffer; // 正解のPV, その他0のPV, その他1のPV という順に並べる。
								// 間には MoveNone で区切りを入れる。

	Move move; // 指し手
	int recordIsNth; // 正解の手が何番目に良い手か。0から数える。
	bool winner; // 勝ったかどうか
	bool useLearning; // 学習に使うかどうか
	bool otherPVExist; // 棋譜の手と近い点数の手があったか。useLearning == true のときだけ有効な値が入る
};

class Learner {
public:
	void learn(Position& pos, std::istringstream& ssCmd) {
		eval_.init(pos.searcher()->options["Eval_Dir"], false);
		s64 gameNum;
		std::string recordFileName;
		size_t threadNum;
		s64 updateMax;
		ssCmd >> recordFileName;
		ssCmd >> gameNum;
		ssCmd >> threadNum;
		ssCmd >> minDepth_;
		ssCmd >> maxDepth_;
		ssCmd >> stepNum_;
		ssCmd >> gameNumForIteration_;
		ssCmd >> updateMax;
		if (updateMax < 0 || 32 < updateMax) {
			updateMax = 32; // 乱数が 32 bit なので、bit count 方式だと 32 が上限。
			std::cout << "you can set update_max [1, 32]" << std::endl;
		}
		std::cout << "record_file: " << recordFileName
				  << "\nread games: " << (gameNum == 0 ? "all" : std::to_string(gameNum))
				  << "\nthread_num: " << threadNum
				  << "\nsearch_depth min, max: " << minDepth_ << ", " << maxDepth_
				  << "\nstep_num: " << stepNum_
				  << "\ngame_num_for_iteration: " << gameNumForIteration_
				  << "\nupdate_max: " << updateMax
				  << std::endl;
		updateMaxMask_ = (UINT64_C(1) << updateMax) - 1;
		readBook(pos, recordFileName, gameNum);
		// 既に 1 つのSearcher, Positionが立ち上がっているので、指定した数 - 1 の Searcher, Position を立ち上げる。
		threadNum = std::max<size_t>(0, threadNum - 1);
		std::vector<Searcher> searchers(threadNum);
		for (auto& s : searchers) {
			s.init();
			setLearnOptions(s);
			positions_.push_back(Position(DefaultStartPositionSFEN, s.threads.mainThread(), s.thisptr));
			mts_.push_back(std::mt19937(std::chrono::system_clock::now().time_since_epoch().count()));
			// ここでデフォルトコンストラクタでpush_backすると、
			// 一時オブジェクトのParse2Dataがスタックに出来ることでプログラムが落ちるので、コピーコンストラクタにする。
			parse2Datum_.push_back(parse2Data_);
		}
		setLearnOptions(*pos.searcher());
		mt_ = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());
		for (int i = 0; ; ++i) {
			std::cout << "iteration " << i << std::endl;
			std::cout << "parse1 start" << std::endl;
			learnParse1(pos);
			std::cout << "parse2 start" << std::endl;
			learnParse2(pos);
		}
	}
private:
	// 学習に使う棋譜から、手と手に対する補助的な情報を付けでデータ保持する。
	// 50000局程度に対して10秒程度で終わるからシングルコアで良い。
	void setLearnMoves(Position& pos, std::set<std::pair<Key, Move> >& dict, std::string& s0, std::string& s1) {
		bookMovesDatum_.push_back(std::vector<BookMoveData>());
		BookMoveData bmdBase[ColorNum];
		bmdBase[Black].move = bmdBase[White].move = Move::moveNone();
		std::stringstream ss(s0);
		std::string elem;
		ss >> elem; // 対局番号
		ss >> elem; // 対局日
		bmdBase[Black].date = bmdBase[White].date = elem;
		ss >> elem; // 先手名
		bmdBase[Black].player = elem;
		ss >> elem; // 後手名
		bmdBase[White].player = elem;
		ss >> elem; // 引き分け勝ち負け
		bmdBase[Black].winner = (elem == "1");
		bmdBase[White].winner = (elem == "2");
		pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
		StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
		while (true) {
			const std::string moveStrCSA = s1.substr(0, 6);
			const Move move = csaToMove(pos, moveStrCSA);
			// 指し手の文字列のサイズが足りなかったり、反則手だったりすれば move.isNone() == true となるので、break する。
			if (move.isNone())
				break;
			BookMoveData bmd = bmdBase[pos.turn()];
			bmd.move = move;
			if (dict.find(std::make_pair(pos.getKey(), move)) == std::end(dict)) {
				// この局面かつこの指し手は初めて見るので、学習に使う。
				bmd.useLearning = true;
				dict.insert(std::make_pair(pos.getKey(), move));
			}
			else
				bmd.useLearning = false;

			bookMovesDatum_.back().push_back(bmd);
			s1.erase(0, 6);

			setUpStates->push(StateInfo());
			pos.doMove(move, setUpStates->top());
		}
	}
	void readBook(Position& pos, const std::string& recordFileName, const s64 gameNum) {
		std::ifstream ifs(recordFileName.c_str(), std::ios::binary);
		if (!ifs) {
			std::cout << "I cannot read " << recordFileName << std::endl;
			exit(EXIT_FAILURE);
		}
		std::set<std::pair<Key, Move> > dict;
		std::string s0;
		std::string s1;
		// 0 なら全部の棋譜を読む
		s64 tmpGameNum = (gameNum == 0 ? std::numeric_limits<s64>::max() : gameNum);
		for (s64 i = 0; i < tmpGameNum; ++i) {
			std::getline(ifs, s0);
			std::getline(ifs, s1);
			if (!ifs) break;
			setLearnMoves(pos, dict, s0, s1);
		}
		std::cout << "games existed: " << bookMovesDatum_.size() << std::endl;
		gameNumForIteration_ = std::min(gameNumForIteration_, bookMovesDatum_.size());
	}
	void setLearnOptions(Searcher& s) {
		std::string options[] = {"name Threads value 1",
								 "name MultiPV value " + std::to_string(MaxLegalMoves),
								 "name OwnBook value false",
								 "name Max_Random_Score_Diff value 0"};
		for (auto& str : options) {
			std::istringstream is(str);
			s.setOption(is);
		}
	}
	template <bool Dump> size_t lockingIndexIncrement() {
		std::unique_lock<std::mutex> lock(mutex_);
		if (Dump) {
			if      (index_ % 500 == 0) std::cout << index_ << std::endl;
			else if (index_ % 100 == 0) std::cout << "o" << std::flush;
			else if (index_ %  10 == 0) std::cout << "." << std::flush;
		}
		return index_++;
	}
	void learnParse1Body(Position& pos, std::mt19937& mt) {
		std::uniform_int_distribution<Ply> dist(minDepth_, maxDepth_);
		pos.searcher()->tt.clear();
		for (size_t i = lockingIndexIncrement<true>(); i < gameNumForIteration_; i = lockingIndexIncrement<true>()) {
			StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
			pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
			auto& gameMoves = bookMovesDatum_[i];
			for (auto& bmd : gameMoves) {
				if (bmd.useLearning) {
					std::istringstream ssCmd("depth " + std::to_string(dist(mt)));
					go(pos, ssCmd);
					pos.searcher()->threads.waitForThinkFinished();
					const auto recordIt = std::find_if(std::begin(pos.searcher()->rootMoves),
													   std::end(pos.searcher()->rootMoves),
													   [&](const RootMove& rm) { return rm.pv_[0] == bmd.move; });
					const Score recordScore = recordIt->score_;
					bmd.recordIsNth = recordIt - std::begin(pos.searcher()->rootMoves);
					bmd.pvBuffer.clear();
					bmd.pvBuffer.insert(std::end(bmd.pvBuffer), std::begin(recordIt->pv_), std::end(recordIt->pv_));

					const auto recordPVSize = bmd.pvBuffer.size();

					if (abs(recordScore) < ScoreMaxEvaluate) {
						for (auto it = recordIt - 1;
							 it >= std::begin(pos.searcher()->rootMoves) && FVWindow > (it->score_ - recordScore);
							 --it)
						{
							bmd.pvBuffer.insert(std::end(bmd.pvBuffer), std::begin(it->pv_), std::end(it->pv_));
						}
						for (auto it = recordIt + 1;
							 it < std::end(pos.searcher()->rootMoves) && FVWindow > (recordScore - it->score_);
							 ++it)
						{
							bmd.pvBuffer.insert(std::end(bmd.pvBuffer), std::begin(it->pv_), std::end(it->pv_));
						}
					}

					bmd.otherPVExist = (recordPVSize != bmd.pvBuffer.size());
				}
				setUpStates->push(StateInfo());
				pos.doMove(bmd.move, setUpStates->top());
			}
		}
	}
	void learnParse1(Position& pos) {
		Time t = Time::currentTime();
		// 棋譜をシャッフルすることで、先頭 gameNum_ 個の学習に使うデータをランダムに選ぶ。
		std::shuffle(std::begin(bookMovesDatum_), std::end(bookMovesDatum_), mt_);
		std::cout << "shuffle elapsed: " << t.elapsed() / 1000 << "[sec]" << std::endl;
		index_ = 0;
		std::vector<std::thread> threads(positions_.size());
		for (size_t i = 0; i < positions_.size(); ++i)
			threads[i] = std::thread([this, i] { learnParse1Body(positions_[i], mts_[i]); });
		learnParse1Body(pos, mt_);
		for (auto& thread : threads)
			thread.join();
		auto total_move = [this] {
			u64 count = 0;
			for (size_t i = 0; i < gameNumForIteration_; ++i) {
				auto& bmds = bookMovesDatum_[i];
				for (auto& bmd : bmds)
					if (bmd.useLearning)
						++count;
			}
			return count;
		};
		auto prediction = [this] (const int i) {
			std::vector<u64> count(i, 0);
			for (size_t ii = 0; ii < gameNumForIteration_; ++ii) {
				auto& bmds = bookMovesDatum_[ii];
				for (auto& bmd : bmds)
					if (bmd.useLearning)
						for (int j = 0; j < i; ++j)
							if (bmd.recordIsNth <= j)
								++count[j];
			}
			return count;
		};
		const auto total = total_move();
		std::cout << "\nGames = " << bookMovesDatum_.size()
				  << "\nTotal Moves = " << total
				  << "\nPrediction = ";
		const auto pred = prediction(8);
		for (auto elem : pred)
			std::cout << static_cast<double>(elem*100) / total << ", ";
		std::cout << std::endl;
		std::cout << "parse1 elapsed: " << t.elapsed() / 1000 << "[sec]" << std::endl;
	}
	static constexpr double FVPenalty() { return (0.2/static_cast<double>(FVScale)); }
	template <typename T>
	void updateFV(T& v, float dv) {
		const int step = count1s(mt_() & updateMaxMask_);
		if      (0 < v) dv -= static_cast<float>(FVPenalty());
		else if (v < 0) dv += static_cast<float>(FVPenalty());

		// T が enum だと 0 になることがある。
		// enum のときは、std::numeric_limits<std::underlying_type<T>::type>::max() などを使う。
		static_assert(std::numeric_limits<T>::max() != 0, "");
		static_assert(std::numeric_limits<T>::min() != 0, "");
		if      (0.0 <= dv && v <= std::numeric_limits<T>::max() - step) v += step;
		else if (dv <= 0.0 && std::numeric_limits<T>::min() + step <= v) v -= step;
	}
	void updateEval(const std::string& dirName) {
		for (size_t i = eval_.kpps_begin_index(), j = parse2Data_.params.kpps_begin_index(); i < eval_.kpps_end_index(); ++i, ++j)
			updateFV(eval_.oneArrayKPP[i], parse2Data_.params.oneArrayKPP[j]);
		for (size_t i = eval_.kkps_begin_index(), j = parse2Data_.params.kkps_begin_index(); i < eval_.kkps_end_index(); ++i, ++j)
			updateFV(eval_.oneArrayKKP[i], parse2Data_.params.oneArrayKKP[j]);
		for (size_t i = eval_.kks_begin_index(), j = parse2Data_.params.kks_begin_index(); i < eval_.kks_end_index(); ++i, ++j)
			updateFV(eval_.oneArrayKK[i], parse2Data_.params.oneArrayKK[j]);

		// 学習しないパラメータがある時は、一旦 write() で学習しているパラメータだけ書きこんで、再度読み込む事で、
		// updateFV()で学習しないパラメータに入ったノイズを無くす。
		eval_.write(dirName);
		eval_.init(dirName, false);
		g_evalTable.clear();
	}
	double sigmoid(const double x) const {
		const double a = 7.0/static_cast<double>(FVWindow);
		const double clipx = std::max(static_cast<double>(-FVWindow), std::min(static_cast<double>(FVWindow), x));
		return 1.0 / (1.0 + exp(-a * clipx));
	}
	double dsigmoid(const double x) const {
		if (x <= -FVWindow || FVWindow <= x) { return 0.0; }
#if 1
		// 符号だけが大切なので、定数掛ける必要は無い。
		const double a = 7.0/static_cast<double>(FVWindow);
		return a * sigmoid(x) * (1 - sigmoid(x));
#else
		// 定数掛けない方を使う。
		return sigmoid(x) * (1 - sigmoid(x));
#endif
	}
	void learnParse2Body(Position& pos, Parse2Data& parse2Data) {
		parse2Data.clear();
		SearchStack ss[2];
		for (size_t i = lockingIndexIncrement<false>(); i < gameNumForIteration_; i = lockingIndexIncrement<false>()) {
			StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
			pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
			auto& gameMoves = bookMovesDatum_[i];
			for (auto& bmd : gameMoves) {
#if defined PRINT_PV
				pos.print();
#endif
				if (bmd.useLearning && bmd.otherPVExist) {
					const Color rootColor = pos.turn();
					int recordPVIndex = 0;
#if defined PRINT_PV
					std::cout << "recordpv: ";
#endif
					for (; !bmd.pvBuffer[recordPVIndex].isNone(); ++recordPVIndex) {
#if defined PRINT_PV
						std::cout << bmd.pvBuffer[recordPVIndex].toCSA();
#endif
						setUpStates->push(StateInfo());
						pos.doMove(bmd.pvBuffer[recordPVIndex], setUpStates->top());
					}
					// evaluate() の差分計算を無効化する。
					ss[0].staticEvalRaw = ss[1].staticEvalRaw = ScoreNotEvaluated;
					const Score recordScore = (rootColor == pos.turn() ? evaluate(pos, ss+1) : -evaluate(pos, ss+1));
#if defined PRINT_PV
					std::cout << ", score: " << recordScore << std::endl;
#endif
					for (int jj = recordPVIndex - 1; 0 <= jj; --jj) {
						pos.undoMove(bmd.pvBuffer[jj]);
					}

					double sum_dT = 0.0;
					for (int otherPVIndex = recordPVIndex + 1; otherPVIndex < static_cast<int>(bmd.pvBuffer.size()); ++otherPVIndex) {
#if defined PRINT_PV
						std::cout << "otherpv : ";
#endif
						for (; !bmd.pvBuffer[otherPVIndex].isNone(); ++otherPVIndex) {
#if defined PRINT_PV
							std::cout << bmd.pvBuffer[otherPVIndex].toCSA();
#endif
							setUpStates->push(StateInfo());
							pos.doMove(bmd.pvBuffer[otherPVIndex], setUpStates->top());
						}
						ss[0].staticEvalRaw = ss[1].staticEvalRaw = ScoreNotEvaluated;
						const Score score = (rootColor == pos.turn() ? evaluate(pos, ss+1) : -evaluate(pos, ss+1));
						const auto diff = score - recordScore;
						const double dT = (rootColor == Black ? dsigmoid(diff) : -dsigmoid(diff));
#if defined PRINT_PV
						std::cout << ", score: " << score << ", dT: " << dT << std::endl;
#endif
						sum_dT += dT;
						parse2Data.params.incParam(pos, -dT);
						for (int jj = otherPVIndex - 1; !bmd.pvBuffer[jj].isNone(); --jj) {
							pos.undoMove(bmd.pvBuffer[jj]);
						}
					}

					for (int jj = 0; jj < recordPVIndex; ++jj) {
						setUpStates->push(StateInfo());
						pos.doMove(bmd.pvBuffer[jj], setUpStates->top());
					}
					parse2Data.params.incParam(pos, sum_dT);
					for (int jj = recordPVIndex - 1; 0 <= jj; --jj) {
						pos.undoMove(bmd.pvBuffer[jj]);
					}
				}
				setUpStates->push(StateInfo());
				pos.doMove(bmd.move, setUpStates->top());
			}
		}
	}
	void learnParse2(Position& pos) {
		Time t;
		for (int step = 1; step <= stepNum_; ++step) {
			t.restart();
			std::cout << "step " << step << "/" << stepNum_ << " " << std::flush;
			index_ = 0;
			std::vector<std::thread> threads(positions_.size());
			for (size_t i = 0; i < positions_.size(); ++i)
				threads[i] = std::thread([this, i] { learnParse2Body(positions_[i], parse2Datum_[i]); });
			learnParse2Body(pos, parse2Data_);
			for (auto& thread : threads)
				thread.join();

			for (auto& parse2 : parse2Datum_) {
				parse2Data_.params += parse2.params;
			}
			parse2Data_.params.lowerDimension();
			std::cout << "update eval ... " << std::flush;
			updateEval(pos.searcher()->options["Eval_Dir"]);
			std::cout << "done" << std::endl;
			std::cout << "parse2 1 step elapsed: " << t.elapsed() / 1000 << "[sec]" << std::endl;
			print();
		}
	}
	void print() {
		for (Rank r = Rank9; r < RankNum; ++r) {
			for (File f = FileA; FileI <= f; --f) {
				const Square sq = makeSquare(f, r);
				printf("%5d", Evaluater::KPP[B2][f_gold + C2][f_gold + sq]);
			}
			printf("\n");
		}
		printf("\n");
		fflush(stdout);
	}

	std::mutex mutex_;
	size_t index_;
	Ply minDepth_;
	Ply maxDepth_;
	std::mt19937 mt_;
	std::vector<std::mt19937> mts_;
	std::vector<Position> positions_;
	std::vector<std::vector<BookMoveData> > bookMovesDatum_;
	Parse2Data parse2Data_;
	std::vector<Parse2Data> parse2Datum_;
	Evaluater eval_;
	int stepNum_;
	size_t gameNumForIteration_;
	u64 updateMaxMask_;

	static const Score FVWindow = static_cast<Score>(256);
};

#endif

#endif // #ifndef APERY_LEARNER_HPP
