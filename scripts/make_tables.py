#!/usr/bin/env python3
"""Create exactly three interactive tables from benchmark CSVs; Python stdlib only."""
import argparse
import csv
import html
import json
import math
import re
from pathlib import Path

TEMPLATE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Closest pair benchmark tables</title>
<style>
:root{color-scheme:light dark;--bg:#fff;--fg:#18202b;--muted:#536174;--line:#dce1e7;--soft:#f4f6f8;--accent:#2459a6}
@media(prefers-color-scheme:dark){:root{--bg:#15191e;--fg:#edf1f5;--muted:#b2bdca;--line:#39414c;--soft:#20262e;--accent:#9fbeee}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,sans-serif}main{max-width:1100px;margin:auto;padding:32px 28px}h1{font-size:24px;margin:0 0 6px}h2{font-size:19px;margin:0 0 4px}p{margin:4px 0 16px;color:var(--muted)}.controls{display:flex;flex-wrap:wrap;gap:20px;align-items:end;margin:24px 0}label{display:grid;gap:6px;font-size:13px;font-weight:600}select,button{font:inherit;color:var(--fg);background:var(--bg);border:1px solid var(--line);padding:9px 12px;border-radius:5px}select{max-width:100%}button{margin-left:auto}section{margin:32px 0}.scroll{overflow-x:auto}table{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}th,td{border-bottom:1px solid var(--line);padding:12px 15px;text-align:right;vertical-align:top}thead th{background:var(--soft);font-weight:600}th:first-child{text-align:left;white-space:nowrap}caption{text-align:left;font-size:12px;color:var(--muted);padding:6px 0 10px}.values{display:grid;gap:5px;min-width:125px}.value{display:flex;justify-content:space-between;gap:16px}.method{font-size:12px;color:var(--muted)}strong{font-weight:600}.note{font-size:12px;color:var(--muted);overflow-wrap:anywhere}#summary{font-size:13px}footer{border-top:1px solid var(--line);padding-top:14px}.unit{font-weight:400;color:var(--muted)}@media(max-width:600px){main{padding:20px 14px}.controls{display:grid;gap:12px}button{margin-left:0}th,td{padding:10px}.values{min-width:115px}}@media print{.controls{display:none}main{max-width:none;padding:0}section{break-inside:avoid}body{font-size:11px}.method{font-size:10px}.values{min-width:0}th,td{padding:6px}}
</style></head><body><main>
<h1>Closest pair benchmark results</h1><p>Columns are point counts; rows are requested thread counts. Choose a metric baseline to distinguish thread scaling from comparison with sequential C++.</p>
<div class="controls"><label>Benchmark run<select id="run">__OPTIONS__</select></label><label>Implementation<select id="method"><option value="all">All parallel implementations</option><option value="openmp">OpenMP</option><option value="tbb">Intel oneTBB</option><option value="pstl">C++ Parallel STL</option><option value="sequential">Sequential baseline</option></select></label><label>Metric baseline<select id="basis"><option value="scaling" __SCALING_SELECTED__>Same implementation at 1 thread</option><option value="sequential" __SEQUENTIAL_SELECTED__>Sequential C++ reference</option></select></label><button id="print" type="button">Print / save PDF</button></div>
<p id="summary" aria-live="polite"></p><div id="tables">__TABLES__</div>
<footer><p id="definitions" class="note">__DEFINITIONS__</p><p id="source" class="note"></p></footer>
</main><script>
const DATA = __DATA__;
const names={openmp:'OpenMP',tbb:'Intel oneTBB',pstl:'C++ Parallel STL',sequential:'Sequential'};
function metricsFor(basis){return [
 {key:'median_ms',title:'1. Median runtime',unit:'ms',digits:3,factor:1},
 {key:basis==='scaling'?'scaling_efficiency':'efficiency',title:basis==='scaling'?'2. Thread scaling efficiency':'2. Efficiency vs sequential C++',unit:'%',digits:2,factor:100},
 {key:basis==='scaling'?'scaling_speedup':'speedup',title:basis==='scaling'?'3. Thread scaling speedup':'3. Speedup vs sequential C++',unit:'×',digits:3,factor:1}];}
const run=document.getElementById('run'),method=document.getElementById('method'),basis=document.getElementById('basis');
function format(value,m){return (value*m.factor).toFixed(m.digits)+(m.unit==='%'?'%':m.unit==='×'?'×':'');}
function render(){
 const d=DATA[run.value];
 basis.options[0].disabled=!d.scaling_available;
 if(basis.value==='scaling'&&!d.scaling_available)basis.value='sequential';
 const scaling=basis.value==='scaling';
 const sequentialOption=method.querySelector('option[value="sequential"]');
 sequentialOption.disabled=scaling;
 if(scaling&&method.value==='sequential')method.value='all';
 const methods=method.value==='all'?['openmp','tbb','pstl']:[method.value];
 const metrics=metricsFor(basis.value);
 document.getElementById('definitions').textContent=scaling
  ? 'Runtime = median per-call time. Thread scaling speedup = this implementation at 1 thread / this implementation at p threads, for the same point count and dataset. Thread scaling efficiency = that speedup / p × 100%. One-thread values are 1× and 100% by definition. These are measured ratios; superlinear results can still exceed 100%. No measured values are capped.'
  : 'Runtime = median per-call time. Speedup = sequential C++ median / implementation median from the same benchmark configuration. Efficiency = that speedup / requested thread count × 100%. A one-thread implementation can measure faster than the separate sequential reference, giving more than 100% without a benefit from adding threads. No measured values are capped.';
 if(!d.scaling_available)document.getElementById('definitions').textContent+=' Thread scaling requires one-thread measurements in this run.';
 document.getElementById('summary').textContent=`${d.points.map(n=>n.toLocaleString()).join(' / ')} points · ${d.threads.join(' / ')} requested threads · ${d.repeats.join(' / ')} timed samples · ${d.warmups.join(' / ')} warmup runs`;
 document.getElementById('source').textContent='Source: '+d.source+' · Compiler: '+d.compilers.join(', ')+' · Timing: '+(d.protocols.includes('interleaved_batched_v2')?'updated interleaved batches':'legacy short sequential-first samples (susceptible to timing drift)');
 const container=document.getElementById('tables');container.replaceChildren();
 for(const m of metrics){
  const section=document.createElement('section'),heading=document.createElement('h2');heading.textContent=m.title+' ('+m.unit+')';section.append(heading);
  const scroll=document.createElement('div');scroll.className='scroll';
  const table=document.createElement('table'),caption=document.createElement('caption');caption.textContent=(method.value==='all'?'OpenMP / Intel oneTBB / C++ Parallel STL':names[method.value])+' · Rows: requested threads · Columns: number of points';table.append(caption);
  const head=document.createElement('thead'),hr=document.createElement('tr');
  for(const text of ['Threads / points',...d.points.map(n=>n.toLocaleString())]){const th=document.createElement('th');th.scope='col';th.textContent=text;hr.append(th);}head.append(hr);table.append(head);
  const body=document.createElement('tbody');
  for(const t of d.threads){const tr=document.createElement('tr'),th=document.createElement('th');th.scope='row';th.textContent=t;tr.append(th);
   for(const n of d.points){const td=document.createElement('td'),values=document.createElement('div');values.className='values';
    for(const a of methods){const r=d.cells[t+':'+n]?.[a];if(!r)continue;const row=document.createElement('div');row.className='value';const label=document.createElement('span');label.className='method';label.textContent=names[a];const value=document.createElement('strong');value.textContent=format(r[m.key],m);row.append(label,value);values.append(row);}td.append(values);tr.append(td);
   }body.append(tr);
  }table.append(body);scroll.append(table);section.append(scroll);container.append(section);
 }
}
run.addEventListener('change',render);method.addEventListener('change',render);basis.addEventListener('change',render);document.getElementById('print').addEventListener('click',()=>window.print());render();
</script></body></html>
'''

def load(folder):
    cells = {}
    identities = {}
    protocols = set()
    points, threads, repeats, warmups, compilers = set(), set(), set(), set(), set()
    for path in sorted(folder.glob('n*_t*.csv')):
        match = re.fullmatch(r'n(\d+)_t(\d+)\.csv', path.name)
        if not match: continue
        n, t = map(int, match.groups())
        with path.open(newline='') as f: rows = list(csv.DictReader(f))
        algorithms = {}
        file_identities = set()
        for row in rows:
            if int(row['points']) != n: raise ValueError(f'Point-count mismatch: {path}')
            protocols.add(row.get('timing_protocol','legacy_short_separate_samples'))
            file_identities.add((row['source'],row['seed']))
            a = row['algorithm']
            if a in algorithms: raise ValueError(f'Duplicate implementation: {path}')
            expected_workers = 1 if a == 'sequential' else t
            if int(row['worker_limit']) != expected_workers: raise ValueError(f'Worker mismatch: {path}')
            values = {key:float(row[key]) for key in ('median_ms','efficiency','speedup')}
            if not all(math.isfinite(v) and v>0 for v in values.values()): raise ValueError(f'Invalid metric: {path}')
            algorithms[a] = values
            repeats.add(int(row['repeats']));warmups.add(int(row['warmups']));compilers.add(row['compiler'])
        if set(algorithms) != {'sequential','openmp','tbb','pstl'}: raise ValueError(f'Missing implementation: {path}')
        for a,values in algorithms.items():
            speed = algorithms['sequential']['median_ms']/values['median_ms']
            if not math.isclose(speed,values['speedup'],rel_tol=1e-10): raise ValueError(f'Invalid speedup: {path}')
            if not math.isclose(values['efficiency'],speed/(1 if a=='sequential' else t),rel_tol=1e-10): raise ValueError(f'Invalid efficiency: {path}')
        if len(file_identities) != 1: raise ValueError(f'Mixed datasets in {path}')
        identities[f'{t}:{n}'] = next(iter(file_identities))
        cells[f'{t}:{n}'] = algorithms;points.add(n);threads.add(t)
    if not cells: raise ValueError(f'No benchmark CSVs in {folder}')
    if len(cells) != len(points)*len(threads): raise ValueError(f'Incomplete thread/point grid: {folder}')
    scaling_available = 1 in threads
    if scaling_available:
        for t in threads:
            for n in points:
                if identities[f'{t}:{n}'] != identities[f'1:{n}']:
                    raise ValueError(f'Thread scaling requires matching datasets and seeds: {folder}, n={n}')
                for a in ('openmp','tbb','pstl'):
                    current = cells[f'{t}:{n}'][a]
                    baseline = cells[f'1:{n}'][a]['median_ms']
                    current['scaling_speedup'] = baseline / current['median_ms']
                    current['scaling_efficiency'] = current['scaling_speedup'] / t
    return {'scaling_available':scaling_available,'source':str(folder.resolve()),'protocols':sorted(protocols),'points':sorted(points),'threads':sorted(threads),'repeats':sorted(repeats),'warmups':sorted(warmups),'compilers':sorted(compilers),'cells':cells}

def static_tables(d):
    sections=[]
    prefix = 'scaling_' if d['scaling_available'] else ''
    efficiency_title = '2. Thread scaling efficiency' if prefix else '2. Efficiency vs sequential C++'
    speedup_title = '3. Thread scaling speedup' if prefix else '3. Speedup vs sequential C++'
    for key,title,unit,digits,factor in [('median_ms','1. Median runtime','ms',3,1),(prefix+'efficiency',efficiency_title,'%',2,100),(prefix+'speedup',speedup_title,'×',3,1)]:
        text=f'<section><h2>{title} ({unit})</h2><div class="scroll"><table><caption>OpenMP / Intel oneTBB / C++ Parallel STL · Rows: requested threads · Columns: number of points</caption><thead><tr><th scope="col">Threads / points</th>'
        text+=''.join(f'<th scope="col">{n:,}</th>' for n in d['points'])+'</tr></thead><tbody>'
        for t in d['threads']:
            text+=f'<tr><th scope="row">{t}</th>'
            for n in d['points']:
                text+='<td><div class="values">'
                for a,name in [('openmp','OpenMP'),('tbb','Intel oneTBB'),('pstl','C++ Parallel STL')]:
                    value=d['cells'][f'{t}:{n}'][a][key]*factor
                    suffix='%' if unit=='%' else '×' if unit=='×' else ''
                    text+=f'<div class="value"><span class="method">{name}</span><strong>{value:.{digits}f}{suffix}</strong></div>'
                text+='</div></td>'
            text+='</tr>'
        sections.append(text+'</tbody></table></div></section>')
    return ''.join(sections)

def main():
    project=Path(__file__).resolve().parent.parent
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folders',type=Path,nargs='*',help='Results folders; default: all timestamped runs in project/results')
    parser.add_argument('--output',type=Path,default=project/'results'/'benchmark_tables.html')
    args=parser.parse_args()
    folders=args.folders or sorted(p for p in (project/'results').iterdir() if p.is_dir() and any(p.glob('n*_t*.csv')))
    if not folders: parser.error('No results folders found; run scripts/run_experiments.sh first.')
    data={p.name:load(p) for p in folders}
    latest=sorted(data)[-1]
    options=''.join(f'<option value="{html.escape(name,quote=True)}"'+(' selected' if name==latest else '')+f'>{html.escape(name)}</option>' for name in sorted(data,reverse=True))
    output=TEMPLATE.replace('__OPTIONS__',options).replace('__TABLES__',static_tables(data[latest])).replace('__DATA__',json.dumps(data,separators=(',',':')).replace('</','<\\/'))
    scaling = data[latest]['scaling_available']
    definition = ('Thread scaling speedup = this implementation at 1 thread / this implementation at p threads. Thread scaling efficiency = that speedup / p × 100%. One-thread values are 1× and 100% by definition.' if scaling else 'Speedup = sequential C++ median / implementation median. Efficiency = that speedup / requested thread count × 100%.')
    definition += ' Runtime = median per-call time. Metrics are measured ratios and are not capped.'
    output = output.replace('__SCALING_SELECTED__','selected' if scaling else '').replace('__SEQUENTIAL_SELECTED__','' if scaling else 'selected').replace('__DEFINITIONS__',html.escape(definition))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(output,encoding='utf-8')
    print(f'Created {args.output.resolve()} with three tables and {len(data)} benchmark runs.')

if __name__=='__main__':main()
