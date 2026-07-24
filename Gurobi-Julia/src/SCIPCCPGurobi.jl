module SCIPCCPGurobi

using JuMP
using Printf
import Gurobi
import MathOptInterface as MOI

include("CCPData.jl")
using .CCPData

export SolveOptions, build_model, main, solve_instance

Base.@kwdef struct SolveOptions
    epsilon::Float64 = 0.10
    setting::Symbol = :BASE
    equal_probability::Bool = true
    time_limit::Float64 = 7_200.0
    threads::Int = 1
    mip_gap::Float64 = 0.0
    output_flag::Int = 1
    cut_tolerance::Float64 = 1.0e-6
    gurobi_parameters::Dict{String,Any} = Dict{String,Any}()
end

struct ModelArtifacts
    model::Model
    z::Vector{VariableRef}
    mixing_variable::Vector{VariableRef}
    mixing_lower_bound::Vector{Float64}
    preprocessed::PreprocessedData
    mixing_cuts::Symbol
    mixing_scope::Symbol
    dominance_lazy::Int
    generated_mixing_cuts::Base.RefValue{Int}
end

mixing_cuts(setting::Symbol) = setting == :BASE_MIX ? :off : :on

function dominance_lazy(setting::Symbol)
    setting == :BASE_DB && return 3
    return 0
end

function configure_gurobi!(model::Model, options::SolveOptions)
    set_attribute(model, "TimeLimit", options.time_limit)
    set_attribute(model, "Threads", options.threads)
    set_attribute(model, "MIPGap", options.mip_gap)
    set_attribute(model, "OutputFlag", options.output_flag)
    if mixing_cuts(options.setting) == :on
        # Gurobi recommends PreCrush=1 for callback-generated user cuts.
        set_attribute(model, "PreCrush", 1)
    end
    for (name, value) in options.gurobi_parameters
        set_attribute(model, name, value)
    end
    return
end

function add_chance_formulation!(
    model::Model,
    z::Vector{VariableRef},
    v::Vector{VariableRef},
    prep::PreprocessedData,
)
    rhs = prep.aggregate_rhs
    for d in axes(rhs, 2)
        sorted = prep.aggregate_sorted[d]
        threshold = prep.thresholds[d]
        for rank in 1:prep.aggregate_ki[d]
            scenario = sorted[rank]
            coefficient = rhs[scenario, d] - threshold
            constraint = @constraint(
                model,
                v[d] + coefficient * z[scenario] >= rhs[scenario, d],
            )
            set_name(constraint, "strengthened_linearization_$(d)_$(rank)")
        end
    end
    chance = @constraint(
        model,
        sum(prep.aggregate_probability[s] * z[s] for s in eachindex(z)) <=
        prep.epsilon,
    )
    set_name(chance, "chance_knapsack")
    return
end

function add_dominance_constraints!(
    model::Model,
    z::Vector{VariableRef},
    prep::PreprocessedData,
    lazy_level::Int,
)
    lazy_level in 0:3 || error(
        "dominance_lazy must be an integer from 0 to 3; got $lazy_level",
    )
    grb = backend(model)
    for (i, (tail, head)) in enumerate(prep.dominance_edges)
        constraint = @constraint(model, z[tail] - z[head] >= 0.0)
        set_name(constraint, "dominance_$i")
        if lazy_level > 0 && grb isa Gurobi.Optimizer
            MOI.set(
                grb,
                Gurobi.ConstraintAttribute("Lazy"),
                index(constraint),
                lazy_level,
            )
        end
    end
    return
end

function build_ccrp_model(
    model::Model,
    instance::CCRPInstance,
    prep::PreprocessedData,
    dominance_lazy::Int,
)
    n_resource = instance.n_resource
    dimension = instance.dimension
    n_aggregate = size(prep.aggregate_rhs, 1)
    lower = max.(0.0, prep.thresholds)

    @variable(model, x[1:n_resource] >= 0.0)
    @variable(model, y[1:n_resource, 1:dimension] >= 0.0)
    @variable(model, z[1:n_aggregate], Bin)
    @variable(model, v[d=1:dimension] >= lower[d])
    @objective(
        model,
        Min,
        sum(instance.cost[i] * x[i] for i in 1:n_resource),
    )
    @constraint(
        model,
        service[j=1:dimension],
        v[j] == sum(
            instance.service_rate[i, j] * y[i, j]
            for i in 1:n_resource
        ),
    )
    @constraint(
        model,
        resource[i=1:n_resource],
        sum(y[i, j] for j in 1:dimension) <= x[i],
    )

    z_vector = collect(z)
    v_vector = collect(v)
    add_chance_formulation!(model, z_vector, v_vector, prep)
    add_dominance_constraints!(model, z_vector, prep, dominance_lazy)
    return z_vector, v_vector, lower
end

function build_ccmpp_model(
    model::Model,
    instance::CCMPPInstance,
    prep::PreprocessedData,
    dominance_lazy::Int,
)
    dimension = instance.dimension
    n_aggregate = size(prep.aggregate_rhs, 1)
    lower = max.(0.0, prep.thresholds)
    coal_lifetime = 15
    nuclear_lifetime = 10
    nuclear_fraction = 0.2

    @variable(model, x[1:dimension] >= 0.0)
    @variable(model, y[1:dimension] >= 0.0)
    @variable(model, z[1:n_aggregate], Bin)
    @variable(model, u[1:dimension] >= 0.0)
    @variable(model, nuclear[1:dimension] >= 0.0)
    @variable(model, w[d=1:dimension] >= lower[d])
    @objective(
        model,
        Min,
        sum(
            instance.coal_cost[t] * x[t] +
            instance.nuclear_cost[t] * y[t]
            for t in 1:dimension
        ),
    )
    @constraint(
        model,
        coal[t=1:dimension],
        u[t] == sum(x[i] for i in max(1, t - coal_lifetime + 1):t),
    )
    @constraint(
        model,
        nuclear_capacity[t=1:dimension],
        nuclear[t] == sum(y[i] for i in max(1, t - nuclear_lifetime + 1):t),
    )
    @constraint(
        model,
        total_capacity[t=1:dimension],
        w[t] == u[t] + nuclear[t],
    )
    @constraint(
        model,
        regulatory_limit[t=1:dimension],
        (1.0 - nuclear_fraction) * nuclear[t] - nuclear_fraction * u[t] <=
        nuclear_fraction * instance.existing_capacity[t],
    )

    z_vector = collect(z)
    w_vector = collect(w)
    add_chance_formulation!(model, z_vector, w_vector, prep)
    add_dominance_constraints!(model, z_vector, prep, dominance_lazy)
    return z_vector, w_vector, lower
end

function build_ccls_model(
    model::Model,
    instance::CCLSInstance,
    prep::PreprocessedData,
    dominance_lazy::Int,
)
    dimension = instance.dimension
    n_scenario = instance.n_scenario
    n_aggregate = size(prep.aggregate_rhs, 1)
    lower = max.(0.0, prep.thresholds)
    big_m = [
        maximum(sum(instance.demand[s, t:dimension]) for s in 1:n_scenario)
        for t in 1:dimension
    ]

    @variable(model, x[1:dimension], Bin)
    @variable(model, y[1:dimension] >= 0.0)
    @variable(model, z[1:n_aggregate], Bin)
    @variable(model, inventory[1:n_scenario, 1:dimension] >= 0.0)
    @variable(model, v[d=1:dimension] >= lower[d])
    @objective(
        model,
        Min,
        sum(
            instance.setup_cost[t] * x[t] +
            instance.production_cost[t] * y[t]
            for t in 1:dimension
        ) + sum(
            instance.probability[s] * instance.inventory_cost[t] * inventory[s, t]
            for s in 1:n_scenario, t in 1:dimension
        ),
    )
    @constraint(
        model,
        cumulative_production[t=1:dimension],
        v[t] == sum(y[i] for i in 1:t),
    )
    @constraint(
        model,
        production_capacity[t=1:dimension],
        y[t] <= big_m[t] * x[t],
    )
    for t in 1:dimension
        sorted = prep.original_sorted[t]
        for rank in 1:n_scenario
            scenario = sorted[rank]
            constraint = @constraint(
                model,
                inventory[rank, t] - v[t] >= -instance.rhs[scenario, t],
            )
            set_name(constraint, "inventory_$(rank)_$(t)")
        end
    end

    z_vector = collect(z)
    v_vector = collect(v)
    add_chance_formulation!(model, z_vector, v_vector, prep)
    add_dominance_constraints!(model, z_vector, prep, dominance_lazy)
    return z_vector, v_vector, lower
end

function install_mixing_callback!(
    model::Model,
    z::Vector{VariableRef},
    v::Vector{VariableRef},
    lower::Vector{Float64},
    prep::PreprocessedData,
    tolerance::Float64,
    generated_mixing_cuts::Base.RefValue{Int},
)
    rhs = prep.aggregate_rhs
    candidates = Vector{Vector{Tuple{Int,Float64}}}(undef, length(v))
    for d in eachindex(v)
        candidates[d] = Tuple{Int,Float64}[]
        sorted = prep.aggregate_sorted[d]
        for rank in 1:prep.aggregate_ki[d]
            scenario = sorted[rank]
            gap = rhs[scenario, d] - lower[d]
            gap > tolerance && push!(candidates[d], (scenario, gap))
        end
    end

    function mixing_callback(callback_data, callback_where::Cint)
        callback_where == Gurobi.GRB_CB_MIPNODE || return
        node_status = Ref{Cint}()
        return_code = Gurobi.GRBcbget(
            callback_data,
            callback_where,
            Gurobi.GRB_CB_MIPNODE_STATUS,
            node_status,
        )
        return_code == 0 || error(
            "GRBcbget(MIPNODE_STATUS) failed with code $return_code",
        )
        node_status[] == Gurobi.GRB_OPTIMAL || return

        node_count = Ref{Cdouble}()
        return_code = Gurobi.GRBcbget(
            callback_data,
            callback_where,
            Gurobi.GRB_CB_MIPNODE_NODCNT,
            node_count,
        )
        return_code == 0 || error(
            "GRBcbget(MIPNODE_NODCNT) failed with code $return_code",
        )
        node_count[] > 0.5 && return

        Gurobi.load_callback_variable_primal(callback_data, callback_where)
        z_value = callback_value.(Ref(callback_data), z)
        v_value = callback_value.(Ref(callback_data), v)
        for d in eachindex(v)
            data = candidates[d]
            isempty(data) && continue
            max_position = argmax(last.(data))
            max_scenario, max_gap = data[max_position]
            number = v_value[d] - lower[d]
            number > max_gap + tolerance && continue

            transformed = [clamp(1.0 - z_value[item[1]], 0.0, 1.0) for item in data]
            order = sortperm(transformed; rev=true, alg=Base.Sort.MergeSort)
            expression = AffExpr(0.0)
            add_to_expression!(expression, -1.0, v[d])
            cut_rhs = -lower[d]
            activity = -number
            last_gap = 0.0
            n_z_terms = 0

            for position in order
                scenario, gap = data[position]
                solution_value = transformed[position]
                if activity + solution_value * (max_gap - last_gap) < -tolerance ||
                   abs(solution_value) <= tolerance
                    break
                end
                gap <= last_gap + tolerance && continue
                delta = gap - last_gap
                activity += delta * solution_value
                cut_rhs -= delta
                add_to_expression!(expression, -delta, z[scenario])
                n_z_terms += 1
                last_gap = gap
            end

            if max_gap > last_gap + tolerance
                delta = max_gap - last_gap
                cut_rhs -= delta
                add_to_expression!(expression, -delta, z[max_scenario])
                n_z_terms += 1
            end

            # This reproduces separateCCP: at least two z terms and positive
            # efficacy are required before the row is submitted.
            if n_z_terms >= 2 && activity > tolerance
                cut = @build_constraint(expression <= cut_rhs)
                MOI.submit(model, MOI.UserCut(callback_data), cut)
                generated_mixing_cuts[] += 1
            end
        end
        return
    end

    set_attribute(
        model,
        Gurobi.CallbackFunction(),
        mixing_callback,
    )
    return
end

function build_model(
    instance::AbstractCCPInstance,
    options::SolveOptions;
    optimizer=Gurobi.Optimizer,
    install_callback::Bool=true,
)
    prep = preprocess(instance, options.setting, options.epsilon)
    mixing_mode = mixing_cuts(options.setting)
    lazy_level = dominance_lazy(options.setting)
    model = optimizer === nothing ? Model() : direct_model(optimizer())
    set_attribute(model, MOI.Name(), splitext(basename(instance.path))[1])
    optimizer === nothing || configure_gurobi!(model, options)

    if instance isa CCRPInstance
        z, mixing_variable, lower = build_ccrp_model(
            model,
            instance,
            prep,
            lazy_level,
        )
    elseif instance isa CCMPPInstance
        z, mixing_variable, lower = build_ccmpp_model(
            model,
            instance,
            prep,
            lazy_level,
        )
    elseif instance isa CCLSInstance
        z, mixing_variable, lower = build_ccls_model(
            model,
            instance,
            prep,
            lazy_level,
        )
    else
        error("unsupported instance type: $(typeof(instance))")
    end
    generated_mixing_cuts = Ref(0)
    if install_callback && mixing_mode == :on
        optimizer === nothing && error(
            "a solver is required when install_callback=true",
        )
        install_mixing_callback!(
            model,
            z,
            mixing_variable,
            lower,
            prep,
            options.cut_tolerance,
            generated_mixing_cuts,
        )
    end
    return ModelArtifacts(
        model,
        z,
        mixing_variable,
        lower,
        prep,
        mixing_mode,
        :root,
        lazy_level,
        generated_mixing_cuts,
    )
end

function print_preprocessing(instance::AbstractCCPInstance, artifacts::ModelArtifacts)
    prep = artifacts.preprocessed
    println("Instance: ", instance.path)
    println("Setting: ", setting_name(prep.setting))
    println("Epsilon: ", prep.epsilon)
    println("Dimension: ", size(instance.rhs, 2))
    println("NumScenario: ", size(instance.rhs, 1))
    println("NumAggScenario: ", size(prep.aggregate_rhs, 1))
    println("BasicNumScenario: ", length(prep.basic_indices))
    println("NumBasicDomIneqs: ", prep.n_dominance_pairs)
    println("NumNonReDomIneqs: ", length(prep.dominance_edges))
    println("NumEdge:        ", prep.statistics.num_edges)
    println("NumBiEdge:      ", prep.statistics.num_bi_edges)
    println(
        "NumDomIneqs:    ",
        prep.statistics.num_dominance_inequalities,
    )
    println(
        "DominanceRatio: ",
        @sprintf("%.4f", prep.statistics.dominance_ratio),
    )
    println("MixingCuts: ", artifacts.mixing_cuts)
    println("MixingScope: ", artifacts.mixing_scope)
    println("DominanceLazy: ", artifacts.dominance_lazy)
    return
end

function solve_instance(path::AbstractString, options::SolveOptions; write_model=nothing)
    instance = read_instance(
        path;
        equal_probability=options.equal_probability,
    )
    artifacts = build_model(instance, options)
    print_preprocessing(instance, artifacts)
    write_model === nothing || write_to_file(artifacts.model, write_model)
    optimize!(artifacts.model)

    model = artifacts.model
    println("TerminationStatus: ", termination_status(model))
    println("PrimalStatus: ", primal_status(model))
    println("RawStatus: ", raw_status(model))
    println("SolvingTime: ", solve_time(model))
    println("NodeCount: ", node_count(model))
    println("MixingCutsSubmitted: ", artifacts.generated_mixing_cuts[])
    if has_values(model)
        println("ObjectiveValue: ", objective_value(model))
    end
    if has_values(model) || termination_status(model) == MOI.TIME_LIMIT
        println("ObjectiveBound: ", objective_bound(model))
        println("RelativeGap: ", relative_gap(model))
    end
    return artifacts
end

function parse_bool(value::AbstractString)
    normalized = lowercase(strip(value))
    normalized in ("true", "1", "yes") && return true
    normalized in ("false", "0", "no") && return false
    error("expected a boolean value; got $value")
end

function parse_parameter_value(value::AbstractString)
    lowercase(value) in ("true", "false") && return parse_bool(value)
    integer = tryparse(Int, value)
    integer === nothing || return integer
    real = tryparse(Float64, value)
    real === nothing || return real
    return String(value)
end

function usage(io::IO=stdout)
    println(io, "Usage:")
    println(io, "  julia --project=Gurobi-Julia Gurobi-Julia/solve.jl \\")
    println(io, "    --instance data/CCMPPData/10-1000-0.ccmpp \\")
    println(io, "    --setting BASE+DI --epsilon 0.1")
    println(io)
    println(io, "Options:")
    println(
        io,
        "  --setting BASE-MIX|BASE|BASE+DI|BASE+sDI|BASE+DB   default: BASE",
    )
    println(io, "  --epsilon VALUE                  default: 0.1")
    println(io, "  --time-limit SECONDS             default: 7200")
    println(io, "  --threads COUNT                  default: 1")
    println(io, "  --mip-gap VALUE                  default: 0")
    println(io, "  --equal-probability BOOL         default: true")
    println(io, "  --output-flag 0|1                default: 1")
    println(io, "  --cut-tolerance VALUE            default: 1e-6")
    println(io, "  --gurobi-param NAME=VALUE        may be repeated")
    println(io, "  --write-model PATH               optional .lp/.mps output")
    return
end

function main(args::Vector{String}=ARGS)
    isempty(args) && (usage(stderr); return 2)
    values = Dict{String,String}()
    gurobi_parameters = Dict{String,Any}()
    i = 1
    while i <= length(args)
        arg = args[i]
        if arg in ("-h", "--help")
            usage()
            return 0
        elseif arg == "--gurobi-param"
            i == length(args) && error("missing value after --gurobi-param")
            parameter = split(args[i + 1], '='; limit=2)
            length(parameter) == 2 || error("use --gurobi-param NAME=VALUE")
            gurobi_parameters[parameter[1]] = parse_parameter_value(parameter[2])
            i += 2
        elseif startswith(arg, "--")
            i == length(args) && error("missing value after $arg")
            values[arg] = args[i + 1]
            i += 2
        else
            error("unexpected argument: $arg")
        end
    end

    allowed_options = (
        "--instance",
        "--setting",
        "--epsilon",
        "--equal-probability",
        "--time-limit",
        "--threads",
        "--mip-gap",
        "--output-flag",
        "--cut-tolerance",
        "--write-model",
    )
    for name in keys(values)
        name in allowed_options || error("unsupported option: $name")
    end

    haskey(values, "--instance") || error("--instance is required")
    options = SolveOptions(
        epsilon=parse(Float64, get(values, "--epsilon", "0.1")),
        setting=parse_setting(get(values, "--setting", "BASE")),
        equal_probability=parse_bool(get(values, "--equal-probability", "true")),
        time_limit=parse(Float64, get(values, "--time-limit", "7200")),
        threads=parse(Int, get(values, "--threads", "1")),
        mip_gap=parse(Float64, get(values, "--mip-gap", "0")),
        output_flag=parse(Int, get(values, "--output-flag", "1")),
        cut_tolerance=parse(Float64, get(values, "--cut-tolerance", "1e-6")),
        gurobi_parameters=gurobi_parameters,
    )
    write_model = get(values, "--write-model", nothing)
    solve_instance(values["--instance"], options; write_model=write_model)
    return 0
end

end
