include(joinpath(@__DIR__, "src", "CCPData.jl"))
using .CCPData
using Printf

if length(ARGS) != 3
    println(stderr, "Usage: julia Gurobi-Julia/inspect.jl INSTANCE SETTING EPSILON")
    exit(2)
end

instance = read_instance(ARGS[1])
setting = parse_setting(ARGS[2])
preprocessed = preprocess(instance, setting, parse(Float64, ARGS[3]))

println("Instance: ", ARGS[1])
println("Setting: ", setting_name(setting))
println("Epsilon: ", preprocessed.epsilon)
println("Dimension: ", size(instance.rhs, 2))
println("NumScenario: ", size(instance.rhs, 1))
println("NumAggScenario: ", size(preprocessed.aggregate_rhs, 1))
println("BasicNumScenario: ", length(preprocessed.basic_indices))
println("NumBasicDomIneqs: ", preprocessed.n_dominance_pairs)
println("NumNonReDomIneqs: ", length(preprocessed.dominance_edges))
println("NumEdge:        ", preprocessed.statistics.num_edges)
println("NumBiEdge:     ", preprocessed.statistics.num_bi_edges)
println(
    "NumDomIneqs:    ",
    preprocessed.statistics.num_dominance_inequalities,
)
@printf("DominanceRatio: %.4f\n", preprocessed.statistics.dominance_ratio)
