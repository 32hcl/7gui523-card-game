import subprocess
import json


class SevenKingEnv:
    def __init__(self, exe_path, opponent_level="AI1_Simple"):
        level_map = {
            "AI1_Simple": "AI1",
            "AI2_Rule": "AI2",
            "AI3_Tracker": "AI3",
            "Random": "Random",
        }
        mapped = level_map.get(opponent_level, "AI1")
        self.proc = subprocess.Popen(
            [exe_path, mapped],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            encoding='utf-8',
            bufsize=1,
        )

    def _send(self, cmd):
        line = json.dumps(cmd, ensure_ascii=False) + "\n"
        self.proc.stdin.write(line)
        self.proc.stdin.flush()
        resp = self.proc.stdout.readline()
        if not resp:
            raise RuntimeError("env_server 已退出")
        return json.loads(resp)

    def reset(self):
        return self._send({"command": "reset"})

    def legal_actions(self):
        resp = self._send({"command": "legal_actions"})
        return resp["actions"]

    def step(self, action):
        return self._send({"command": "step", "action": action})

    def state_vec(self):
        resp = self._send({"command": "state_vec"})
        return resp["state_vec"]

    def peek(self):
        resp = self._send({"command": "peek"})
        return resp["actions"], resp["next_states"]

    def auto_play(self, opp_ai="AI2", agent_ai="AI2"):
        resp = self._send({
            "command": "auto_play",
            "opp_ai": opp_ai,
            "agent_ai": agent_ai,
        })
        return resp["trajectory"]

    def close(self):
        try:
            self._send({"command": "quit"})
        except Exception:
            pass
        self.proc.wait()