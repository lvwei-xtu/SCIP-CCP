ENV["LC_ALL"] = "C"
ENV["LANG"] = "C"
ENV["LANGUAGE"] = "C"

include(joinpath(@__DIR__, "src", "SCIPCCPGurobi.jl"))
using .SCIPCCPGurobi

exit(SCIPCCPGurobi.main(ARGS))
