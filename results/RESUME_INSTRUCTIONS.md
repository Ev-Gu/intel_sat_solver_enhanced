# Resume ref-first benchmark

Stopped at **104/141** (continue from **105/141**).

When network is stable, run:

```bash
cd /root/bisan
./scripts/run_mse2022_bench.sh ref-first-wcnf --timeout 60 --resume 2>&1 | tee -a results/mse2022_ref_first_run.log
```

Checkpoint: `results/ref_first_checkpoint.json` (104 instances done, 37 remaining)

**Do not push to git unless explicitly requested.**
