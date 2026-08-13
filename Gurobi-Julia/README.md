# Gurobi implementation in Julia
---

This directory implements the `BASE-MIX` and `BASE+sDI` settings with JuMP and Gurobi, denoted as `G-BASE-MIX` and  `G-BASE+sDI`:
- `G-BASE-MIX`: solving formulation (MILP) using the \BnC algorithm of Gurobi with the mixing cuts of \cite{Luedtke2010a}
- `G-BASE+sDI`: `G-BASE-MIX` with the proposed dominance inequalities in (32).

## Installation

Install Julia 1.11.7, Gurobi, and a valid Gurobi license. Then run:

```bash
cd Gurobi-Julia
julia --project=. -e 'using Pkg; Pkg.instantiate()'
```

Gurobi.jl documents the supported local-installation and license options at
<https://jump.dev/JuMP.jl/stable/packages/Gurobi/>.

## Usage

Run commands from the repository root. For example:

```bash
# BASE-MIX
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \
  --instance data/CCMPPData/10-1000-0.ccmpp \
  --setting BASE-MIX --epsilon 0.1

# BASE+sDI
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \
  --instance data/CCMPPData/10-1000-0.ccmpp \
  --setting BASE+sDI --epsilon 0.1