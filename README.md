# Source code, instances, and detailed computational results of the paper: 

### Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side

### Wei Lv, Wei-Kun Chen, Yu-Hong Dai, and Xiao-Jiao Tong

---

### 📖 Description 

This repository contains the source code, instances, and detailed results of the computational experiments conducted for the paper **"Exploiting Overlap Information in Chance-constrained Program with Random Right-hand Side"**. 

#### [![arXiv](https://img.shields.io/badge/arXiv-2406.10472-b31b1b.svg)](https://arxiv.org/abs/2406.10472)

### 📂 Repository Structure

```text
SCIP-CCP
├── CMakeLists.txt      # CMake build configuration script
├── src/                # Source code for implementing the proposed approaches
├── data/               # Benchmark instances (CCRP, CCMPP, CCLS)
├── settings/           # Configuration files with different settings
└── results/            # Logfiles produced by the computational experiments
```

### 📦 Dependencies & Installation

To run this code and reproduce the results reported in the paper, you must install **SCIPOptSuite 9.1.0**.

1.  **Download**: Get the source code from [scipoptsuite-9.1.0.tgz](https://scipopt.org/download/release/scipoptsuite-9.1.0.tgz).
2.  **Build**: Compile SCIP using CMake. Detailed instructions can be found in the [SCIP Installation Guide](https://www.scipopt.org/doc-10.0.0/html/md_INSTALL.php).
3.  **Configure Environment**: Add SCIP to your system path:
      ```bash
      # replace <path_to_scip> with your actual installation path
      export SCIP_ROOT=<path_to_scip>/scipoptsuite-9.1.0
      export PATH="$SCIP_ROOT/bin:$PATH"
      ```

### 🛠️ Configuration & Build

Follow two steps to configure and build the project:

1. Clone the source code
   ```bash
   git clone https://github.com/lvwei-xtu/SCIP-CCP.git
   
2. Create a new build subdirectory, jump to the new directory and configure the project using ```cmake```. For instance, type
   ```bash
   mkdir build; cd build; cmake .. 
   ```
and build the executable using the ```make``` command. For more details, refer to the [SCIP documentation](https://www.scipopt.org/doc-10.0.0/html/START.php).

### 🏃 Usage

- Run experiments on **CCMPP** instances (with $\epsilon=0.1$) in root directory using the following commands:

```bash
# setting B&C+MIX
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BnC-MIX1.set
# setting B&C+MIX+DI
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BnC-MIX-DI1.set
# setting B&C+MIX+sDI
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BnC-MIX-sDI1.set
# setting DB
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/DB1.set
# setting DB+OPF
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/DB-OPF1.set
# setting DB+EOPF
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/DB-EOPF1.set
```

### 📊 Detailed computational results
Detailed statistics of instance-wise computational results can be found in the online supplement available at [Google Drive](https://drive.google.com/file/d/1hZnv0jgoFUjyIS7Fwyo6bA_6p1tu9yil/view?usp=share_link)
