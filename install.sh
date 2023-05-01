#!/bin/bash

cwd=$(pwd)

function run_command {
    local cmd="$1"
    local msg="$2"
    # if [ ${#msg} -gt 0 ]; then
    #     echo -e "\e[32m${msg}...\e[0m"
    # fi
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

echo "Installing package dependencies..."

run_command "sudo apt update" ""
run_command "sudo apt install -y -V build-essential gcc g++ make cmake ndctl" "installed package dependencies"

echo "Installing Apache Arrow..."

run_command "sudo apt install -y -V ca-certificates lsb-release wget" ""
run_command "rm -f apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" ""
run_command "wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" ""
run_command "sudo apt install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" "installed Apache Arrow package"
run_command "sudo apt update" ""
run_command "sudo apt install -y -V libarrow-dev" "installed libarrow-dev"
run_command "sudo apt install -y -V libarrow-dataset-dev" "installed libarrow-dataset-dev"
run_command "sudo apt install -y -V libparquet-dev" "installed libparquet-dev"

echo "Building pmemtrace"

run_command "rm -rf pmemtrace/build" ""
run_command "mkdir pmemtrace/build" ""
cd pmemtrace/build
run_command "cmake .." ""
run_command "sudo make install -j4" "built pmemtrace"

cd $cwd

echo "Building pmemreplay"

run_command "rm -rf pmemreplay/build" ""
run_command "mkdir pmemreplay/build" ""
cd pmemreplay/build
run_command "cmake .." ""
run_command "make -j4" "built pmemreplay"

cd $cwd

echo "Cleaning..."
run_command "rm -f apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb" ""

echo "Done!"
