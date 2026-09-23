"""Checkpointed CPU-only experiments. Every subprocess has a <2h timeout.
Screening/validation/holdout seeds are disjoint. Resume uses binary+config hash.
"""
import argparse, hashlib, json, random, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
EXE=OUT/'search_benchmark.exe'
def save(path,data):
 tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8');tmp.replace(path)
def run(tag,cfg,opponent='AI2',seed=61000,candidate='Fair2'):
 path=OUT/(tag+'.json');digest=hashlib.sha256(EXE.read_bytes()).hexdigest()
 spec={'binary_sha256':digest,'config':cfg,'opponent':opponent,'seed':seed,'candidate':candidate,'games':200}
 if path.exists():
  d=json.loads(path.read_text(encoding='utf-8'))
  if d.get('spec')==spec:return d
 cmd=[str(EXE),'--candidate',candidate,'--opponent',opponent,'--games','200','--seed',str(seed),'--output',str(path)]
 for k,v in cfg.items():cmd.extend(['--'+k,str(v)])
 with path.with_suffix('.log').open('w',encoding='utf-8') as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=log,timeout=7100,check=True)
 d=json.loads(path.read_text());d['spec']=spec;save(path,d)
 print(tag, 'wins',d['wins'],'draws',d['draws'],'p99',round(d['p99_ms'],2),'seconds',round(d['elapsed_seconds'],2),flush=True)
 return d
def objective(d):return d['score_rate']
def main():
 started=time.time();rng=random.Random(20260919)
 default={'particles':48,'simulations':48,'horizon':12,'budget-ms':120,'exploration':1.2,'tolerance':.08,'rollout-model':1}
 for opp in ['AI2','AI3']:
  run('stage2-final-Fair2-'+opp,default,opp,51000)
  run('stage2-final-World-'+opp,default,opp,51000,'World')
 space={'particles':[24,48,96],'simulations':[24,48,96],'horizon':[12,24,48],'budget-ms':[60,120,240],'exploration':[.5,1.2,2.0],'tolerance':[.05,.15,.35],'rollout-model':[0,1,2,3]}
 configs=[default]
 while len(configs)<20:
  c={k:rng.choice(v) for k,v in space.items()}
  mix=rng.choice([[.6,.15,.1,.1,.05],[.15,.6,.1,.1,.05],[.2,.2,.2,.2,.2]])
  c.update({'prior'+str(i):v for i,v in enumerate(mix)})
  if c not in configs:configs.append(c)
 save(OUT/'search_plan.json',{'space':space,'starts':configs,'screen_seed':61000,'validation_seed':71000,'holdout_seeds':[81000,91000,101000],'games_each':200,'selection':'screen score rate vs AI2; top5 validation average vs AI2/AI3; no holdout tuning','local':'top3 one-coordinate neighbors (simulations,horizon,rollout-model), retain elites','limits':'7100 seconds per subprocess, CPU only'})
 rows=[]
 for i,c in enumerate(configs):
  d=run('screen-%02d'%i,c);rows.append({'id':'screen-%02d'%i,'config':c,'score_rate':objective(d),'win_rate':d['win_rate'],'ci95':d['ci95_wilson_marginal']})
  save(OUT/'multistart_results.json',rows);save(OUT/'best_screen.json',max(rows,key=lambda x:x['score_rate']))
 elites=sorted(rows,key=lambda x:x['score_rate'],reverse=True)[:5]
 local=[]
 for i,e in enumerate(elites[:3]):
  best=e
  for j,key in enumerate(['simulations','horizon','rollout-model']):
   c=best['config'].copy();choices=space[key];c[key]=choices[(choices.index(c[key])+1)%len(choices)]
   tag='local-%d-%d'%(i,j);d=run(tag,c);r={'id':tag,'config':c,'score_rate':objective(d),'win_rate':d['win_rate'],'ci95':d['ci95_wilson_marginal']};local.append(r)
   if r['score_rate']>best['score_rate']:best=r
   save(OUT/'local_results.json',local);save(OUT/'best_screen.json',max(rows+local,key=lambda x:x['score_rate']))
  save(OUT/('local_optimum_%d.json'%i),best)
 candidates=sorted(rows+local,key=lambda x:x['score_rate'],reverse=True);unique=[]
 for row in candidates:
  if row['config'] not in [r['config'] for r in unique]:unique.append(row)
  if len(unique)==5:break
 validated=[]
 for i,e in enumerate(unique):
  ds=[run('validation-%d-%s'%(i,opp),e['config'],opp,71000) for opp in ['AI2','AI3']]
  validated.append({**e,'validation_score':sum(objective(d) for d in ds)/2,'validation_win_rates':[d['win_rate'] for d in ds]});save(OUT/'validation_results.json',validated)
 best=max(validated,key=lambda x:x['validation_score']);save(OUT/'best_config.json',{'claim':'Strongest configuration found within this search budget, rule version and opponent set','config':best['config'],'source':best['id'],'selection_score':best['validation_score'],'holdout_used_for_selection':False})
 # Final holdout is consumed once, after selecting parameters.
 for candidate in ['Fair1','Fair2']:
  for opponent in ['AI2','AI3','AI4']:
   for seed in [81000,91000,101000]:run('holdout-%s-%s-%d'%(candidate,opponent,seed),best['config'],opponent,seed,candidate)
 save(OUT/'run_complete.json',{'elapsed_seconds':time.time()-started,'completed':True,'neural_model':'skipped','convergence':'Fixed 20-start+9-neighbor budget exhausted; no claim of global convergence'})
if __name__=='__main__':main()
