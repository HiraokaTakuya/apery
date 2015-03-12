#include "generateMoves.hpp"
#include "usi.hpp"

namespace {
	// 馬, 龍の場合
	template <MoveType MT, PieceType PT, Color US>
	FORCE_INLINE MoveStack* generateHorseOrDragonMoves(MoveStack* moveStackList, const Position& pos,
													   const Bitboard& target, const Square ksq)
	{
		Bitboard fromBB = pos.bbOf(PT, US);
		while (fromBB.isNot0()) {
			const Square from = fromBB.firstOneFromI9();
			Bitboard toBB = pos.attacksFrom<PT>(US, from) & target;
			while (toBB.isNot0()) {
				const Square to = toBB.firstOneFromI9();
				(*moveStackList++).move = makeNonPromoteMove<MT>(PT, from, to, pos);
			}
		}
		return moveStackList;
	}

	// 角, 飛車の場合
	template <MoveType MT, PieceType PT, Color US, bool ALL>
	FORCE_INLINE MoveStack* generateBishopOrRookMoves(MoveStack* moveStackList, const Position& pos,
													  const Bitboard& target, const Square ksq)
	{
		Bitboard fromBB = pos.bbOf(PT, US);

		// Txxx は先手、後手の情報を吸収した変数。数字は先手に合わせている。
		const Rank TRank6 = (US == Black ? Rank6 : Rank4);
		const Bitboard TRank789BB = inFrontMask<US, TRank6>();

		if (MT == NonCaptureMinusPro) {
			fromBB.andEqualNot(TRank789BB);
		}
		else {
			Bitboard fromOn789BB = fromBB & TRank789BB;
			// from が 1,2,3 段目にある。NonEvasion, ALL 以外は必ず成る。
			if (fromOn789BB.isNot0()) {
				const Bitboard target_tmp =
					(MT == Capture       ) ? pos.bbOf(oppositeColor(US)) :
					(MT == NonCapture    ) ? pos.emptyBB()               :
					(MT == CapturePlusPro) ? ~pos.bbOf(US)               :
					(MT == NonEvasion    ) ? ~pos.bbOf(US)               :
					(MT == Evasion       ) ? target                      :
					allOneBB(); // error
				assert(target_tmp != allOneBB());

				fromBB.andEqualNot(TRank789BB);
				do {
					const Square from = fromOn789BB.firstOneFromI9();
					Bitboard toBB = pos.attacksFrom<PT>(US, from) & target_tmp;
					while (toBB.isNot0()) {
						const Square to = toBB.firstOneFromI9();
						(*moveStackList++).move = makePromoteMove<MT>(PT, from, to, pos);
						if (MT == NonEvasion || ALL) {
							(*moveStackList++).move = makeNonPromoteMove<MT>(PT, from, to, pos);
						}
					}
				} while (fromOn789BB.isNot0());
			}
		}

		// from が 1,2,3 段目以外にある。 to が 1,2,3 段目ならば成る。
		while (fromBB.isNot0()) {
			const Square from = fromBB.firstOneFromI9();
			Bitboard toBB = pos.attacksFrom<PT>(US, from) & target;
			if (MT != NonCaptureMinusPro) {
				Bitboard toOn789BB = toBB & TRank789BB;
				// 成り
				if (toOn789BB.isNot0()) {
					toBB.andEqualNot(TRank789BB);
					do {
						const Square to = toOn789BB.firstOneFromI9();
						(*moveStackList++).move = makePromoteMove<MT>(PT, from, to, pos);
						if (MT == NonEvasion) {
							(*moveStackList++).move = makeNonPromoteMove<MT>(PT, from, to, pos);
						}
					} while (toOn789BB.isNot0());
				}
			}
			// 不成
			while (toBB.isNot0()) {
				const Square to = toBB.firstOneFromI9();
				(*moveStackList++).move = makeNonPromoteMove<MT>(PT, from, to, pos);
			}
		}

		return moveStackList;
	}

	// 駒打ちの場合
	// 歩以外の持ち駒は、loop の前に持ち駒の種類の数によって switch で展開している。
	// ループの展開はコードが膨れ上がる事によるキャッシュヒット率の低下と、演算回数のバランスを取って決める必要がある。
	// NPSに影響が出ないならシンプルにした方が良さそう。
	template <Color US>
	MoveStack* generateDropMoves(MoveStack* moveStackList, const Position& pos, const Bitboard& target) {
		const Hand hand = pos.hand(US);
		// まず、歩に対して指し手を生成
		if (hand.exists<HPawn>()) {
			Bitboard toBB = target;
			// 一段目には打てない
			const Rank TRank9 = (US == Black ? Rank9 : Rank1);
			toBB.andEqualNot(rankMask<TRank9>());

			// 二歩の回避
			Bitboard pawnsBB = pos.bbOf(Pawn, US);
			Square pawnsSquare;
			foreachBB(pawnsBB, pawnsSquare, [&](const int part) {
					toBB.set(part, toBB.p(part) & ~squareFileMask(pawnsSquare).p(part));
				});

			// 打ち歩詰めの回避
			const Rank TRank1 = (US == Black ? Rank1 : Rank9);
			const SquareDelta TDeltaS = (US == Black ? DeltaS : DeltaN);

			const Square ksq = pos.kingSquare(oppositeColor(US));
			// 相手玉が九段目なら、歩で王手出来ないので、打ち歩詰めを調べる必要はない。
			if (makeRank(ksq) != TRank1) {
				const Square pawnDropCheckSquare = ksq + TDeltaS;
				assert(isInSquare(pawnDropCheckSquare));
				if (toBB.isSet(pawnDropCheckSquare) && pos.piece(pawnDropCheckSquare) == Empty) {
					if (!pos.isPawnDropCheckMate(US, pawnDropCheckSquare)) {
						// ここで clearBit だけして MakeMove しないことも出来る。
						// 指し手が生成される順番が変わり、王手が先に生成されるが、後で問題にならないか?
						(*moveStackList++).move = makeDropMove(Pawn, pawnDropCheckSquare);
					}
					toBB.xorBit(pawnDropCheckSquare);
				}
			}

			Square to;
			FOREACH_BB(toBB, to, {
					(*moveStackList++).move = makeDropMove(Pawn, to);
				});
		}

		// 歩 以外の駒を持っているか
		if (hand.exceptPawnExists()) {
			PieceType haveHand[6]; // 歩以外の持ち駒。vector 使いたいけど、速度を求めるので使わない。
			int haveHandNum = 0; // 持ち駒の駒の種類の数

			// 桂馬、香車、それ以外の順番で格納する。(駒を打てる位置が限定的な順)
			if (hand.exists<HKnight>()) { haveHand[haveHandNum++] = Knight; }
			const int noKnightIdx      = haveHandNum; // 桂馬を除く駒でループするときのループの初期値
			if (hand.exists<HLance >()) { haveHand[haveHandNum++] = Lance;  }
			const int noKnightLanceIdx = haveHandNum; // 桂馬, 香車を除く駒でループするときのループの初期値
			if (hand.exists<HSilver>()) { haveHand[haveHandNum++] = Silver; }
			if (hand.exists<HGold  >()) { haveHand[haveHandNum++] = Gold;   }
			if (hand.exists<HBishop>()) { haveHand[haveHandNum++] = Bishop; }
			if (hand.exists<HRook  >()) { haveHand[haveHandNum++] = Rook;   }

			const Rank TRank8 = (US == Black ? Rank8 : Rank2);
			const Rank TRank9 = (US == Black ? Rank9 : Rank1);
			const Bitboard TRank8BB = rankMask<TRank8>();
			const Bitboard TRank9BB = rankMask<TRank9>();

			Bitboard toBB;
			Square to;
			// 桂馬、香車 以外の持ち駒があれば、
			// 一段目に対して、桂馬、香車以外の指し手を生成。
			switch (haveHandNum - noKnightLanceIdx) {
			case 0: break; // 桂馬、香車 以外の持ち駒がない。
			case 1: toBB = target & TRank9BB; FOREACH_BB(toBB, to, { Unroller<1>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightLanceIdx + i], to); }); }); break;
			case 2: toBB = target & TRank9BB; FOREACH_BB(toBB, to, { Unroller<2>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightLanceIdx + i], to); }); }); break;
			case 3: toBB = target & TRank9BB; FOREACH_BB(toBB, to, { Unroller<3>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightLanceIdx + i], to); }); }); break;
			case 4: toBB = target & TRank9BB; FOREACH_BB(toBB, to, { Unroller<4>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightLanceIdx + i], to); }); }); break;
			default: UNREACHABLE;
			}

			// 桂馬以外の持ち駒があれば、
			// 二段目に対して、桂馬以外の指し手を生成。
			switch (haveHandNum - noKnightIdx) {
			case 0: break; // 桂馬 以外の持ち駒がない。
			case 1: toBB = target & TRank8BB; FOREACH_BB(toBB, to, { Unroller<1>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightIdx + i], to); }); }); break;
			case 2: toBB = target & TRank8BB; FOREACH_BB(toBB, to, { Unroller<2>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightIdx + i], to); }); }); break;
			case 3: toBB = target & TRank8BB; FOREACH_BB(toBB, to, { Unroller<3>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightIdx + i], to); }); }); break;
			case 4: toBB = target & TRank8BB; FOREACH_BB(toBB, to, { Unroller<4>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightIdx + i], to); }); }); break;
			case 5: toBB = target & TRank8BB; FOREACH_BB(toBB, to, { Unroller<5>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[noKnightIdx + i], to); }); }); break;
			default: UNREACHABLE;
			}

			// 一、二段目以外に対して、全ての持ち駒の指し手を生成。
			toBB = target & ~(TRank8BB | TRank9BB);
			switch (haveHandNum) {
			case 0: assert(false); break; // 最適化の為のダミー
			case 1: FOREACH_BB(toBB, to, { Unroller<1>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[i], to); }); }); break;
			case 2: FOREACH_BB(toBB, to, { Unroller<2>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[i], to); }); }); break;
			case 3: FOREACH_BB(toBB, to, { Unroller<3>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[i], to); }); }); break;
			case 4: FOREACH_BB(toBB, to, { Unroller<4>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[i], to); }); }); break;
			case 5: FOREACH_BB(toBB, to, { Unroller<5>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[i], to); }); }); break;
			case 6: FOREACH_BB(toBB, to, { Unroller<6>()([&](const int i) { (*moveStackList++).move = makeDropMove(haveHand[i], to); }); }); break;
			default: UNREACHABLE;
			}
		}

		return moveStackList;
	}

	// 金, 成り金をまとめて指し手生成
	template <MoveType MT, PieceType PT, Color US, bool ALL> struct GeneratePieceMoves {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			STATIC_ASSERT(PT == Gold || PT == ProPawn || PT == ProLance || PT == ProKnight || PT == ProSilver);
			// 金、成金のbitboardをまとめて扱う。
			// todo: 金、成金 をまとめたbitboardをPositionクラスが持つべきか検討すること。
			Bitboard fromBB = pos.goldsBB(US);
			while (fromBB.isNot0()) {
				const Square from = fromBB.firstOneFromI9();
				Bitboard toBB = pos.attacksFrom<Gold>(US, from) & target;
				// from にある駒の種類を判別
				const PieceType pt = pieceToPieceType(pos.piece(from));
				while (toBB.isNot0()) {
					const Square to = toBB.firstOneFromI9();
					(*moveStackList++).move = makeNonPromoteMove<MT>(pt, from, to, pos);
				}
			}
			return moveStackList;
		}
	};
	// 歩の場合
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Pawn, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			// Txxx は先手、後手の情報を吸収した変数。数字は先手に合わせている。
			const Rank TRank6 = (US == Black ? Rank6 : Rank4);
			const Bitboard TRank789BB = inFrontMask<US, TRank6>();
			const SquareDelta TDeltaS = (US == Black ? DeltaS : DeltaN);

			Bitboard toBB = pawnAttack<US>(pos.bbOf(Pawn, US)) & target;

			// 成り
			if (MT != NonCaptureMinusPro) {
				Bitboard toOn789BB = toBB & TRank789BB;
				if (toOn789BB.isNot0()) {
					toBB.andEqualNot(TRank789BB);
					Square to;
					FOREACH_BB(toOn789BB, to, {
							const Square from = to + TDeltaS;
							(*moveStackList++).move = makePromoteMove<MT>(Pawn, from, to, pos);
							if (MT == NonEvasion || ALL) {
								const Rank TRank9 = (US == Black ? Rank9 : Rank1);
								if (makeRank(to) != TRank9) {
									(*moveStackList++).move = makeNonPromoteMove<MT>(Pawn, from, to, pos);
								}
							}
						});
				}
			}
			else {
				assert(!(target & TRank789BB).isNot0());
			}

			// 残り(不成)
			// toBB は 8~4 段目まで。
			Square to;
			FOREACH_BB(toBB, to, {
					const Square from = to + TDeltaS;
					(*moveStackList++).move = makeNonPromoteMove<MT>(Pawn, from, to, pos);
				});
			return moveStackList;
		}
	};
	// 香車の場合
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Lance, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			Bitboard fromBB = pos.bbOf(Lance, US);

			// bitboard のレイアウトが縦になっているので、from と to が同じ 64bit 変数に収まる。
			// それを利用して、片方の 64bit 変数のみにアクセスするようにしている。
			// 本当は、attacks を Bitboard 型で生成せずに、u64 で生成するようにした方が速度的には良い。
			Square from;
			foreachBB(fromBB, from, [&](const int part) {
					Bitboard toBB;
					toBB.set(part, pos.attacksFrom<Lance>(US, from).p(part) & target.p(part));
					while (toBB.p(part)) {
						// インライン化されれば、三項演算は最適化で消えるはず。
						const Square to = (part == 0 ? toBB.firstOneRightFromI9() : toBB.firstOneLeftFromB9());
						if (MT == Capture || MT == NonCapture || MT == NonEvasion || MT == Evasion || MT == CapturePlusPro) {
							const Rank toRank = makeRank(to);
							if (isInFrontOf<US, Rank7, Rank3>(toRank)) {
								// 1, 2 段目は成りのみを生成する。
								(*moveStackList++).move = makePromoteMove<MT>(Lance, from, to, pos);
								if (MT == NonEvasion || ALL) {
									// 1段目でなければ
									if (!isInFrontOf<US, Rank8, Rank2>(toRank)) {
										(*moveStackList++).move = makeNonPromoteMove<MT>(Lance, from, to, pos);
									}
								}
							}
							else if (isInFrontOf<US, Rank6, Rank4>(toRank)) {
								// 3 段目は成りと不成を生成する。
								(*moveStackList++).move = makePromoteMove<MT>(Lance, from, to, pos);
								if (MT == CapturePlusPro) {
									if (pos.piece(to) != Empty) {
										(*moveStackList++).move = makeNonPromoteMove<MT>(Lance, from, to, pos);
									}
								}
								else {
									(*moveStackList++).move = makeNonPromoteMove<MT>(Lance, from, to, pos);
								}
							}
							else {
								// それ以外は不成のみを生成する。
								(*moveStackList++).move = makeNonPromoteMove<MT>(Lance, from, to, pos);
							}
						}
						else if (MT == NonCaptureMinusPro) {
							// 常に不成。
							(*moveStackList++).move = makeNonPromoteMove<MT>(Lance, from, to, pos);
						}
						else {
							UNREACHABLE;
						}
					}
				});
			return moveStackList;
		}
	};
	// 桂馬の場合
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Knight, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			Bitboard fromBB = pos.bbOf(Knight, US);
			while (fromBB.isNot0()) {
				const Square from = fromBB.firstOneFromI9();
				Bitboard toBB = pos.attacksFrom<Knight>(US, from) & target;
				while (toBB.isNot0()) {
					const Square to = toBB.firstOneFromI9();
					if (MT == Capture || MT == NonCapture || MT == NonEvasion || MT == Evasion || MT == CapturePlusPro) {
						const Rank toRank = makeRank(to);
						if (isInFrontOf<US, Rank7, Rank3>(toRank)) {
							// 1, 2 段目は成りのみを生成する。
							(*moveStackList++).move = makePromoteMove<MT>(Knight, from, to, pos);
						}
						else if (isInFrontOf<US, Rank6, Rank4>(toRank)) {
							// 3 段目は成りと不成を生成する。
							(*moveStackList++).move = makePromoteMove<MT>(Knight, from, to, pos);
							if (MT == CapturePlusPro) {
								if (pos.piece(to) != Empty) {
									(*moveStackList++).move = makeNonPromoteMove<MT>(Knight, from, to, pos);
								}
							}
							else {
								(*moveStackList++).move = makeNonPromoteMove<MT>(Knight, from, to, pos);
							}
						}
						else {
							// それ以外は不成のみを生成する。
							(*moveStackList++).move = makeNonPromoteMove<MT>(Knight, from, to, pos);
						}
					}
					else if (MT == NonCaptureMinusPro) {
						// 常に不成。
						(*moveStackList++).move = makeNonPromoteMove<MT>(Knight, from, to, pos);
					}
					else {
						UNREACHABLE;
					}
				}
			}
			return moveStackList;
		}
	};
	// 銀の場合
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Silver, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			Bitboard fromBB = pos.bbOf(Silver, US);
			while (fromBB.isNot0()) {
				const Square from = fromBB.firstOneFromI9();
				const bool fromCanPromote = canPromote(US, makeRank(from));
				Bitboard toBB = pos.attacksFrom<Silver>(US, from) & target;
				while (toBB.isNot0()) {
					const Square to = toBB.firstOneFromI9();
					const bool toCanPromote = canPromote(US, makeRank(to));
					if (fromCanPromote | toCanPromote)
						(*moveStackList++).move = makePromoteMove<MT>(Silver, from, to, pos);
					(*moveStackList++).move = makeNonPromoteMove<MT>(Silver, from, to, pos);
				}
			}
			return moveStackList;
		}
	};
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Bishop, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			return generateBishopOrRookMoves<MT, Bishop, US, ALL>(moveStackList, pos, target, ksq);
		}
	};
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Rook, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			return generateBishopOrRookMoves<MT, Rook, US, ALL>(moveStackList, pos, target, ksq);
		}
	};
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Horse, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			return generateHorseOrDragonMoves<MT, Horse, US>(moveStackList, pos, target, ksq);
		}
	};
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, Dragon, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			return generateHorseOrDragonMoves<MT, Dragon, US>(moveStackList, pos, target, ksq);
		}
	};
	// 玉の場合
	// 必ず盤上に 1 枚だけあることを前提にすることで、while ループを 1 つ無くして高速化している。
	template <MoveType MT, Color US, bool ALL> struct GeneratePieceMoves<MT, King, US, ALL> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos, const Bitboard& target, const Square ksq) {
			const Square from = pos.kingSquare(US);
			Bitboard toBB = pos.attacksFrom<King>(US, from) & target;
			while (toBB.isNot0()) {
				const Square to = toBB.firstOneFromI9();
				(*moveStackList++).move = makeNonPromoteMove<MT>(King, from, to, pos);
			}
			return moveStackList;
		}
	};

	// pin は省かない。
	FORCE_INLINE MoveStack* generateRecaptureMoves(MoveStack* moveStackList, const Position& pos, const Square to, const Color us) {
		Bitboard fromBB = pos.attackersTo(us, to);
		while (fromBB.isNot0()) {
			const Square from = fromBB.firstOneFromI9();
			const PieceType pt = pieceToPieceType(pos.piece(from));
			switch (pt) {
			case Empty    : assert(false); break; // 最適化の為のダミー
			case Pawn     : case Lance    : case Knight   : case Silver   : case Bishop   : case Rook     :
				(*moveStackList++).move = ((canPromote(us, makeRank(to)) | canPromote(us, makeRank(from))) ?
										   makePromoteMove<Capture>(pt, from, to, pos) :
										   makeNonPromoteMove<Capture>(pt, from, to, pos));
				break;
			case Gold     : case King     : case ProPawn  : case ProLance : case ProKnight: case ProSilver: case Horse    : case Dragon   :
				(*moveStackList++).move = makeNonPromoteMove<Capture>(pt, from, to, pos);
				break;
			default       : UNREACHABLE;
			}
		}
		return moveStackList;
	}

	// 指し手生成 functor
	// テンプレート引数が複数あり、部分特殊化したかったので、関数ではなく、struct にした。
	// ALL == true のとき、歩、飛、角の不成、香の2段目の不成、香の3段目の駒を取らない不成も生成する。
	template <MoveType MT, Color US, bool ALL = false> struct GenerateMoves {
		MoveStack* operator () (MoveStack* moveStackList, const Position& pos) {
			STATIC_ASSERT(MT == Capture || MT == NonCapture || MT == CapturePlusPro || MT == NonCaptureMinusPro);
			// Txxx は先手、後手の情報を吸収した変数。数字は先手に合わせている。
			const Rank TRank6 = (US == Black ? Rank6 : Rank4);
			const Rank TRank7 = (US == Black ? Rank7 : Rank3);
			const Rank TRank8 = (US == Black ? Rank8 : Rank2);
			const Bitboard TRank789BB = inFrontMask<US, TRank6>();
			const Bitboard TRank1_6BB = inFrontMask<oppositeColor(US), TRank7>();
			const Bitboard TRank1_7BB = inFrontMask<oppositeColor(US), TRank8>();
			// promoted, king, silver, 
			const Bitboard target1 =
				(MT == Capture           ) ? pos.bbOf(oppositeColor(US)) :
				(MT == NonCapture        ) ? pos.emptyBB()               :
				(MT == CapturePlusPro    ) ? pos.bbOf(oppositeColor(US)) :
				(MT == NonCaptureMinusPro) ? pos.emptyBB()               :
				allOneBB(); // error

			// pawn, knight, lance, rook, bishop
			const Bitboard target2 =
				(MT == Capture           ) ? target1                                                        :
				(MT == NonCapture        ) ? target1                                                        :
				(MT == CapturePlusPro    ) ? pos.bbOf(oppositeColor(US)) | (pos.nOccupiedBB() & TRank789BB) :
				(MT == NonCaptureMinusPro) ? pos.nOccupiedBB() & TRank1_6BB                                 :
				allOneBB(); // error

			const Bitboard target3 =
				(MT == Capture           ) ? target2                        :
				(MT == NonCapture        ) ? target2                        :
				(MT == CapturePlusPro    ) ? target2                        :
				(MT == NonCaptureMinusPro) ? pos.nOccupiedBB() & TRank1_7BB :
				allOneBB(); // error

			const Square ksq = pos.kingSquare(oppositeColor(US));

			moveStackList = GeneratePieceMoves<MT, Pawn,   US, ALL>()(moveStackList, pos, target2, ksq);
			// 香車が駒を取らずに、敵陣3段目に不成で入ってくることはほぼあり得ない。(詰絡みなら厳密にはあり得ると思う。)
			// よって、target3 ではなく、target2 を引数にすることで、3段目への移動を省く。
			// 桂馬の場合は普通にあり得るので、省かない。
			moveStackList = GeneratePieceMoves<MT, Lance,  US, ALL>()(moveStackList, pos, (ALL ? target3 : target2), ksq);
			moveStackList = GeneratePieceMoves<MT, Knight, US, ALL>()(moveStackList, pos, target3, ksq);
			moveStackList = GeneratePieceMoves<MT, Silver, US, ALL>()(moveStackList, pos, target1, ksq);
			moveStackList = GeneratePieceMoves<MT, Bishop, US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<MT, Rook,   US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<MT, Gold,   US, ALL>()(moveStackList, pos, target1, ksq);
			moveStackList = GeneratePieceMoves<MT, King,   US, ALL>()(moveStackList, pos, target1, ksq);
			moveStackList = GeneratePieceMoves<MT, Horse,  US, ALL>()(moveStackList, pos, target1, ksq);
			moveStackList = GeneratePieceMoves<MT, Dragon, US, ALL>()(moveStackList, pos, target1, ksq);

			return moveStackList;
		}
	};

	// 部分特殊化
	// 駒打ち生成
	template <Color US> struct GenerateMoves<Drop, US> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos) {
			const Bitboard target = pos.emptyBB();
			moveStackList = generateDropMoves<US>(moveStackList, pos, target);
			return moveStackList;
		}
	};

	// checkSq にある駒で王手されたとき、玉はその駒の利きの位置には移動できないので、移動できない位置を bannnedKingToBB に格納する。
	// 両王手のときには二度連続で呼ばれるため、= ではなく |= を使用している。
	// 最初に呼ばれたときは、bannedKingToBB == allZeroBB() である。
	// todo: FOECE_INLINE と template 省いてNPS比較
	template <Color THEM>
	FORCE_INLINE void makeBannedKingTo(Bitboard& bannedKingToBB, const Position& pos,
									   const Square checkSq, const Square ksq)
	{
		switch (pos.piece(checkSq)) {
//		case Empty: assert(false); break; // 最適化の為のダミー
		case (THEM == Black ? BPawn      : WPawn):
		case (THEM == Black ? BKnight    : WKnight):
			// 歩、桂馬で王手したときは、どこへ逃げても、その駒で取られることはない。
			// よって、ここでは何もしない。
			assert(
				pos.piece(checkSq) == (THEM == Black ? BPawn   : WPawn) ||
				pos.piece(checkSq) == (THEM == Black ? BKnight : WKnight)
				);
		break;
		case (THEM == Black ? BLance     : WLance):
			bannedKingToBB |= lanceAttackToEdge(THEM, checkSq);
			break;
		case (THEM == Black ? BSilver    : WSilver):
			bannedKingToBB |= silverAttack(THEM, checkSq);
			break;
		case (THEM == Black ? BGold      : WGold):
		case (THEM == Black ? BProPawn   : WProPawn):
		case (THEM == Black ? BProLance  : WProLance):
		case (THEM == Black ? BProKnight : WProKnight):
		case (THEM == Black ? BProSilver : WProSilver):
			bannedKingToBB |= goldAttack(THEM, checkSq);
		break;
		case (THEM == Black ? BBishop    : WBishop):
			bannedKingToBB |= bishopAttackToEdge(checkSq);
			break;
		case (THEM == Black ? BHorse     : WHorse):
			bannedKingToBB |= horseAttackToEdge(checkSq);
			break;
		case (THEM == Black ? BRook      : WRook):
			bannedKingToBB |= rookAttackToEdge(checkSq);
			break;
		case (THEM == Black ? BDragon    : WDragon):
			if (squareRelation(checkSq, ksq) & DirecDiag) {
				// 斜めから王手したときは、玉の移動先と王手した駒の間に駒があることがあるので、
				// dragonAttackToEdge(checkSq) は使えない。
				bannedKingToBB |= pos.attacksFrom<Dragon>(checkSq);
			}
			else {
				bannedKingToBB |= dragonAttackToEdge(checkSq);
			}
			break;
		default:
			UNREACHABLE;
		}
	}

	// 部分特殊化
	// 王手回避生成
	// 王手をしている駒による王手は避けるが、
	// 玉の移動先に敵の利きがある場合と、pinされている味方の駒を動かした場合、非合法手を生成する。
	// そのため、pseudo legal である。
	template <Color US, bool ALL> struct GenerateMoves<Evasion, US, ALL> {
		/*FORCE_INLINE*/ MoveStack* operator () (MoveStack* moveStackList, const Position& pos) {
			assert(pos.isOK());
			assert(pos.inCheck());

			const Square ksq = pos.kingSquare(US);
			const Color Them = oppositeColor(US);
			const Bitboard checkers = pos.checkersBB();
			Bitboard bb = checkers;
			Bitboard bannedKingToBB = allZeroBB();
			int checkersNum = 0;
			Square checkSq;

			// 玉が逃げられない位置の bitboard を生成する。
			// 絶対に王手が掛かっているので、while ではなく、do while
			do {
				checkSq = bb.firstOneFromI9();
				assert(pieceToColor(pos.piece(checkSq)) == Them);
				++checkersNum;
				makeBannedKingTo<Them>(bannedKingToBB, pos, checkSq, ksq);
			} while (bb.isNot0());

			// 玉が移動出来る移動先を格納。
			bb = bannedKingToBB.notThisAnd(pos.bbOf(US).notThisAnd(kingAttack(ksq)));
			while (bb.isNot0()) {
				const Square to = bb.firstOneFromI9();
#if 0
				// 移動先に相手駒の利きがあればそれを省く。
				if (!pos.attackersToIsNot0(Them, to)) {
					(*moveStackList++).move = makeNonPromoteMove<Capture>(King, ksq, to, pos);
				}
#else
				// 移動先に相手駒の利きがあるか調べずに指し手を生成する。
				// attackersTo() が重いので、movePicker か search で合法手か調べる。
				(*moveStackList++).move = makeNonPromoteMove<Capture>(King, ksq, to, pos);
#endif
			}

			// 両王手なら、玉を移動するしか回避方法は無い。
			// 玉の移動は生成したので、ここで終了
			if (1 < checkersNum) {
				return moveStackList;
			}

			// 王手している駒を玉以外で取る手の生成。
			// pin されているかどうかは movePicker か search で調べる。
			const Bitboard target1 = betweenBB(checkSq, ksq);
			const Bitboard target2 = target1 | checkers;
			moveStackList = GeneratePieceMoves<Evasion, Pawn,   US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Lance,  US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Knight, US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Silver, US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Bishop, US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Rook,   US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Gold,   US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Horse,  US, ALL>()(moveStackList, pos, target2, ksq);
			moveStackList = GeneratePieceMoves<Evasion, Dragon, US, ALL>()(moveStackList, pos, target2, ksq);

			if (target1.isNot0()) {
				moveStackList = generateDropMoves<US>(moveStackList, pos, target1);
			}

			return moveStackList;
		}
	};

	// 部分特殊化
	// 王手が掛かっていないときの指し手生成
	// これには、玉が相手駒の利きのある地点に移動する自殺手と、pin されている駒を動かす自殺手を含む。
	// ここで生成した手は pseudo legal
	template <Color US> struct GenerateMoves<NonEvasion, US> {
		/*FORCE_INLINE*/ MoveStack* operator () (MoveStack* moveStackList, const Position& pos) {
			Bitboard target = pos.emptyBB();

			moveStackList = generateDropMoves<US>(moveStackList, pos, target);
			target |= pos.bbOf(oppositeColor(US));
			const Square ksq = pos.kingSquare(oppositeColor(US));

			moveStackList = GeneratePieceMoves<NonEvasion, Pawn,   US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Lance,  US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Knight, US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Silver, US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Bishop, US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Rook,   US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Gold,   US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, King,   US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Horse,  US, false>()(moveStackList, pos, target, ksq);
			moveStackList = GeneratePieceMoves<NonEvasion, Dragon, US, false>()(moveStackList, pos, target, ksq);

			return moveStackList;
		}
	};

	// 部分特殊化
	// 連続王手の千日手以外の反則手を排除した合法手生成
	// そんなに速度が要求されるところでは呼ばない。
	template <Color US> struct GenerateMoves<Legal, US> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos) {
			MoveStack* curr = moveStackList;
			const Bitboard pinned = pos.pinnedBB();

			moveStackList = pos.inCheck() ?
				GenerateMoves<Evasion, US>()(moveStackList, pos) : GenerateMoves<NonEvasion, US>()(moveStackList, pos);

			// 玉の移動による自殺手と、pinされている駒の移動による自殺手を削除
			while (curr != moveStackList) {
				if (!pos.pseudoLegalMoveIsLegal<false, false>(curr->move, pinned)) {
					curr->move = (--moveStackList)->move;
				}
				else {
					++curr;
				}
			}

			return moveStackList;
		}
	};

	// 部分特殊化
	// Evasion のときに歩、飛、角と、香の2段目の不成も生成する。
	template <Color US> struct GenerateMoves<LegalAll, US> {
		FORCE_INLINE MoveStack* operator () (MoveStack* moveStackList, const Position& pos) {
			MoveStack* curr = moveStackList;
			const Bitboard pinned = pos.pinnedBB();

			moveStackList = pos.inCheck() ?
				GenerateMoves<Evasion, US, true>()(moveStackList, pos) : GenerateMoves<NonEvasion, US>()(moveStackList, pos);

			// 玉の移動による自殺手と、pinされている駒の移動による自殺手を削除
			while (curr != moveStackList) {
				if (!pos.pseudoLegalMoveIsLegal<false, false>(curr->move, pinned)) {
					curr->move = (--moveStackList)->move;
				}
				else {
					++curr;
				}
			}

			return moveStackList;
		}
	};
}

template <MoveType MT>
MoveStack* generateMoves(MoveStack* moveStackList, const Position& pos) {
	return (pos.turn() == Black ?
			GenerateMoves<MT, Black>()(moveStackList, pos) : GenerateMoves<MT, White>()(moveStackList, pos));
}
template <MoveType MT>
MoveStack* generateMoves(MoveStack* moveStackList, const Position& pos, const Square to) {
	return generateRecaptureMoves(moveStackList, pos, to, pos.turn());
}

// 明示的なインスタンス化
// これが無いと、他のファイルから呼んだ時に、
// 実体が無いためにリンクエラーになる。
// ちなみに、特殊化されたテンプレート関数は、明示的なインスタンス化の必要はない。
// 実装を cpp に置くことで、コンパイル時間の短縮が出来る。
//template MoveStack* generateMoves<Capture           >(MoveStack* moveStackList, const Position& pos);
//template MoveStack* generateMoves<NonCapture        >(MoveStack* moveStackList, const Position& pos);
template MoveStack* generateMoves<Drop              >(MoveStack* moveStackList, const Position& pos);
template MoveStack* generateMoves<CapturePlusPro    >(MoveStack* moveStackList, const Position& pos);
template MoveStack* generateMoves<NonCaptureMinusPro>(MoveStack* moveStackList, const Position& pos);
template MoveStack* generateMoves<Evasion           >(MoveStack* moveStackList, const Position& pos);
template MoveStack* generateMoves<NonEvasion        >(MoveStack* moveStackList, const Position& pos);
template MoveStack* generateMoves<Legal             >(MoveStack* moveStackList, const Position& pos);
#if !defined NDEBUG || defined LEARN
template MoveStack* generateMoves<LegalAll          >(MoveStack* moveStackList, const Position& pos);
#endif
template MoveStack* generateMoves<Recapture         >(MoveStack* moveStackList, const Position& pos, const Square to);
