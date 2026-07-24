# Gurobi implementation in Julia

This directory implements the `BASE-MIX`, `BASE`, `BASE+DI`, `BASE+sDI`, and
`BASE+DB` settings with JuMP and Gurobi. It reads the original CCRP, CCMPP,
and CCLS instance files in `../data`.

The implementation follows the SCIP code as follows:

- `reader_ccrp.c`, `reader_ccmpp.c`, and `reader_ccls.c`: data parsing and the
  three MILP formulations;
- `processdata.c`: sorting, right-hand-side strengthening, duplicate-scenario
  aggregation, and basic-scenario detection;
- `graph.c`: coordinate-wise dominance and transitive-redundancy removal;
- `chancecons_ccp.c::separateCCP`: mixing-cut separation.

The setting name completely determines the algorithm. Mixing inequalities are
submitted as `MOI.UserCut` rows only during root-node cut passes. Dominance
inequalities, when enabled by the setting, are created as model constraints;
they are never added from a callback.

- `BASE-MIX`: no mixing cuts and no dominance inequalities;
- `BASE`: root-node mixing cuts and no dominance inequalities;
- `BASE+DI`: root-node mixing cuts and ordinary dominance constraints;
- `BASE+sDI`: root-node mixing cuts and strong dominance constraints;
- `BASE+DB`: root-node mixing cuts and strong dominance constraints marked
  with Gurobi's `Lazy=3` attribute.

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

# BASE
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \
  --instance data/CCMPPData/10-1000-0.ccmpp \
  --setting BASE --epsilon 0.1

# BASE+DI
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \
  --instance data/CCMPPData/10-1000-0.ccmpp \
  --setting BASE+DI --epsilon 0.1

# BASE+sDI
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \
  --instance data/CCMPPData/10-1000-0.ccmpp \
  --setting BASE+sDI --epsilon 0.1

# BASE+DB
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \
  --instance data/CCMPPData/10-1000-0.ccmpp \
  --setting BASE+DB --epsilon 0.1
```

The defaults match the paper's main runs: equal scenario probabilities,
`Threads=1`, `TimeLimit=7200`, and `MIPGap=0`. Use `--help` for all options.
Raw Gurobi parameters can be passed repeatedly, for example:

```bash
--gurobi-param Seed=1 --gurobi-param Presolve=2
```

The root-node test uses Gurobi's `MIPNODE_NODCNT` callback value, rather than
assuming that the first callback invocation is the only root callback. Gurobi
may invoke the callback multiple times during root-node cut passes.

The command-line options `--mixing-cuts`, `--mixing-scope`,
`--dominance-lazy`, and `--dominance-callback` are intentionally unsupported.
Use one of the five setting names instead.

To inspect scenario aggregation and dominance counts without loading Gurobi:

```bash
julia Gurobi-Julia/inspect.jl \
  data/CCMPPData/10-1000-0.ccmpp BASE+sDI 0.1
```

## Verification

The pure-Julia preprocessing tests compare aggregate-scenario and dominance
counts with the existing SCIP logs:

```bash
julia Gurobi-Julia/test/runtests.jl
```

Gurobi and SCIP use different presolve, cut management, heuristics, and
branching implementations. Therefore, matching formulations and user-cut
logic do not imply identical node counts or running times. In particular,
Gurobi decides when its user-cut callback is invoked, whereas the SCIP
constraint handler uses its own separation frequency.
