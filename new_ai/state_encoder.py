"""State encoder: converts env_server observation to fixed-size float vector."""

from action_space import ACTION_SPACE_SIZE

# ── card ordering for 15-d vectors ──
POINT_ORDER = ["4", "6", "8", "9", "10", "J", "Q", "K", "A", "3", "2", "5",
               "小鬼", "大鬼", "7"]
POINT_TO_IDX = {p: i for i, p in enumerate(POINT_ORDER)}
N_POINTS = len(POINT_ORDER)  # 15

# ── card type names ──
CARD_TYPES = [
    "Invalid",          # 0
    "Single",           # 1
    "Pair",             # 2
    "Triple",           # 3
    "TripleWithOne",    # 4
    "TripleWithTwo",    # 5
    "Bomb",             # 6
    "Rocket",           # 7
    "Special523",       # 8
]
CARD_TYPE_TO_IDX = {t: i for i, t in enumerate(CARD_TYPES)}
N_CARD_TYPES = len(CARD_TYPES)  # 9

# ── normalization constants ──
MAX_TABLE_SCORE = 200.0
MAX_SCORE_DIFF  = 200.0
MAX_DECK        = 54.0
MAX_HAND_SIZE   = 20.0


def _point_vector(card_strs):
    """Convert list of card strings to 15-d count vector."""
    vec = [0] * N_POINTS
    for c in card_strs:
        p = _extract_point(c)
        idx = POINT_TO_IDX.get(p, -1)
        if idx >= 0:
            vec[idx] += 1
    return vec


def _extract_point(card_str):
    """Extract point from card string like '红桃A' → 'A', '大鬼' → '大鬼'."""
    if card_str in ("大鬼", "小鬼"):
        return card_str
    for suit in ("黑桃", "红桃", "梅花", "方块"):
        if card_str.startswith(suit):
            return card_str[len(suit):]
    return card_str


def encode_state(state_dict, action_history=None):
    """
    state_dict : env_server observation dict
    action_history : list of (player_cards, opp_cards_played) tuples,
                     or None for empty history.
    Returns list[float] of length 103.
    """
    if action_history is None:
        action_history = []

    # 1. my hand (15)
    my_hand_vec = _point_vector(state_dict.get("my_hand", []))

    # 2. table cards (15)
    table_vec = _point_vector(state_dict.get("table_cards", []))

    # 3. played cards (15) — cumulative from action_history
    played_vec = [0] * N_POINTS
    for my_play, opp_play in action_history:
        for c in my_play:
            idx = POINT_TO_IDX.get(_extract_point(c), -1)
            if idx >= 0:
                played_vec[idx] += 1
        if opp_play:
            for c in opp_play:
                idx = POINT_TO_IDX.get(_extract_point(c), -1)
                if idx >= 0:
                    played_vec[idx] += 1

    # 4. opponent expected count per point (15)
    #     unknown[p] = TOTAL[p] - played[p] - my_hand[p]
    #     opp_est[p] = unknown[p] * opp_hand_size / (opp_hand_size + deck_size)
    TOTAL = [4,4,4,4,4,4,4,4,4,4,4,4, 1,1, 4]
    opp_hand_size = float(state_dict.get("opp_hand_size", 0))
    deck_size = float(state_dict.get("deck_remaining", 0))
    denom = opp_hand_size + deck_size
    opp_est = [0.0] * N_POINTS
    if denom > 0:
        for i in range(N_POINTS):
            unknown = TOTAL[i] - played_vec[i] - my_hand_vec[i]
            if unknown < 0:
                unknown = 0
            opp_est[i] = unknown * opp_hand_size / denom

    # 5. deck expected count per point (15)
    #     deck_est[p] = unknown[p] - opp_est[p]
    deck_est = [0.0] * N_POINTS
    if denom > 0:
        for i in range(N_POINTS):
            unknown = TOTAL[i] - played_vec[i] - my_hand_vec[i]
            if unknown < 0:
                unknown = 0
            deck_est[i] = unknown * deck_size / denom

    # 6. last play type (8-dim one-hot)
    last_play = state_dict.get("last_play")
    type_vec = [0.0] * N_CARD_TYPES
    if last_play is not None and isinstance(last_play, dict):
        t = last_play.get("type", "Invalid")
        type_vec[CARD_TYPE_TO_IDX.get(t, 0)] = 1.0
    type_vec = type_vec[1:]  # now 8 dims

    # 7. last play key point (15-dim one-hot)
    key_vec = [0.0] * N_POINTS
    if last_play is not None and isinstance(last_play, dict):
        kp = last_play.get("key", "")
        if kp in POINT_TO_IDX:
            key_vec[POINT_TO_IDX[kp]] = 1.0

    # 8. table score (1)
    table_score = float(state_dict.get("table_score", 0)) / MAX_TABLE_SCORE

    # 9. score diff (1)
    my_score = float(state_dict.get("my_score", 0))
    opp_score = float(state_dict.get("opp_score", 0))
    diff = (my_score - opp_score) / MAX_SCORE_DIFF

    # 10. deck remaining (1)
    deck = float(state_dict.get("deck_remaining", 0)) / MAX_DECK

    # 11. opponent hand size (1)
    opp_hand = float(state_dict.get("opp_hand_size", 0)) / MAX_HAND_SIZE

    # 12. my hand size (1)
    my_hand_size = float(len(state_dict.get("my_hand", []))) / MAX_HAND_SIZE

    vec = []
    vec.extend(my_hand_vec)      # 15
    vec.extend(table_vec)         # 15
    vec.extend(played_vec)        # 15
    vec.extend(opp_est)           # 15  (new)
    vec.extend(deck_est)          # 15  (new)
    vec.extend(type_vec)          # 8
    vec.extend(key_vec)           # 15
    vec.append(table_score)       # 1
    vec.append(diff)              # 1
    vec.append(deck)              # 1
    vec.append(opp_hand)          # 1
    vec.append(my_hand_size)      # 1
    # total = 15*5 + 8 + 15 + 5 = 103

    return vec


STATE_DIM = 103


# ── self-test ──
if __name__ == "__main__":
    sample_state = {
        "my_hand": ["红桃A", "方块3", "黑桃K", "黑桃5", "梅花7"],
        "opp_hand_size": 4,
        "table_cards": ["红桃Q"],
        "table_score": 0,
        "my_score": 25,
        "opp_score": 10,
        "deck_remaining": 40,
        "last_play": {"type": "Single", "key": "Q", "cards": ["红桃Q"]},
    }

    vec = encode_state(sample_state)
    print(f"state dim = {len(vec)}  (expected 73)")
    print(f"action space size = {ACTION_SPACE_SIZE}")

    print(f"\nmy_hand_vec     (15): {vec[0:15]}")
    print(f"table_vec       (15): {vec[15:30]}")
    print(f"played_vec      (15): {vec[30:45]}")
    print(f"last_type_vec    (8): {vec[45:53]}")
    print(f"last_key_vec    (15): {vec[53:68]}")
    print(f"table_score      (1): {vec[68]:.3f}")
    print(f"score_diff       (1): {vec[69]:.3f}")
    print(f"deck_remaining   (1): {vec[70]:.3f}")
    print(f"opp_hand         (1): {vec[71]:.3f}")
    print(f"my_hand_size     (1): {vec[72]:.3f}")