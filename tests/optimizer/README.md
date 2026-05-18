# KBO optimizer golden fixtures

These fixtures lock the public behavior of `tools/kbo_optimizer.py` before code
quality refactors.

Run from the repository root:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\tests\optimizer\verify-kbo-optimizer-golden.ps1
```

The verifier exercises:

- default CLI mode: `kbo_optimizer.py REQUEST_CSV RESULT_CSV`
- explicit mode CLI: `kbo_optimizer.py --mode MODE REQUEST_CSV RESULT_CSV`
- server mode: `kbo_optimizer.py --server`
- amateur assignment single-player and batch paths

Golden output files are compared byte-for-byte.
