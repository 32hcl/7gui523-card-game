"""Protocol tests: legality, reproducible paired seats, WDL labels, fair routes."""
import json, subprocess, sys, random
exe=sys.argv[1]
def session(level):
 return subprocess.Popen([exe,level,'55331'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
def send(p,command,**kwargs):
 p.stdin.write(json.dumps(dict(command=command,**kwargs),ensure_ascii=False)+'\n');p.stdin.flush();d=json.loads(p.stdout.readline());assert d['status']=='ok',d;return d
def finish(p):send(p,'quit');assert p.wait(timeout=5)==0
p=session('AI2');q=session('AI2');rng=random.Random(777)
for game in range(10):
 a=send(p,'reset');b=send(q,'reset');assert a==b
 for step in range(250):
  if a['done']:break
  o=a['observation'];assert sum(o['played_counts'])+len(o['my_hand'])+o['opp_hand_size']+o['deck_remaining']==54
  choices=send(p,'legal_actions');assert choices==send(q,'legal_actions')
  action=rng.choice(choices['actions']);a=send(p,'step',action=action);b=send(q,'step',action=action);assert a==b
 else:raise AssertionError('nonterminating game')
 assert a['reward'] in (-1,0,1)
finish(p);finish(q)
for level in ('AI2','Fair1','Fair2'):
 p=session('AI3');d=send(p,'auto_play',agent_ai=level,opp_ai='AI3');assert d['target_version']=='terminal_wdl_v2';labels={r['cumulative_reward'] for r in d['trajectory']};assert len(labels)==1 and labels.issubset({-1,0,1});finish(p)
print('Protocol determinism, card conservation, fair routes and WDL labels passed')
