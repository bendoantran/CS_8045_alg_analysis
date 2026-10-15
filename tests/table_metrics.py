"""Test metric baselines without hiding legitimately superlinear measurements."""
import csv
import importlib.util
import math
import tempfile
from pathlib import Path

project = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('make_tables',project/'scripts'/'make_tables.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def write_case(folder, t, seed='42'):
    times = {1:{'sequential':11,'openmp':12,'tbb':10,'pstl':9},
             2:{'sequential':11,'openmp':7,'tbb':4,'pstl':5}}[t]
    rows=[]
    for name,time in times.items():
        workers=1 if name=='sequential' else t
        rows.append(dict(algorithm=name,points=10,worker_limit=workers,repeats=3,warmups=1,
            source='generated',seed=seed,median_ms=time,speedup=11/time,
            efficiency=11/time/workers,compiler='fixture'))
    with (folder/f'n10_t{t}.csv').open('w',newline='') as file:
        writer=csv.DictWriter(file,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)

with tempfile.TemporaryDirectory() as path:
    folder=Path(path)
    write_case(folder,1);write_case(folder,2)
    data=module.load(folder)
    assert data['scaling_available']
    for name in ['openmp','tbb','pstl']:
        assert data['cells']['1:10'][name]['scaling_speedup']==1
        assert data['cells']['1:10'][name]['scaling_efficiency']==1
    assert math.isclose(data['cells']['2:10']['openmp']['scaling_efficiency'],12/14)
    assert data['cells']['2:10']['tbb']['scaling_efficiency']==1.25 # never clamp
    assert data['cells']['1:10']['tbb']['efficiency']==1.1 # original metric retained
    assert data['cells']['2:10']['tbb']['median_ms']==4 # raw timing retained
    write_case(folder,2,seed='99')
    try:
        module.load(folder)
    except ValueError as e:
        assert 'matching datasets and seeds' in str(e)
    else:
        raise AssertionError('Mixed datasets accepted for thread scaling')
    (folder/'n10_t1.csv').unlink()
    data=module.load(folder)
    assert not data['scaling_available']
    assert 'Efficiency vs sequential C++' in module.static_tables(data)
print('Passed both baselines, exact one-thread normalization, no clamping, and dataset checks.')
