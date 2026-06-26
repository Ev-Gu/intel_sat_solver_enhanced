# MaxSAT IPAMIR Tools

## WCNF → IPAMIR loaders (Yevgeny's approach)

| Binary | Library |
|--------|---------|
| `bin/ipamir_wcnf_ours` | IntelSatSolver |
| `bin/ipamir_wcnf_uwrmaxsat` | UWrMaxSat 1.4 (reference) |
| `bin/ipamir_wcnf_main` | symlink → `ipamir_wcnf_ours` |

### Build

```bash
bash tools/install_ipamir.sh
bash tools/build_ipamir_wcnf.sh
```

### Fuzzer 2 (ours vs UWrMaxSat on same WCNF)

```bash
./scripts/fuzz_maxsat_ipamir.sh -n 10 --nuwls-time 1
```

### Single instance

```bash
tools/bin/ipamir_wcnf_ours instance.wcnf
tools/bin/ipamir_wcnf_uwrmaxsat instance.wcnf
```

### Batch vs IPAMIR (our solver only)

```bash
./scripts/compare_wcnf_batch_vs_ipamir.sh instance.wcnf
```
