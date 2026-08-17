# Proposed approaches were implemented in Julia using Gurobi

---

### 📖 Description

This directory contains the Julia implementation used to compare the proposed approaches with [Gurobi](https://www.gurobi.com/) (with the mixing cuts of Luedtke et al. (2010)) as reported in Table 6 of the paper **"Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side"**.

In particular, we compare the following two settings:

- `G-BASE`: solves formulation (MILP) using the B&C algorithm of Gurobi with the mixing cuts Luedtke et al. (2010).
- `G-BASE+sDI`: `G-BASE` with the enhanced version of dominance inequalities in (30).
  
### 📦 Dependencies & Installation

To run this implementation on a `Linux` system, install **Julia 1.11.7**, **Gurobi 12.0.3**, and a valid Gurobi license. Instructions for configuring a local Gurobi installation and license are available in the [Gurobi.jl documentation](https://jump.dev/JuMP.jl/stable/packages/Gurobi/).

From the repository root, instantiate the Julia project by running:

```bash
julia --project=Gurobi-Julia -e 'using Pkg; Pkg.instantiate()'
```

To verify that Julia can load the Gurobi interface and create a solver environment, run:

```bash
julia --project=Gurobi-Julia -e 'using Gurobi; Gurobi.Env(); println("Gurobi environment created successfully")'
```

### 🏃 Usage

From the repository root, use the following command to solve an instance:

```bash
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl --instance data/<P>Data/<filename> --setting <S> --epsilon <E>
```

Here:

- `<P>` denotes the problem type, chosen from `{CCRP, CCMPP, CCLS}`;
- `<S>` denotes the setting, chosen from `{G-BASE, G-BASE+sDI}`;  
- `<E>` encodes the value of $\epsilon$: for CCMPP and CCLS, `05`, `1`, and `2` correspond to $\epsilon=0.05$, $0.1$, and $0.2$, respectively; for CCRP, `1`, `15`, and `2` correspond to $\epsilon=0.1$, $0.15$, and $0.2$, respectively; and
- `<filename>` denotes the name of an instance in the corresponding data directory.

For example, the following commands solve the CCMPP instance `10-1000-0.ccmpp` with $\epsilon=0.05$ under the two settings reported in Table 6:

```bash
# setting G-BASE
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl --instance data/CCMPPData/10-1000-0.ccmpp --setting BASE --epsilon 0.05

# setting G-BASE+sDI
julia --project=Gurobi-Julia Gurobi-Julia/solve.jl --instance data/CCMPPData/10-1000-0.ccmpp --setting BASE+sDI --epsilon 0.05
```
