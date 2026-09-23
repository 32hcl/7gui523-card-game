"""Summaries use deal-pair clustered bootstrap, not independent-seat errors."""
import json, random, statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parent

def load(name):return json.loads((ROOT/name).read_text(encoding='utf-8'))
def interval(values,seed=9931):
 r=random.Random(seed);n=len(values);means=sorted(sum(r.choices(values,k=n))/n for _ in range(10000));return [means[249],means[9749]]
def aggregate(paths):
 ds=[load(p) for p in paths];details=[x for d in ds for x in d['details']]
 pairs={}
 for x in details:pairs.setdefault(x['deal_seed'],[]).append(float(x['winner']==1))
 assert all(len(x)==2 for x in pairs.values())
 wins=sum(d['wins'] for d in ds);games=len(details)
 return {'games':games,'wins':wins,'losses':sum(d['losses'] for d in ds),'draws':sum(d['draws'] for d in ds),'win_rate':wins/games,'ci95_deal_pair_bootstrap':interval([sum(v)/2 for v in pairs.values()]),'score_rate':sum(d['wins']+d['draws']*.5 for d in ds)/games,'p50_ms_by_seed':[d['p50_ms'] for d in ds],'p95_ms_by_seed':[d['p95_ms'] for d in ds],'p99_ms_by_seed':[d['p99_ms'] for d in ds],'max_ms':max(d['max_ms'] for d in ds),'belief_resets':sum(d.get('belief_resets',0) for d in ds)}
def save(name,data):(ROOT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def main():
 rows={}
 for candidate in ['Fair1','Fair2']:
  for opponent in ['AI2','AI3','AI4']:
   rows[candidate+' vs '+opponent]=aggregate(['holdout-%s-%s-%d.json'%(candidate,opponent,s) for s in [81000,91000,101000]])
 base=aggregate(['stage0-%d.json'%s for s in [11000,21000,31000]])
 differences={}
 for opponent in ['AI2','AI3','AI4']:
  pairs=[]
  for seed in [81000,91000,101000]:
   a=load('holdout-Fair1-%s-%d.json'%(opponent,seed))['details'];b=load('holdout-Fair2-%s-%d.json'%(opponent,seed))['details']
   for i in range(0,200,2):pairs.append(sum(int(b[j]['winner']==1)-int(a[j]['winner']==1) for j in [i,i+1])/2)
  differences[opponent]={'win_rate_difference':statistics.mean(pairs),'ci95_paired_difference':interval(pairs)}
 result={'stage0':base,'holdout':rows,'Fair2_minus_Fair1':differences,'best':load('best_config.json'),'method':'10000 deal-pair bootstrap replicates; wins/games, draws reported separately; same shuffled deals exchanged seats; CI not corrected for all multiple comparisons; final seeds never used for tuning'}
 save('FINAL_RESULTS.json',result)
 text=['# 公平 AI 研究结果','','本报告仅称“此预算、此规则、此对手集合上已找到的最强配置”。','', '| 配置 | 胜/负/平 | 胜率 | 95%区间（牌局对重采样） | 最慢单步ms |','|---|---|---|---|---|']
 for key,d in rows.items():
  lo,hi=d['ci95_deal_pair_bootstrap'];text.append('| %s | %d/%d/%d | %.2f%% | %.2f–%.2f%% | %.2f |'%(key,d['wins'],d['losses'],d['draws'],100*d['win_rate'],100*lo,100*hi,d['max_ms']))
 text+=['','阶段0：六个搜索 bug 分别提交；共用 v2 转移；修正规则下作弊 AI4 vs AI3 588/600胜。','阶段1：公开记牌重建残局，Alpha-Beta + 置换表 + 900ms上限。初测17/200，与AI3对照相同。','阶段2：公开观测接口、粒子滤波、信息集树和完整世界采样对照。默认Fair2对AI2 74/200，对AI3 148/200。','阶段3：跳过小网络；默认搜索尚未胜过AI2，未进行神经模型收益实验。','阶段4：20起点、精英、9次坐标邻域、5组独立验证，再冻结配置测三组留出。未证明全局收敛。','','## 最佳配置','```json',json.dumps(result['best'],ensure_ascii=False,indent=2),'```','','## 结论边界','留出数据才用于最终强度结论；200局筛选存在选择偏差。600局的2个百分点差异不能直接视作显著。','完整世界对照使用minimax叶评估，信息集搜索使用建模rollout，两者差异不完全等于信息集共享的收益。','粒子全部失效后回到当前公开计数重采样，会丢失历史。对手模型近似，不构成贝叶斯精确推断。','残局求解只在完整公开记牌、无未记录暗弃牌时成立。超时返回已完成搜索的合法动作，不保证已证最优。','已验证无界面MSVC构建；本机无用户指定Qt/MinGW路径，未验证Qt GUI。UI和main.cpp未修改，界面仍保留旧流程和档位。','源码实际位置为当前工作目录；用户给出的主目录/副本目录在本机不存在，未声称已合并到这些目录。','','## 保留与暂缓','保留AI1–AI3、旧作弊AI4、公共状态转移、Fair1/2、协议与实验工具。','旧PPO/价值网络及贝叶斯采样保留文件供复现，不接入新公平AI；建议停用旧标签模型，未自动删除用户文件。','peek是特权调试接口，公平策略禁止调用。JSON卡名沿用UTF-8原文格式。','','完整逐局数据、各种子p50/p95/p99、粒子重置次数见FINAL_RESULTS.json及holdout JSON。']
 (ROOT/'FINAL_REPORT.md').write_text('\n'.join(text)+'\n',encoding='utf-8')
 print(json.dumps(rows,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
