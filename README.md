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
└── scripts/            # shell and awk scripts used to generate the tables in the paper
```

### 📦 Dependencies & Installation

To run this code on a ```Linux``` system and reproduce the results reported in the paper, you must install **SCIPOptSuite 9.1.0**.

**1. Download Software**: Get the source code from [scipoptsuite-9.1.0.tgz](https://scipopt.org/download/release/scipoptsuite-9.1.0.tgz).

**2. Build SCIP**: Compile **SCIP** using **CMake**. Detailed instructions can be found in the [SCIP Installation Guide](https://www.scipopt.org/doc-10.0.0/html/md_INSTALL.php).

**3. Configure Environment**: Add **SCIP** to your system path by appending the following lines to your shell configuration file (e.g., ```~/.bashrc```):
   ```bash
   # replace <path_to_scip> with your actual installation path
   export SCIP_ROOT=<path_to_scip>/scipoptsuite-9.1.0
   export PATH="${SCIP_ROOT}/bin:${PATH}"
   export C_INCLUDE_PATH="${SCIP_ROOT}/include:${C_INCLUDE_PATH}"
   export CPLUS_INCLUDE_PATH="${SCIP_ROOT}/include:${CPLUS_INCLUDE_PATH}"
   export LIBRARY_PATH="${SCIP_ROOT}/lib:${LIBRARY_PATH}"
   export LD_LIBRARY_PATH="${SCIP_ROOT}/lib:${LD_LIBRARY_PATH}"
   ```
and reload your shell configuration using ```source ~/.bashrc``` command.

**4. Verify Installation**: Confirm the installation by running the following command:
   ```bash
   scip --version
   ```
   If the installation is successful, you should see the version information of SCIP.

### 🛠️ Configuration & Build

Follow **two steps** to configure and build the project:

**1.** Clone this repository:
   ```bash
   git clone https://github.com/lvwei-xtu/SCIP-CCP.git
   ```

**2.** Create a new subdirectory, jump to the new directory and configure the project using ```cmake```. For instance, type
   ```bash
   mkdir build; cd build; cmake .. -DCMAKE_C_FLAGS="-std=c99"
   ```
and build the executable using the ```make``` command. For more details, refer to the [SCIP documentation](https://www.scipopt.org/doc-10.0.0/html/START.php).

### 🏃 Usage

- In the root directory, run the following commands to test the CCMPP instance ```10-1000-0.ccmpp``` with $\epsilon=0.1$ under different settings:

```bash
# setting BASE
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BASE1.set
# setting BASE+DI
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BASE+DI1.set
# setting BASE+sDI
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BASE+sDI1.set
# setting BASE+DB
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BASE+DB1.set
# setting BASE+DB+OPF
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BASE+DB+OPF1.set
# setting BASE+DB+EOPF
./build/ccp -f data/CCMPPData/10-1000-0.ccmpp -s settings/CCMPPSetting/BASE+DB+EOPF1.set
```

### 🔁 Replicating
To reproduce the computational results presented in the paper, please run:
```bash
awk -f script/CCP.awk -v root=results -v table=all > PaperTables.txt
```
This command writes the LaTeX source code for all tables in the paper to the `PaperTables.txt`. You can then copy the table source code from `PaperTables.txt` into the [Springer Nature's LaTeX template](https://www.springernature.com/gp/authors/campaigns/latex-author-support) for further editing.

### 📊 Detailed computational results
Detailed statistics of instance-wise computational results can be found in the online supplement available at [My Google Drive](https://drive.google.com/file/d/1hZnv0jgoFUjyIS7Fwyo6bA_6p1tu9yil/view?usp=share_link)
