# Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side

## Description
This repository contains the code and data to the manuscript <br>
&nbsp;&nbsp;&nbsp;&nbsp;**Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side** <br>
by Wei Lv, Wei-Kun Chen, Yu-Hong Dai, and Xiao-Jiao Tong.

## Repository Structure

```text
.
├── CCP/                # Source code of the CCP example
│   ├── CMakeLists.txt  # Build script for the CCP example
│   └── src/            # C source code of the method implementation
├── data/               # Benchmark instance data used in the manuscript (CCRP, CCMPP, CCLS).
├── settings/           # Parameter setting files for running different solver configurations. (.set)
└── numerical-results/  # Logfiles produced by our numerical experiments.
```

## Installation
In order to run this code, you must install **SCIPOptSuite 9.1.0** from [https://scipopt.org/download/release/scipoptsuite-9.1.0.tgz](https://scipopt.org/download/release/scipoptsuite-9.1.0.tgz).

## Build and Run
```shell
# Enter the SCIP root directory
cd scipoptsuite-9.1.0
# Download this repository
# Move the CCP folder into SCIP examples
mv -r CCP scip/examples
# Add the example in the examples CMakeLists
cd  scip/examples
vim CMakeLists.txt
# Insert the following line:
#   add_subdirectory(CCP EXCLUDE_FROM_ALL)
# Back to SCIP root directory
cd ../..
# Build SCIP
mkdir -p build
cd build
cmake ..
make
# Build CCP example target
cd scip/examples/CCP
make
# Back to SCIP root directory
cd ../..
# Below are the commands to run different settings (using CCMPP data as an example)
# run setting B&C+MIX
./build/bin/examples/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BnC-MIX1.set
# run setting B&C+MIX+DI
./build/bin/examples/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BnC-MIX-DI1.set
# run setting BnC+MIX+sDI
./build/bin/examples/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BnC-MIX-sDI1.set
# run setting DB
./build/bin/examples/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/DB1.set
# run setting DB+OPF
./build/bin/examples/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/DB-OPF1.set
# run setting DB+EOPF
./build/bin/examples/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/DB-EOPF1.set