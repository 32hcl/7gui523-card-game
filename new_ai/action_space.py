"""Abstract action space — suit-agnostic, ~408 actions."""

POINTS_SINGLE = ["4", "6", "8", "9", "10", "J", "Q", "K", "A", "3", "2", "5",
                 "小鬼", "大鬼", "7"]
POINTS_PAIR  = ["4", "6", "8", "9", "10", "J", "Q", "K", "A", "3", "2", "5", "7"]

_action_list = []

def _add(key):
    _action_list.append(key)

# pass
_add("pass")

# single: 15
for p in POINTS_SINGLE:
    _add(f"single:{p}")

# pair: 13
for p in POINTS_PAIR:
    _add(f"pair:{p}")

# triple: 13
for p in POINTS_PAIR:
    _add(f"triple:{p}")

# bomb: 13
for p in POINTS_PAIR:
    _add(f"bomb:{p}")

# rocket: 1
_add("rocket")

# triple+1: 13 triple × 14 kicker  (kicker from 15 singles minus triple)
for tp in POINTS_PAIR:
    for kp in POINTS_SINGLE:
        if kp != tp:
            _add(f"triple+{tp}:{kp}")

# triple+2: 13 triple × 12 pair  (pair from 13 minus triple)
for tp in POINTS_PAIR:
    for pp in POINTS_PAIR:
        if pp != tp:
            _add(f"triple+pair:{tp}:{pp}")

# special523: 1
_add("special523")

ACTION_SPACE_SIZE = len(_action_list)
action_to_index_map = {k: i for i, k in enumerate(_action_list)}
index_to_action_map = {i: k for i, k in enumerate(_action_list)}


def action_list():
    return _action_list


def action_to_index(key):
    return action_to_index_map[key]


def index_to_action(idx):
    return index_to_action_map[idx]


def legal_indices(legal_concrete_actions, hand):
    """
    legal_concrete_actions: list of card-string lists from env.legal_actions()
    hand: current hand card strings
    Returns set of abstract action indices that are legal.
    """
    from abstract_to_concrete import concrete_to_abstract
    indices = set()
    for act in legal_concrete_actions:
        abs_key = concrete_to_abstract(act, hand)
        if abs_key in action_to_index_map:
            indices.add(action_to_index_map[abs_key])
    # pass is always present in legal_concrete_actions as []
    return indices


# ── self-test ──
if __name__ == "__main__":
    print(f"动作空间总大小: {ACTION_SPACE_SIZE}")
    print(f"Pass      : index {action_to_index('pass')}")
    print(f"single:7  : index {action_to_index('single:7')}")
    print(f"pair:5    : index {action_to_index('pair:5')}")
    print(f"rocket    : index {action_to_index('rocket')}")
    print(f"special523: index {action_to_index('special523')}")
    print()

    print("前 10 个 action:")
    for i in range(10):
        print(f"  {i}: {index_to_action_map[i]}")
    print("...")
    print("后 5 个 action:")
    for i in range(ACTION_SPACE_SIZE - 5, ACTION_SPACE_SIZE):
        print(f"  {i}: {index_to_action_map[i]}")