# Source code, instances, and detailed computational results of the paper: 

### Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side

### Wei Lv, Wei-Kun Chen, Yu-Hong Dai, and Xiao-Jiao Tong

---

### Description 

This repository contains the source code, instances, and detailed results of the computational experiments conducted for the paper **"Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side"**. 

#### [![arXiv](https://img.shields.io/badge/arXiv-2406.10472-b31b1b.svg)](https://arxiv.org/abs/2406.10472)

### Repository Structure

```text
SCIP-CCP
├── CMakeLists.txt      # CMake build configuration script
|── src/                # Source code for implementing our proposed approaches
├── data/               # Benchmark instances (CCRP, CCMPP, CCLS)
├── settings/           # Configuration files with different settings
└── results/            # Logfiles produced by our computational experiments
```

### Installation
In order to run this code, you must install **SCIPOptSuite 9.1.0** from [https://scipopt.org/download/release/scipoptsuite-9.1.0.tgz](https://scipopt.org/download/release/scipoptsuite-9.1.0.tgz).

### Build and Run
```shell
# Enter the SCIP root directory
cd scipoptsuite-9.1.0
# Download this repository
# Move the CCP folder into SCIP examples
mv CCP scip/examples
# Add the example in the examples CMakeLists
cd scip/examples
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
# Below are the commands to run different settings (using CCMPP instances and epsilon=0.1)
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
```

### Detailed computational results 
Detailed statistics of instance-wise computational results can be found in the online supplement available at [https://drive.google.com/file/d/1hZnv0jgoFUjyIS7Fwyo6bA_6p1tu9yil/view?usp=share_link](https://drive.google.com/file/d/1hZnv0jgoFUjyIS7Fwyo6bA_6p1tu9yil/view?usp=share_link).