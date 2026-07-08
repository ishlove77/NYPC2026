#!/bin/bash
#SBATCH --job-name=jyp_moe
#SBATCH --partition=all
#SBATCH --qos=q-high-jinyoung.park
#SBATCH --array=0-3
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=24:00:00
#SBATCH --open-mode=append
#SBATCH --output=ga_run_moe%a.out
#SBATCH --error=ga_run_moe%a.out

set -e
cd /gallery_rijks/jinyoung.park/NYPC/genetic/ga_work2
K=$SLURM_ARRAY_TASK_ID
echo "# ======== MoE expert $K job $SLURM_JOB_ID host=$(hostname) cpus=$SLURM_CPUS_PER_TASK start=$(date) ========"
python3 ga_moe.py --expert "$K" --pop 48 --nseeds 50 --workers "$SLURM_CPUS_PER_TASK" \
    --mut 0.03 --race-top 4 --race-games 200 --max-min 1400 --resume
echo "# ======== expert $K done $(date) ========"
