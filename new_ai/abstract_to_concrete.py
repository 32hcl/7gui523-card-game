"""Convert abstract action keys to/from concrete card lists based on current hand."""


def _point_of(card_str):
    """Extract point from card string: '红桃A' → 'A', '大鬼' → '大鬼'."""
    if card_str in ("大鬼", "小鬼"):
        return card_str
    for s in ("黑桃", "红桃", "梅花", "方块"):
        if card_str.startswith(s):
            return card_str[len(s):]
    return card_str


def _cards_of_point(point, hand):
    """Return all cards in hand matching point, in suit order."""
    suit_order = {"黑桃": 0, "红桃": 1, "梅花": 2, "方块": 3, "": 4}
    matches = [c for c in hand if _point_of(c) == point]
    matches.sort(key=lambda c: suit_order.get(c[:2], 4))
    return matches


def abstract_to_concrete(abstract_key, my_hand):
    """
    Convert abstract action key to a list of concrete card strings.
    Returns list[str] or raises ValueError.
    """
    if not isinstance(my_hand, list):
        raise ValueError("my_hand must be a list of card strings")

    if abstract_key == "pass":
        return []

    # ── single ──
    if abstract_key.startswith("single:"):
        p = abstract_key[len("single:"):]
        cards = _cards_of_point(p, my_hand)
        if not cards:
            raise ValueError(f"没有 {p} 在手里: {my_hand}")
        return [cards[0]]

    # ── pair ──
    if abstract_key.startswith("pair:"):
        p = abstract_key[len("pair:"):]
        cards = _cards_of_point(p, my_hand)
        if len(cards) < 2:
            raise ValueError(f"{p} 不足两张: {cards}")
        return cards[:2]

    # ── triple ──
    if abstract_key.startswith("triple+pair:"):
        rest = abstract_key[len("triple+pair:"):]  # "X:Y"
        tp, pp = rest.split(":")
        # triple part
        t_cards = _cards_of_point(tp, my_hand)
        if len(t_cards) < 3:
            raise ValueError(f"{tp} 不足三张: {t_cards}")
        # pair kicker
        p_cards = _cards_of_point(pp, my_hand)
        if len(p_cards) < 2:
            raise ValueError(f"{pp} 不足两张: {p_cards}")
        return t_cards[:3] + p_cards[:2]

    if abstract_key.startswith("triple+"):
        rest = abstract_key[len("triple+"):]  # "X:Y"
        tp, kp = rest.split(":")
        t_cards = _cards_of_point(tp, my_hand)
        if len(t_cards) < 3:
            raise ValueError(f"{tp} 不足三张: {t_cards}")
        k_cards = _cards_of_point(kp, my_hand)
        # kicker cannot overlap with triple cards
        used = set(t_cards[:3])
        k_cards = [c for c in k_cards if c not in used]
        if not k_cards:
            raise ValueError(f"{kp} 不在手里或已被三张占用: {my_hand}")
        return t_cards[:3] + [k_cards[0]]

    if abstract_key.startswith("triple:"):
        p = abstract_key[len("triple:"):]
        cards = _cards_of_point(p, my_hand)
        if len(cards) < 3:
            raise ValueError(f"{p} 不足三张: {cards}")
        return cards[:3]

    # ── bomb ──
    if abstract_key.startswith("bomb:"):
        p = abstract_key[len("bomb:"):]
        cards = _cards_of_point(p, my_hand)
        if len(cards) < 4:
            raise ValueError(f"{p} 不足四张: {cards}")
        return cards[:4]

    # ── rocket ──
    if abstract_key == "rocket":
        big = _cards_of_point("大鬼", my_hand)
        small = _cards_of_point("小鬼", my_hand)
        if not big or not small:
            raise ValueError(f"火箭需要大小鬼都在手: {my_hand}")
        return [small[0], big[0]]

    # ── special523 ──
    if abstract_key == "special523":
        joker = _cards_of_point("大鬼", my_hand)
        if not joker:
            joker = _cards_of_point("小鬼", my_hand)
        c7 = _cards_of_point("7", my_hand)
        c5 = _cards_of_point("5", my_hand)
        c2 = _cards_of_point("2", my_hand)
        c3 = _cards_of_point("3", my_hand)
        missing = []
        if not joker:
            missing.append("鬼")
        if not c7:
            missing.append("7")
        if not c5:
            missing.append("5")
        if not c2:
            missing.append("2")
        if not c3:
            missing.append("3")
        if missing:
            raise ValueError(f"Special523 缺牌: {missing}  手牌: {my_hand}")
        return [c7[0], joker[0], c5[0], c2[0], c3[0]]

    raise ValueError(f"未知抽象动作: {abstract_key}")


def concrete_to_abstract(cards, my_hand=None):
    """
    Convert a concrete card list (from env.legal_actions()) back to an
    abstract action key. Returns string key.
    The my_hand parameter is unused but kept for backward compatibility.
    """
    if not cards:
        return "pass"

    points = [_point_of(c) for c in cards]
    n = len(cards)

    if n == 1:
        return f"single:{points[0]}"

    if n == 2:
        if set(points) == {"大鬼", "小鬼"}:
            return "rocket"
        if points[0] == points[1]:
            return f"pair:{points[0]}"
        # fallback — shouldn't happen for legal plays
        return f"single:{points[0]}"

    if n == 3:
        # Special523? No, that's 5 cards.
        if points[0] == points[1] == points[2]:
            return f"triple:{points[0]}"
        # count frequencies
        from collections import Counter
        cnt = Counter(points)
        if 3 in cnt.values():
            triple_pt = [p for p, c in cnt.items() if c == 3][0]
            kicker_pt = [p for p, c in cnt.items() if c == 1][0]
            return f"triple+{triple_pt}:{kicker_pt}"
        return f"single:{points[0]}"

    if n == 4:
        from collections import Counter
        cnt = Counter(points)
        if 4 in cnt.values():
            return f"bomb:{points[0]}"
        if 3 in cnt.values():
            triple_pt = [p for p, c in cnt.items() if c == 3][0]
            kicker_pt = [p for p, c in cnt.items() if c == 1][0]
            return f"triple+{triple_pt}:{kicker_pt}"
        return f"single:{points[0]}"

    if n == 5:
        from collections import Counter
        cnt = Counter(points)
        # check Special523
        has_joker = ("大鬼" in points or "小鬼" in points)
        has_7 = ("7" in points)
        has_5 = ("5" in points)
        has_2 = ("2" in points)
        has_3 = ("3" in points)
        if has_joker and has_7 and has_5 and has_2 and has_3:
            return "special523"
        if 3 in cnt.values() and 2 in cnt.values():
            triple_pt = [p for p, c in cnt.items() if c == 3][0]
            pair_pt = [p for p, c in cnt.items() if c == 2][0]
            return f"triple+pair:{triple_pt}:{pair_pt}"
        return f"single:{points[0]}"

    return f"single:{points[0]}"


# ── self-test ──
if __name__ == "__main__":
    from action_space import ACTION_SPACE_SIZE, index_to_action_map

    hand = ["黑桃5", "红桃5", "梅花5", "方块7",
            "大鬼", "小鬼", "红桃A", "方块A"]

    print(f"手牌: {hand}")
    print(f"动作空间: {ACTION_SPACE_SIZE}")
    print()

    tests = [
        "pass",
        "single:5",
        "pair:5",
        "pair:A",
        "triple:5",
        "bomb:7",
        "rocket",
        "special523",
        "triple+5:A",
        "triple+5:小鬼",
        "triple+pair:5:A",
    ]

    for key in tests:
        try:
            concrete = abstract_to_concrete(key, hand)
            print(f"  {key:25s} → {concrete}")
        except ValueError as e:
            print(f"  {key:25s} → ERROR: {e}")

    print()
    print("反向测试 (concrete → abstract):")
    rev_tests = [
        [],
        ["黑桃5"],
        ["黑桃5", "红桃5"],
        ["黑桃5", "红桃5", "梅花5"],
        ["黑桃5", "红桃5", "梅花5", "方块7", "大鬼"],
        ["小鬼", "大鬼"],
    ]
    for cards in rev_tests:
        ab = concrete_to_abstract(cards, hand)
        print(f"  {str(cards):40s} → {ab}")