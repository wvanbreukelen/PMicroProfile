#!/bin/bash

cwd=$(readlink -f .)
build_llvm=false

function run_command {
    local cmd="$1"
    local msg="$2"

    if $cmd > /dev/null 2>&1; then
        if [ ${#msg} -gt 0 ]; then
            echo -e "\e[32m[✔] Successfully ${msg}!\e[0m"
        fi
    else
        echo -e "\e[31m${msg} failed! Printing output:\e[0m"
        $cmd
        exit 1
    fi
}

# Check if --build-llvm argument is passed
if [ "$1" == "--build-llvm" ]; then
    build_llvm=true
    echo "Building LLVM enabled."
else
    echo "--> Will not build LLVM by default, may be enabled by passing --build-llvm flag (compiling may take long time)."
fi


echo "--> Installing package dependencies..."

run_command "sudo apt update" ""
run_command "sudo apt install -y -V wget build-essential gcc g++ make cmake ndctl" "installed package dependencies"

echo "--> Installing Apache Arrow..."

run_command "sudo apt install -y -V ca-certificates lsb-release wget" ""
run_command "rm -f apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" ""
run_command "wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" ""
run_command "sudo apt install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" "installed Apache Arrow package"
run_command "sudo apt update" ""
run_command "sudo apt install -y -V libarrow-dev" "installed libarrow-dev"
run_command "sudo apt install -y -V libarrow-dataset-dev" "installed libarrow-dataset-dev"
run_command "sudo apt install -y -V libparquet-dev" "installed libparquet-dev"l

cd $cwd

if [ "$build_llvm" = true ]; then
    echo "--> Building LLVM..."
    run_command "sudo apt install -y ninja-build" ""
    run_command "true | git clone --depth 1 --branch llvmorg-16.0.3 https://github.com/llvm/llvm-project.git"
    run_command "cp llvm_patch/Bye.cpp llvm-project/llvm/examples/Bye/"

    cd $cwd/llvm-project
    run_command "cmake -S llvm -B build -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS='clang'" ""
    run_command "cmake --build build -j24" ""
    run_command "sudo cmake --build build --target install" ""
else
    echo "--> Skipping LLVM build! Enable by setting --build-llvm flag."
fi

cd $cwd

echo "--> Building pmemtrace"

run_command "rm -rf pmemtrace/build" ""
run_command "mkdir pmemtrace/build" ""
cd pmemtrace/build
run_command "cmake .." ""
run_command "sudo make install -j4" "built pmemtrace"

cd $cwd

echo "--> Building pmemanalyze"

run_command "rm -rf pmemanalyze/build" ""
run_command "mkdir pmemanalyze/build" ""
cd pmemanalyze/build
run_command "cmake .." ""
run_command "sudo make install -j4" "built pmemanalyze"

cd $cwd

echo "--> Cleaning..."
run_command "rm -f apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" ""

echo "Done!"
