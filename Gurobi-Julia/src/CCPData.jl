module CCPData

export AbstractCCPInstance,
       CCLSInstance,
       CCMPPInstance,
       CCRPInstance,
       DominanceStatistics,
       PreprocessedData,
       dominance_statistics,
       dominance_edges,
       parse_setting,
       preprocess,
       read_instance,
       setting_name

const NUM_PATTERN = raw"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
const NUM_RE = Regex(NUM_PATTERN)
const VECTOR_RE = r"\[([^\[\]]*)\]"s
const TOL = 1.0e-9

abstract type AbstractCCPInstance end

struct CCRPInstance <: AbstractCCPInstance
    path::String
    n_resource::Int
    dimension::Int
    n_scenario::Int
    service_rate::Matrix{Float64}  # resource x customer
    cost::Vector{Float64}
    rhs::Matrix{Float64}           # scenario x customer
    probability::Vector{Float64}
end

struct CCMPPInstance <: AbstractCCPInstance
    path::String
    dimension::Int
    n_scenario::Int
    coal_cost::Vector{Float64}
    nuclear_cost::Vector{Float64}
    existing_capacity::Vector{Float64}
    rhs::Matrix{Float64}           # demand minus existing capacity
    probability::Vector{Float64}
end

struct CCLSInstance <: AbstractCCPInstance
    path::String
    dimension::Int
    n_scenario::Int
    setup_cost::Vector{Float64}
    production_cost::Vector{Float64}
    inventory_cost::Vector{Float64}
    demand::Matrix{Float64}        # non-cumulative demand
    rhs::Matrix{Float64}           # cumulative demand
    probability::Vector{Float64}
end

struct DominanceStatistics
    num_edges::Int
    num_bi_edges::Int
    num_dominance_inequalities::Int
    dominance_ratio::Float64
end

struct PreprocessedData
    setting::Symbol
    epsilon::Float64
    original_sorted::Vector{Vector{Int}}
    original_ki::Vector{Int}
    strengthened_rhs::Matrix{Float64}
    aggregate_rhs::Matrix{Float64}
    aggregate_probability::Vector{Float64}
    aggregate_sorted::Vector{Vector{Int}}
    aggregate_ki::Vector{Int}
    thresholds::Vector{Float64}
    basic_indices::Vector{Int}
    dominance_edges::Vector{Tuple{Int,Int}}
    n_dominance_pairs::Int
    statistics::DominanceStatistics
end

parse_numbers(text::AbstractString) =
    [parse(Float64, m.match) for m in eachmatch(NUM_RE, text)]

function bracket_matches(text::AbstractString)
    return collect(eachmatch(VECTOR_RE, text))
end

function checked_vector(match::RegexMatch, expected::Int, label::String)
    values = parse_numbers(match.captures[1])
    length(values) == expected || error(
        "$label has $(length(values)) values; expected $expected",
    )
    return values
end

function rows_to_matrix(rows::Vector{Vector{Float64}}, ncol::Int, label::String)
    matrix = Matrix{Float64}(undef, length(rows), ncol)
    for (i, row) in enumerate(rows)
        length(row) == ncol || error(
            "$label row $i has $(length(row)) values; expected $ncol",
        )
        matrix[i, :] = row
    end
    return matrix
end

function first_two_integers(text::AbstractString)
    lines = filter(!isempty, strip.(split(text, '\n')))
    length(lines) >= 2 || error("data file must start with two integer lines")
    return parse(Int, lines[1]), parse(Int, lines[2])
end

function scenario_count_from_ccrp_filename(path::AbstractString)
    fields = split(splitext(basename(path))[1], '-')
    length(fields) >= 3 || error(
        "CCRP filename must have the form |I|-|J|-n-k.ccrp: $path",
    )
    return parse(Int, fields[3])
end

function equal_or_file_probability(
    probability::Vector{Float64},
    n_scenario::Int,
    equal_probability::Bool,
)
    if equal_probability
        return fill(1.0 / n_scenario, n_scenario)
    end
    length(probability) == n_scenario || error("invalid probability vector")
    abs(sum(probability) - 1.0) <= 1.0e-5 || error(
        "scenario probabilities sum to $(sum(probability)), not 1",
    )
    return probability
end

function read_ccrp(path::AbstractString; equal_probability::Bool=true)
    text = read(path, String)
    n_resource, dimension = first_two_integers(text)
    n_scenario = scenario_count_from_ccrp_filename(path)
    matches = bracket_matches(text)
    expected = dimension + 1 + n_scenario
    length(matches) >= expected || error(
        "CCRP parser found $(length(matches)) vectors; expected at least $expected",
    )

    service_rows = [
        checked_vector(matches[j], n_resource, "service-rate vector $j")
        for j in 1:dimension
    ]
    service_rate = permutedims(rows_to_matrix(
        service_rows,
        n_resource,
        "service-rate matrix",
    ))
    service_rate[service_rate .== -1.0] .= 0.0
    cost = checked_vector(matches[dimension + 1], n_resource, "cost vector")
    scenario_rows = [
        abs.(checked_vector(
            matches[dimension + 1 + s],
            dimension,
            "scenario $s",
        )) for s in 1:n_scenario
    ]
    rhs = rows_to_matrix(scenario_rows, dimension, "CCRP scenarios")
    probability = equal_or_file_probability(Float64[], n_scenario, equal_probability)
    return CCRPInstance(
        String(path),
        n_resource,
        dimension,
        n_scenario,
        service_rate,
        cost,
        rhs,
        probability,
    )
end

function parse_weighted_vectors(
    text::AbstractString,
    dimension::Int,
    n_scenario::Int,
)
    matches = bracket_matches(text)
    length(matches) == 3 + n_scenario || error(
        "parser found $(length(matches)) vectors; expected $(3 + n_scenario)",
    )
    fixed = [checked_vector(matches[i], dimension, "fixed vector $i") for i in 1:3]

    third = matches[3]
    tail_start = third.offset + ncodeunits(third.match)
    tail = SubString(text, tail_start)
    scenario_re = Regex("(" * NUM_PATTERN * ")\\s*\\[([^\\[\\]]*)\\]", "s")
    scenario_matches = collect(eachmatch(scenario_re, tail))
    length(scenario_matches) == n_scenario || error(
        "parser found $(length(scenario_matches)) weighted scenarios; expected $n_scenario",
    )
    probability = [parse(Float64, m.captures[1]) for m in scenario_matches]
    rows = Vector{Vector{Float64}}(undef, n_scenario)
    for (s, match) in enumerate(scenario_matches)
        row = parse_numbers(match.captures[2])
        length(row) == dimension || error(
            "scenario $s has $(length(row)) values; expected $dimension",
        )
        rows[s] = abs.(row)
    end
    return fixed, rows_to_matrix(rows, dimension, "weighted scenarios"), probability
end

function read_ccmpp(path::AbstractString; equal_probability::Bool=true)
    text = read(path, String)
    dimension, n_scenario = first_two_integers(text)
    fixed, demand, file_probability = parse_weighted_vectors(
        text,
        dimension,
        n_scenario,
    )
    probability = equal_or_file_probability(
        file_probability,
        n_scenario,
        equal_probability,
    )
    rhs = demand .- permutedims(fixed[3])
    return CCMPPInstance(
        String(path),
        dimension,
        n_scenario,
        fixed[1],
        fixed[2],
        fixed[3],
        rhs,
        probability,
    )
end

function read_ccls(path::AbstractString; equal_probability::Bool=true)
    text = read(path, String)
    dimension, n_scenario = first_two_integers(text)
    fixed, demand, file_probability = parse_weighted_vectors(
        text,
        dimension,
        n_scenario,
    )
    probability = equal_or_file_probability(
        file_probability,
        n_scenario,
        equal_probability,
    )
    rhs = cumsum(demand; dims=2)
    return CCLSInstance(
        String(path),
        dimension,
        n_scenario,
        fixed[1],
        fixed[2],
        fixed[3],
        demand,
        rhs,
        probability,
    )
end

function read_instance(path::AbstractString; equal_probability::Bool=true)
    isfile(path) || error("instance file does not exist: $path")
    extension = lowercase(splitext(path)[2])
    if extension == ".ccrp"
        return read_ccrp(path; equal_probability=equal_probability)
    elseif extension == ".ccmpp"
        return read_ccmpp(path; equal_probability=equal_probability)
    elseif extension == ".ccls"
        return read_ccls(path; equal_probability=equal_probability)
    end
    error("unsupported instance extension: $extension")
end

function parse_setting(value::AbstractString)
    normalized = uppercase(replace(strip(value), " " => ""))
    if normalized in ("BASE-MIX", "BASE_MIX")
        return :BASE_MIX
    elseif normalized == "BASE"
        return :BASE
    elseif normalized in ("BASE+DI", "BASE_DI")
        return :BASE_DI
    elseif normalized in ("BASE+SDI", "BASE_SDI")
        return :BASE_sDI
    elseif normalized in ("BASE+DB", "BASE_DB")
        return :BASE_DB
    end
    error(
        "setting must be BASE-MIX, BASE, BASE+DI, BASE+sDI, or BASE+DB; got $value",
    )
end

function setting_name(setting::Symbol)
    setting == :BASE_MIX && return "BASE-MIX"
    setting == :BASE && return "BASE"
    setting == :BASE_DI && return "BASE+DI"
    setting == :BASE_sDI && return "BASE+sDI"
    setting == :BASE_DB && return "BASE+DB"
    error("unknown setting: $setting")
end

function sorted_rhs_data(
    rhs::Matrix{Float64},
    probability::Vector{Float64},
    epsilon::Float64,
)
    n_scenario, dimension = size(rhs)
    sorted = Vector{Vector{Int}}(undef, dimension)
    ki = Vector{Int}(undef, dimension)
    thresholds = Vector{Float64}(undef, dimension)
    for d in 1:dimension
        sorted[d] = sortperm(
            view(rhs, :, d);
            rev=true,
            alg=Base.Sort.MergeSort,
        )
        cumulative = 0.0
        threshold_rank = 0
        for rank in 1:n_scenario
            cumulative += probability[sorted[d][rank]]
            if cumulative > epsilon + TOL
                threshold_rank = rank
                break
            end
        end
        threshold_rank > 0 || error(
            "epsilon=$epsilon is not smaller than total scenario probability",
        )
        ki[d] = threshold_rank - 1
        thresholds[d] = rhs[sorted[d][threshold_rank], d]
    end
    return sorted, ki, thresholds
end

function strengthen_rhs(rhs::Matrix{Float64}, thresholds::Vector{Float64})
    strengthened = copy(rhs)
    for d in axes(rhs, 2), s in axes(rhs, 1)
        strengthened[s, d] = max(rhs[s, d], thresholds[d])
    end
    return strengthened
end

function aggregate_scenarios(
    rhs::Matrix{Float64},
    probability::Vector{Float64},
)
    index_by_row = Dict{Tuple,Int}()
    rows = Vector{Vector{Float64}}()
    aggregate_probability = Float64[]
    for s in axes(rhs, 1)
        key = Tuple(view(rhs, s, :))
        index = get(index_by_row, key, 0)
        if index == 0
            push!(rows, collect(view(rhs, s, :)))
            push!(aggregate_probability, probability[s])
            index_by_row[key] = length(rows)
        else
            aggregate_probability[index] += probability[s]
        end
    end
    return rows_to_matrix(rows, size(rhs, 2), "aggregated scenarios"),
           aggregate_probability
end

function coordinate_relation(
    rhs::Matrix{Float64},
    first::Int,
    second::Int,
)
    first_ge = true
    first_le = true
    for d in axes(rhs, 2)
        first_ge &= rhs[first, d] >= rhs[second, d] - TOL
        first_le &= rhs[first, d] <= rhs[second, d] + TOL
        (!first_ge && !first_le) && break
    end
    if first_ge && !first_le
        return 1
    elseif first_le && !first_ge
        return -1
    end
    return 0
end

function dominance_edges(
    rhs::Matrix{Float64},
    selected::Vector{Int}=collect(axes(rhs, 1)),
)
    n = length(selected)
    outgoing = [BitSet() for _ in 1:n]
    incoming = [BitSet() for _ in 1:n]
    all_edges = Tuple{Int,Int}[]
    for i in 1:n
        for j in (i + 1):n
            relation = coordinate_relation(rhs, selected[i], selected[j])
            if relation == 1
                push!(outgoing[i], j)
                push!(incoming[j], i)
                push!(all_edges, (i, j))
            elseif relation == -1
                push!(outgoing[j], i)
                push!(incoming[i], j)
                push!(all_edges, (j, i))
            end
        end
    end

    reduced = Tuple{Int,Int}[]
    for (tail, head) in all_edges
        redundant = false
        if length(outgoing[tail]) <= length(incoming[head])
            for middle in outgoing[tail]
                if middle != head && head in outgoing[middle]
                    redundant = true
                    break
                end
            end
        else
            for middle in incoming[head]
                if middle != tail && middle in outgoing[tail]
                    redundant = true
                    break
                end
            end
        end
        redundant || push!(reduced, (selected[tail], selected[head]))
    end
    return reduced, length(all_edges)
end

function dominance_statistics(rhs::Matrix{Float64})
    n_scenario = size(rhs, 1)
    num_edges = 0
    num_bi_edges = 0
    for i in 1:n_scenario
        for j in (i + 1):n_scenario
            is_ge = true
            is_le = true
            for d in axes(rhs, 2)
                if rhs[i, d] < rhs[j, d] - TOL
                    is_ge = false
                    break
                end
            end
            for d in axes(rhs, 2)
                if rhs[i, d] > rhs[j, d] + TOL
                    is_le = false
                    break
                end
            end
            if is_ge
                num_edges += 1
                if is_le
                    num_edges += 1
                    num_bi_edges += 1
                end
            elseif is_le
                num_edges += 1
            end
        end
    end
    num_dominance_inequalities = num_edges - num_bi_edges
    num_pairs = n_scenario * (n_scenario - 1) ÷ 2
    ratio = num_pairs == 0 ? 0.0 : num_dominance_inequalities / num_pairs
    return DominanceStatistics(
        num_edges,
        num_bi_edges,
        num_dominance_inequalities,
        ratio,
    )
end

function preprocess(
    instance::AbstractCCPInstance,
    setting::Symbol,
    epsilon::Real,
)
    0.0 < epsilon < 1.0 || error("epsilon must lie strictly between 0 and 1")
    rhs = instance.rhs
    probability = instance.probability
    original_sorted, original_ki, thresholds = sorted_rhs_data(
        rhs,
        probability,
        Float64(epsilon),
    )
    strengthened = strengthen_rhs(rhs, thresholds)
    source_rhs = setting in (:BASE_sDI, :BASE_DB) ? strengthened : rhs
    statistics = dominance_statistics(source_rhs)
    aggregate_rhs, aggregate_probability = aggregate_scenarios(
        source_rhs,
        probability,
    )
    aggregate_sorted, aggregate_ki, aggregate_thresholds = sorted_rhs_data(
        aggregate_rhs,
        aggregate_probability,
        Float64(epsilon),
    )
    for d in eachindex(thresholds)
        abs(thresholds[d] - aggregate_thresholds[d]) <= 1.0e-7 || error(
            "aggregation changed threshold in dimension $d",
        )
    end
    basic_indices = [
        s for s in axes(aggregate_rhs, 1)
        if any(
            aggregate_rhs[s, d] > aggregate_thresholds[d] + TOL
            for d in axes(aggregate_rhs, 2)
        )
    ]

    edges = Tuple{Int,Int}[]
    n_pairs = 0
    if setting == :BASE_DI
        edges, n_pairs = dominance_edges(aggregate_rhs)
    elseif setting in (:BASE_sDI, :BASE_DB)
        edges, n_pairs = dominance_edges(aggregate_rhs, basic_indices)
    elseif !(setting in (:BASE_MIX, :BASE))
        error("unsupported setting: $setting")
    end

    return PreprocessedData(
        setting,
        Float64(epsilon),
        original_sorted,
        original_ki,
        strengthened,
        aggregate_rhs,
        aggregate_probability,
        aggregate_sorted,
        aggregate_ki,
        aggregate_thresholds,
        basic_indices,
        edges,
        n_pairs,
        statistics,
    )
end

end
