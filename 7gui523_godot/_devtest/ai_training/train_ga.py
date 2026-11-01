"""
Genetic Algorithm AI Weight Trainer for 七鬼523
================================================
Trains AI2 weights to beat AI1 (rule-based boss).
Pure Python, zero Godot dependency.

Usage:  python train_ga.py
Output: ai2_weights.json  →  convert to .tres for Godot
"""

import json
import math
import random
import time
from copy import deepcopy
from dataclasses import dataclass, field
from typing import List, Dict, Tuple, Optional

# =============================================================================
# Card System
# =============================================================================

RANK_MAP: Dict[str, int] = {
    "7": 15,
    "大鬼": 14, "小鬼": 13,
    "5": 12, "2": 11, "3": 10,
    "A": 9, "K": 8, "Q": 7, "J": 6,
    "10": 5, "9": 4, "8": 3, "6": 2, "4": 1,
}

SUITS = ["黑桃", "红桃", "梅花", "方块"]
RANKS = ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"]

TOTAL_PER_POINT: Dict[str, int] = {
    "A": 4, "K": 4, "Q": 4, "J": 4, "10": 4,
    "9": 4, "8": 4, "7": 4, "6": 4, "5": 4,
    "4": 4, "3": 4, "2": 4,
    "大鬼": 1, "小鬼": 1,
}


@dataclass
class Card:
    """Immutable card."""

    id: int
    point: str
    suit: str
    score: int

    def __repr__(self):
        return f"{self.point}{self.suit}"


def score_from_point(p: str) -> int:
    if p == "5":
        return 5
    if p == "10":
        return 10
    if p == "K":
        return 20
    return 0


def get_rank(p: str) -> int:
    if p == "王炸":
        return 100
    if p == "7gui523":
        return 999
    return RANK_MAP.get(p, 0)


# Build deck
ALL_54: List[Card] = []
_cid = 0
for suit in SUITS:
    for rank in RANKS:
        ALL_54.append(Card(id=_cid, point=rank, suit=suit, score=score_from_point(rank)))
        _cid += 1
ALL_54.append(Card(id=52, point="小鬼", suit="", score=0))
ALL_54.append(Card(id=53, point="大鬼", suit="", score=0))


# =============================================================================
# Card Type System
# =============================================================================

from enum import IntEnum


class CardType(IntEnum):
    INVALID = -2
    PASS = -1
    SINGLE = 0
    PAIR = 1
    TRIPLE = 2
    TRIPLE_WITH_ONE = 3
    TRIPLE_WITH_TWO = 4
    BOMB = 5
    ROCKET = 6
    FIVE_OF_A_KIND = 7
    GUI523 = 999


TYPE_RANK: Dict[CardType, int] = {
    CardType.INVALID: -2,
    CardType.PASS: -1,
    CardType.SINGLE: 0,
    CardType.PAIR: 1,
    CardType.TRIPLE: 2,
    CardType.TRIPLE_WITH_ONE: 3,
    CardType.TRIPLE_WITH_TWO: 4,
    CardType.BOMB: 5,
    CardType.ROCKET: 6,
    CardType.FIVE_OF_A_KIND: 7,
    CardType.GUI523: 999,
}


def _get_rank_count(cards: List[Card]) -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for c in cards:
        counts[c.point] = counts.get(c.point, 0) + 1
    return counts


def _is_same_point(cards: List[Card]) -> bool:
    if not cards:
        return False
    p = cards[0].point
    return all(c.point == p for c in cards)


def _is_joker(c: Card) -> bool:
    return c.point in ("大鬼", "小鬼")


def _is_rocket(cards: List[Card]) -> bool:
    if len(cards) != 2:
        return False
    pts = {c.point for c in cards}
    return pts == {"大鬼", "小鬼"}


def _is_gui523(cards: List[Card]) -> bool:
    if len(cards) != 5:
        return False
    pts = {c.point for c in cards}
    return pts & {"大鬼", "小鬼"} and "7" in pts and "5" in pts and "2" in pts and "3" in pts


def _has_triple(cards: List[Card]) -> str:
    for pt, n in _get_rank_count(cards).items():
        if pt in ("大鬼", "小鬼"):
            continue
        if n >= 3:
            return pt
    return ""


def _has_pair(cards: List[Card], exclude: str = "") -> str:
    for pt, n in _get_rank_count(cards).items():
        if pt in ("大鬼", "小鬼"):
            continue
        if pt == exclude:
            continue
        if n >= 2:
            return pt
    return ""


def _is_special(t: CardType) -> bool:
    return t in (CardType.BOMB, CardType.ROCKET, CardType.FIVE_OF_A_KIND, CardType.GUI523)


@dataclass
class CardTypeResult:
    type: CardType = CardType.INVALID
    key_point: str = ""
    kicker: str = ""
    count: int = 0


def parse_any_card_type(cards: List[Card]) -> CardTypeResult:
    r = CardTypeResult()
    n = len(cards)

    if n == 1:
        r.type = CardType.SINGLE
        r.key_point = cards[0].point
    elif n == 2:
        if _is_rocket(cards):
            r.type = CardType.ROCKET
            r.key_point = "王炸"
        elif _is_same_point(cards) and not _is_joker(cards[0]):
            r.type = CardType.PAIR
            r.key_point = cards[0].point
    elif n == 3:
        if _is_same_point(cards) and not _is_joker(cards[0]):
            r.type = CardType.TRIPLE
            r.key_point = cards[0].point
    elif n == 4:
        if _is_same_point(cards) and not _is_joker(cards[0]):
            r.type = CardType.BOMB
            r.key_point = cards[0].point
            r.count = 4
        else:
            tp = _has_triple(cards)
            if tp:
                r.type = CardType.TRIPLE_WITH_ONE
                r.key_point = tp
                for c in cards:
                    if c.point != tp:
                        r.kicker = c.point
                        break
    elif n >= 5:
        if _is_gui523(cards):
            r.type = CardType.GUI523
            r.key_point = "7gui523"
            return r
        # five of a kind
        if _is_same_point(cards) and not _is_joker(cards[0]):
            r.type = CardType.FIVE_OF_A_KIND
            r.key_point = cards[0].point
            r.count = n
        elif n == 5:
            tp = _has_triple(cards)
            if tp:
                pp = _has_pair(cards, tp)
                if pp:
                    r.type = CardType.TRIPLE_WITH_TWO
                    r.key_point = tp
                    r.kicker = pp

    return r


def can_beat(candidate: CardTypeResult, previous: CardTypeResult) -> bool:
    if previous.type in (CardType.INVALID, CardType.PASS) or not previous.key_point:
        return candidate.type != CardType.INVALID
    if candidate.type == CardType.INVALID:
        return False

    ra = TYPE_RANK.get(candidate.type, -99)
    rb = TYPE_RANK.get(previous.type, -99)
    sa = _is_special(candidate.type)
    sb = _is_special(previous.type)

    if ra != rb:
        if not sa and not sb:
            return False
        if sa and not sb:
            return True
        if not sa and sb:
            return False
        if sa and sb:
            return ra > rb

    cmp = get_rank(candidate.key_point) - get_rank(previous.key_point)
    if cmp > 0:
        return True
    if cmp < 0:
        return False

    if candidate.type in (CardType.TRIPLE_WITH_ONE, CardType.TRIPLE_WITH_TWO):
        if candidate.kicker and previous.kicker:
            return get_rank(candidate.kicker) >= get_rank(previous.kicker)

    if candidate.type in (CardType.FIVE_OF_A_KIND, CardType.BOMB):
        return candidate.count >= previous.count

    return True


# =============================================================================
# Player Simulation
# =============================================================================


@dataclass
class PlayerData:
    name: str = ""
    hand: List[Card] = field(default_factory=list)
    is_ai1: bool = False  # True = AI1 (fixed rule), False = AI3 (trained weights)

    def hand_size(self) -> int:
        return len(self.hand)

    def is_hand_empty(self) -> bool:
        return len(self.hand) == 0


# =============================================================================
# Enumerate Legal Plays
# =============================================================================


def enumerate_legal_plays(hand: List[Card]) -> List[List[Card]]:
    """Bitmask enumeration of all legal card combinations from hand."""
    result: List[List[Card]] = []
    n = len(hand)
    for mask in range(1, 1 << n):
        subset = [hand[i] for i in range(n) if mask & (1 << i)]
        parsed = parse_any_card_type(subset)
        if parsed.type != CardType.INVALID:
            result.append(subset)
    return result


# =============================================================================
# AI1 — Rule-based (identical to game boss)
# =============================================================================

AI1_ZERO_SCORE_PASS = True


def choose_ai1(hand: List[Card], prev: Optional[CardTypeResult]) -> List[Card]:
    all_plays = enumerate_legal_plays(hand)
    if not all_plays:
        return []

    # First play (table empty)
    if prev is None or not prev.key_point:
        if not hand:
            return []
        sorted_hand = sorted(hand, key=lambda c: get_rank(c.point))
        first_point = sorted_hand[0].point
        combo = [c for c in hand if c.point == first_point]
        if len(combo) >= 3:
            return [combo[0]]
        return combo

    same_type: List[List[Card]] = []
    bombs: List[List[Card]] = []
    fives: List[List[Card]] = []
    rockets: List[List[Card]] = []

    for play in all_plays:
        parsed = parse_any_card_type(play)
        if not can_beat(parsed, prev):
            continue
        if parsed.type == CardType.FIVE_OF_A_KIND:
            fives.append(play)
        elif parsed.type == CardType.BOMB:
            bombs.append(play)
        elif parsed.type == CardType.ROCKET:
            rockets.append(play)
        else:
            same_type.append(play)

    if same_type:
        best = same_type[0]
        best_rank = get_rank(parse_any_card_type(best).key_point)
        for play in same_type:
            r = get_rank(parse_any_card_type(play).key_point)
            if r < best_rank:
                best = play
                best_rank = r
        return best

    if AI1_ZERO_SCORE_PASS and bombs:
        return []

    if bombs:
        best = bombs[0]
        best_rank = get_rank(parse_any_card_type(best).key_point)
        for play in bombs:
            r = get_rank(parse_any_card_type(play).key_point)
            if r < best_rank:
                best = play
                best_rank = r
        return best

    if fives:
        best = fives[0]
        best_rank = get_rank(parse_any_card_type(best).key_point)
        for play in fives:
            r = get_rank(parse_any_card_type(play).key_point)
            if r < best_rank:
                best = play
                best_rank = r
        return best

    if rockets:
        return rockets[0]

    return []


# =============================================================================
# AI3 — Weighted scoring (training target)
# =============================================================================

PARAMETER_NAMES = [
    "finish_hand_reward",
    "finish_round_reward",
    "clear_table_reward",
    "reduce_hand_size_reward",
    "play_card_count_cost",
    "rank_cost",
    "high_card_cost",
    "low_card_release_reward",
    "break_pair_cost",
    "break_triple_cost",
    "break_bomb_cost",
    "break_five_kind_cost",
    "break_rocket_cost",
    "preserve_same_point_reward",
    "preserve_bomb_reward",
    "preserve_rocket_reward",
    "preserve_gui523_reward",
    "beat_same_type_reward",
    "beat_bomb_reward",
    "opponent_low_hand_reward",
    "opponent_empty_hand_penalty",
    "counter_risk_cost",
    "table_score_reward",
    "score_card_reward",
    "five_card_score_reward",
    "damage_per_point_reward",
    "round_damage_reward",
    "large_damage_reward",
    "exhausted_point_reward",
    "rare_point_reward",
    "known_opponent_card_reward",
    "unknown_card_risk_cost",
    "remaining_deck_risk_cost",
]


def default_weights() -> List[float]:
    """Same defaults as AIWeights.default_weights() in Godot."""
    return [
        100000.0,  # finish_hand_reward
        1000.0,  # finish_round_reward
        100.0,  # clear_table_reward
        0.1,  # reduce_hand_size_reward
        1.0,  # play_card_count_cost
        10.0,  # rank_cost
        10.0,  # high_card_cost
        1.0,  # low_card_release_reward
        1000.0,  # break_pair_cost
        1000.0,  # break_triple_cost
        1000.0,  # break_bomb_cost
        1000.0,  # break_five_kind_cost
        1000.0,  # break_rocket_cost
        100.0,  # preserve_same_point_reward
        1000.0,  # preserve_bomb_reward
        1000.0,  # preserve_rocket_reward
        100.0,  # preserve_gui523_reward
        1000.0,  # beat_same_type_reward
        -1000.0,  # beat_bomb_reward
        10000.0,  # opponent_low_hand_reward
        1000.0,  # opponent_empty_hand_penalty
        1.0,  # counter_risk_cost
        1.0,  # table_score_reward
        10.0,  # score_card_reward
        1000.0,  # five_card_score_reward
        1.0,  # damage_per_point_reward
        1.0,  # round_damage_reward
        1000.0,  # large_damage_reward
        10.0,  # exhausted_point_reward
        1.0,  # rare_point_reward
        100.0,  # known_opponent_card_reward
        1.0,  # unknown_card_risk_cost
        0.1,  # remaining_deck_risk_cost
    ]


def weights_to_dict(w: List[float]) -> Dict[str, float]:
    return {PARAMETER_NAMES[i]: w[i] for i in range(len(PARAMETER_NAMES))}


# Helper: what combo size did we break in the hand?
def _broken_combo_size(play: List[Card], hand: List[Card]) -> int:
    hand_count: Dict[str, int] = {}
    for c in hand:
        hand_count[c.point] = hand_count.get(c.point, 0) + 1
    play_count: Dict[str, int] = {}
    for c in play:
        play_count[c.point] = play_count.get(c.point, 0) + 1
    worst = 0
    for pt, pc in play_count.items():
        in_hand = hand_count.get(pt, 0)
        left = in_hand - pc
        if in_hand >= 2 and left >= 1:
            worst = max(worst, in_hand)
    return worst


def _breaks_rocket(play: List[Card], hand: List[Card]) -> bool:
    hand_pts = {c.point for c in hand}
    if "大鬼" not in hand_pts or "小鬼" not in hand_pts:
        return False
    play_pts = {c.point for c in play}
    return ("大鬼" in play_pts) != ("小鬼" in play_pts)


def _rest_counts(play: List[Card], hand: List[Card]) -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for c in hand:
        counts[c.point] = counts.get(c.point, 0) + 1
    for c in play:
        counts[c.point] = counts.get(c.point, 0) - 1
        if counts[c.point] <= 0:
            del counts[c.point]
    return counts


def weighted_gain(
    play: List[Card],
    hand: List[Card],
    opp_hand_size: int,
    parsed: CardTypeResult,
    previous: Optional[CardTypeResult],
    table_score: int,
    w: List[float],
    tracker_played: Dict[str, int],
) -> float:
    gain = 0.0
    has_prev = previous is not None and previous.key_point != ""
    is_bomb = parsed.type in (CardType.BOMB, CardType.ROCKET, CardType.FIVE_OF_A_KIND)

    play_score = sum(c.score for c in play)

    # finish hand
    if len(play) == len(hand):
        gain += w[0]  # finish_hand_reward

    # lead / follow
    if has_prev:
        gain += w[17] if not is_bomb else w[18]  # beat_same_type / beat_bomb_reward
        if opp_hand_size <= 2 and is_bomb:
            gain += w[19]  # opponent_low_hand_reward
        elif opp_hand_size <= 2:
            gain -= w[20]  # opponent_empty_hand_penalty
        gain -= get_rank(parsed.key_point) * w[5]  # rank_cost
    else:
        gain -= get_rank(parsed.key_point) * w[6]  # high_card_cost
        gain += play_score * w[25]  # damage_per_point_reward

    # break combo cost
    match _broken_combo_size(play, hand):
        case 2:
            gain -= w[8]  # break_pair_cost
        case 3:
            gain -= w[9]  # break_triple_cost
        case 4:
            gain -= w[10]  # break_bomb_cost
        case 5:
            gain -= w[11]  # break_five_kind_cost
    if _breaks_rocket(play, hand):
        gain -= w[12]  # break_rocket_cost

    # score cards
    for c in play:
        if table_score > 0:
            gain += c.score * w[23]  # score_card_reward
        else:
            gain += c.score * w[7]  # low_card_release_reward
    if table_score > 0 and play_score >= table_score:
        gain += w[2]  # clear_table_reward

    if parsed.type == CardType.FIVE_OF_A_KIND:
        gain += w[24]  # five_card_score_reward

    # preserve potential in remaining hand
    rest = _rest_counts(play, hand)
    pairs_kept = sum(1 for pt, n in rest.items() if n >= 2)
    bomb_kept = any(n >= 4 for n in rest.values())
    if pairs_kept > 0:
        gain += w[13] * min(pairs_kept, 3)  # preserve_same_point_reward
    if bomb_kept:
        gain += w[14]  # preserve_bomb_reward
    if rest.get("大鬼", 0) > 0 and rest.get("小鬼", 0) > 0:
        gain += w[15]  # preserve_rocket_reward
    if rest.get("7", 0) > 0 and rest.get("5", 0) > 0 and rest.get("2", 0) > 0 and rest.get("3", 0) > 0:
        gain += w[16]  # preserve_gui523_reward

    # tracker info (simplified)
    for c in play:
        remaining = TOTAL_PER_POINT.get(c.point, 4) - tracker_played.get(c.point, 0)
        if remaining <= 0:
            gain += w[28]  # exhausted_point_reward
        elif remaining <= 2:
            gain += w[29]  # rare_point_reward

    # round finish heuristic
    ends_round = has_prev  # simplified: if we can beat, assume we end round
    if ends_round:
        gain += w[1]  # finish_round_reward
        gain += table_score * w[22]  # table_score_reward
    if len(play) == len(hand) or ends_round:
        expected_damage = table_score + play_score
        gain += expected_damage * w[26]  # round_damage_reward
        if expected_damage >= 30:
            gain += w[27]  # large_damage_reward

    # hand size
    gain += len(play) * w[3]  # reduce_hand_size_reward
    gain -= len(play) * w[4]  # play_card_count_cost

    return gain


# Special plays (hard rules before scoring)
def try_finish_play(hand: List[Card], all_plays: List[List[Card]], prev: Optional[CardTypeResult]) -> List[Card]:
    if prev is None or not prev.key_point:
        if all_plays:
            last = all_plays[-1]
            if parse_any_card_type(last).type == CardType.GUI523:
                return last
    return []


def try_endgame_intercept(
    all_plays: List[List[Card]], prev: Optional[CardTypeResult], opp_hand_size: int
) -> List[Card]:
    if opp_hand_size > 2:
        return []
    for play in all_plays:
        parsed = parse_any_card_type(play)
        if prev is not None and prev.key_point and not can_beat(parsed, prev):
            continue
        if parsed.type in (CardType.ROCKET, CardType.BOMB):
            return play
    return []


def choose_ai3(
    hand: List[Card],
    opp_hand: List[Card],
    prev: Optional[CardTypeResult],
    table_score: int,
    weights: List[float],
    tracker_played: Dict[str, int],
) -> List[Card]:
    all_plays = enumerate_legal_plays(hand)
    if not all_plays:
        return []

    finish = try_finish_play(hand, all_plays, prev)
    if finish:
        return finish

    intercept = try_endgame_intercept(all_plays, prev, len(opp_hand))
    if intercept:
        return intercept

    best: List[Card] = []
    best_gain = float("-inf")
    for play in all_plays:
        parsed = parse_any_card_type(play)
        if prev is not None and prev.key_point and not can_beat(parsed, prev):
            continue
        gain = weighted_gain(play, hand, len(opp_hand), parsed, prev, table_score, weights, tracker_played)
        if gain > best_gain:
            best_gain = gain
            best = play

    # fallback: if no legal play, return empty (PASS)
    if not best:
        if prev is None or not prev.key_point:
            # free round: must play smallest single
            min_card = min(hand, key=lambda c: get_rank(c.point))
            return [min_card]
    return best


# =============================================================================
# Battle Simulation
# =============================================================================


def _make_deck() -> List[int]:
    """Create a shuffled deck of 54 card IDs (independent per player)."""
    ids = list(range(54))
    random.shuffle(ids)
    return ids


def _draw_up_to(hand: List[Card], deck: List[int], target: int) -> None:
    """Draw from deck until hand has `target` cards or deck is empty."""
    while len(hand) < target and deck:
        cid = deck.pop()
        hand.append(deepcopy(ALL_54[cid]))


def simulate_one_game(
    weights: List[float], ai3_first: bool
) -> float:  # AI3 net chips (positive = AI3 gained more)
    """
    Simulate one full game. Returns net chip difference (AI3 - AI1).
    Each round winner collects the pot (table_score + card scores).
    """
    # Independent decks per player
    deck3 = _make_deck()
    deck1 = _make_deck()

    hand3: List[Card] = []
    hand1: List[Card] = []
    _draw_up_to(hand3, deck3, 5)
    _draw_up_to(hand1, deck1, 5)

    tracker3: Dict[str, int] = {}
    tracker1: Dict[str, int] = {}

    ai3_chips = 0
    ai1_chips = 0

    # Who leads the first round
    leader = "ai3" if ai3_first else "ai1"

    for _ in range(500):
        table_score = 0
        prev: Optional[CardTypeResult] = None
        current = leader

        # --- One round ---
        for __ in range(100):  # round safety limit
            if current == "ai3":
                hand = hand3
                opp = hand1
                play = choose_ai3(hand, opp, prev, table_score, weights, tracker3)
            else:
                hand = hand1
                opp = hand3
                play = choose_ai1(hand, prev)

            # Free round must play something
            if not play and (prev is None or not prev.key_point):
                if hand:
                    play = [min(hand, key=lambda c: get_rank(c.point))]

            if not play:
                # PASS: opponent (last player who played) wins this round
                if prev is not None and prev.key_point:
                    winner = "ai1" if current == "ai3" else "ai3"
                    if winner == "ai3":
                        ai3_chips += table_score
                    else:
                        ai1_chips += table_score
                    leader = winner
                else:
                    leader = "ai1" if leader == "ai3" else "ai3"
                break  # end round

            parsed = parse_any_card_type(play)

            if prev is not None and prev.key_point and not can_beat(parsed, prev):
                winner = "ai1" if current == "ai3" else "ai3"
                if winner == "ai3":
                    ai3_chips += table_score
                else:
                    ai1_chips += table_score
                leader = winner
                break

            # GUI523: instant round win
            if parsed.type == CardType.GUI523:
                play_score = sum(c.score for c in play)
                if current == "ai3":
                    ai3_chips += table_score + play_score
                else:
                    ai1_chips += table_score + play_score
                leader = current
                break

            table_score += sum(c.score for c in play)

            # Remove cards from hand
            play_ids = {(c.id, c.point) for c in play}
            hand_copy = hand[:]
            hand.clear()
            for c in hand_copy:
                if (c.id, c.point) not in play_ids:
                    hand.append(c)
                else:
                    play_ids.discard((c.id, c.point))

            # Update trackers
            for c in play:
                trk = tracker3 if current == "ai3" else tracker1
                trk[c.point] = trk.get(c.point, 0) + 1

            # Hand empty + deck empty = game over
            if not hand and not deck3 and not deck1:
                if current == "ai3":
                    ai3_chips += table_score
                else:
                    ai1_chips += table_score
                return ai3_chips - ai1_chips

            if not hand:
                deck = deck3 if current == "ai3" else deck1
                if not deck:
                    if current == "ai3":
                        ai3_chips += table_score
                    else:
                        ai1_chips += table_score
                    return ai3_chips - ai1_chips

            current = "ai1" if current == "ai3" else "ai3"
            prev = parsed

        # Round ended — refill both hands to 5
        _draw_up_to(hand3, deck3, 5)
        _draw_up_to(hand1, deck1, 5)

        if not hand3 and not deck3:
            return ai3_chips - ai1_chips
        if not hand1 and not deck1:
            return ai3_chips - ai1_chips

    return ai3_chips - ai1_chips


def simulate_batch(
    weights: List[float], num_games: int = 40
) -> float:  # average net chips per game
    """Simulate num_games games. Returns average net chips (AI3 - AI1)."""
    total = 0.0
    for i in range(num_games):
        ai3_first = i % 2 == 0
        total += simulate_one_game(weights, ai3_first)
    return total / max(num_games, 1)


# =============================================================================
# Genetic Algorithm
# =============================================================================

POP_SIZE = 12
ELITE_COUNT = 2
TOURNAMENT_SIZE = 3
CROSSOVER_PROB = 0.7
EVAL_GAMES = 40
MAX_GENERATIONS = 50
CHECKPOINT_GEN = 10
N_PARAMS = len(PARAMETER_NAMES)


def perturb_copy(src: List[float]) -> List[float]:
    """Create a mutated copy of weights for initial population."""
    w = src[:]
    n_mutate = random.randint(8, N_PARAMS)
    indices = random.sample(range(N_PARAMS), n_mutate)
    for i in indices:
        if abs(w[i]) < 1e-9:
            w[i] = (random.random() - 0.5) * 2.0
        else:
            w[i] *= 0.75 + random.random() * 0.5
    return w


def tournament_select(population: List[List[float]], fitness: List[float]) -> List[float]:
    best_idx = -1
    best_fit = float("-inf")
    for _ in range(TOURNAMENT_SIZE):
        idx = random.randrange(len(population))
        if fitness[idx] > best_fit:
            best_fit = fitness[idx]
            best_idx = idx
    return population[best_idx]


def crossover(a: List[float], b: List[float]) -> List[float]:
    child = [0.0] * N_PARAMS
    for i in range(N_PARAMS):
        alpha = random.random()
        child[i] = alpha * a[i] + (1.0 - alpha) * b[i]
    return child


def mutate(w: List[float]) -> None:
    n = random.randint(1, 3)
    indices = random.sample(range(N_PARAMS), n)
    for i in indices:
        factor = random.uniform(0.8, 1.2)
        if abs(w[i]) < 1e-9:
            w[i] = (random.random() - 0.5) * 2.0
        else:
            w[i] *= factor


def diversity(population: List[List[float]]) -> float:
    """Normalized average pairwise Euclidean distance."""
    if len(population) <= 1:
        return 0.0
    total = 0.0
    pairs = 0
    for i in range(len(population)):
        for j in range(i + 1, len(population)):
            dist = math.sqrt(sum((population[i][k] - population[j][k]) ** 2 for k in range(N_PARAMS)))
            total += dist
            pairs += 1
    return total / pairs


def run_training():
    random.seed()
    start_time = time.time()

    print("=" * 60)
    print("  AI Trainer GA — Train AI2 weights vs AI1 (rule-based boss)")
    print(f"  Pop {POP_SIZE}  |  {EVAL_GAMES} games/ind  |  max {MAX_GENERATIONS} gen")
    print(f"  Tournament(k={TOURNAMENT_SIZE})  |  Arithmetic crossover  |  Elite {ELITE_COUNT}")
    print(f"  Parameters: {N_PARAMS}  |  Fitness: avg net chips (AI3-AI1)")
    print("=" * 60)

    # Init population
    base = default_weights()
    population = [perturb_copy(base) for _ in range(POP_SIZE)]

    # Evaluate initial population
    fitness = [0.0] * POP_SIZE
    champion: List[float] = []
    champion_fitness = float("-inf")
    champion_gen = 0

    def evaluate():
        nonlocal champion, champion_fitness, champion_gen
        for i in range(POP_SIZE):
            net = simulate_batch(population[i], EVAL_GAMES)
            fitness[i] = net
            if net > champion_fitness:
                champion = population[i][:]
                champion_fitness = net
                champion_gen = generation

    generation = 0
    evaluate()
    avg_fit = sum(fitness) / POP_SIZE
    div = diversity(population)
    print(f"  gen={generation}  champion={champion_fitness:+.1f}  avg={avg_fit:+.1f}  div={div:.1f}")

    for generation in range(1, MAX_GENERATIONS + 1):
        # Selection and breeding
        ranked = sorted(range(POP_SIZE), key=lambda i: fitness[i], reverse=True)
        next_gen = [population[ranked[i]][:] for i in range(ELITE_COUNT)]

        while len(next_gen) < POP_SIZE:
            pa = tournament_select(population, fitness)
            if random.random() < CROSSOVER_PROB and POP_SIZE >= 2:
                pb = tournament_select(population, fitness)
                child = crossover(pa, pb)
            else:
                child = pa[:]
            # Don't mutate elites
            is_elite = any(child == e for e in next_gen)
            if not is_elite:
                mutate(child)
            next_gen.append(child)

        population = next_gen
        evaluate()

        avg_fit = sum(fitness) / POP_SIZE
        div = diversity(population)
        print(f"  gen={generation}  champion={champion_fitness:+.1f}  avg={avg_fit:+.1f}  div={div:.1f}")

        if generation % CHECKPOINT_GEN == 0:
            save_weights(champion, champion_fitness, generation)

        if generation - champion_gen >= 15 and div < 0.01:
            print(f"  Early stop: converged ({generation - champion_gen} gens no progress, div={div:.4f})")
            break

    elapsed = time.time() - start_time
    total_games = POP_SIZE * EVAL_GAMES * generation  # approximate
    print()
    print("=" * 60)
    print(f"  GA Complete  {generation} gen  |  ~{total_games} games  |  {elapsed:.1f}s")
    print(f"  Champion net chips: {champion_fitness:+.1f}  (gen {champion_gen})")
    save_weights(champion, champion_fitness, generation)
    print("=" * 60)


def save_weights(w: List[float], fitness: float, gen: int):
    path = "ai2_weights.json"
    data = {
        "generation": gen,
        "fitness_net_chips": fitness,
        "parameters": weights_to_dict(w),
    }
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
    print(f"  Saved: {path}")


if __name__ == "__main__":
    run_training()