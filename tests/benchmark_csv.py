"""Check batched measurement CSV metadata, numeric summaries, and result parity."""
import csv
import math
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

executable = sys.argv[1]
with tempfile.TemporaryDirectory() as folder:
    for target, repeats in [(5, 4), (0, 3)]:
        output = Path(folder) / f'target_{target}.csv'
        subprocess.run([executable, '--points', '64', '--threads', '2', '--repeats', str(repeats),
                        '--warmups', '1', '--min-sample-ms', str(target), '--csv', str(output)],
                       check=True, capture_output=True, text=True)
        with output.open(newline='') as file:
            rows = list(csv.DictReader(file))
        assert {r['algorithm'] for r in rows} == {'sequential','openmp','tbb','pstl'}
        assert len({(r['i'],r['j'],r['distance']) for r in rows}) == 1
        baseline = float(next(r['median_ms'] for r in rows if r['algorithm']=='sequential'))
        for row in rows:
            samples = [float(row[f'sample_{i}_ms']) for i in range(1,repeats+1)]
            assert all(math.isfinite(v) and v > 0 for v in samples)
            assert row['timing_protocol'] == 'interleaved_batched_v2'
            assert int(row['min_sample_ms']) == target
            iterations = int(row['sample_iterations'])
            assert iterations >= 1 and iterations & (iterations-1) == 0
            assert iterations > 1 if target else iterations == 1
            median = float(row['median_ms'])
            assert math.isclose(median, statistics.median(samples), rel_tol=1e-12)
            assert float(row['min_ms']) == min(samples)
            assert float(row['max_ms']) == max(samples)
            assert math.isclose(float(row['speedup']),baseline/median,rel_tol=1e-12)
            assert math.isclose(float(row['efficiency']),baseline/median/int(row['worker_limit']),rel_tol=1e-12)
print('Batched and unbatched CSV validation passed, including even/odd medians.')
