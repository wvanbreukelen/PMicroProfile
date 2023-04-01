#!/bin/bash

cwd=$(pwd)

# WIP: do kernel install...

echo "Installing Apache Arrow..."

sudo apt update
sudo apt install -y -V ca-certificates lsb-release wget
rm -f apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt install -y -V ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb
sudo apt update
sudo apt install -y -V libarrow-dev # For C++
sudo apt install -y -V libarrow-dataset-dev # For Apache Arrow Dataset C++
sudo apt install -y -V libparquet-dev # For Apache Parquet C++

echo "Building pmemtrace"

rm -rf pmemtrace/build
mkdir pmemtrace/build
cd pmemtrace/build
cmake ..
make -j4

cd $cwd

echo "Building pmemreplay"

rm -rf pmemreplay/build
mkdir pmemreplay/build
cd pmemreplay/build
cmake ..
make -j4

echo "Done!"