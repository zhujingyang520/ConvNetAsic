# ConvNetAsic
This is an on-going project to develop the ASIC generator for a specific neural
network architecture provided by the Caffe Prototxt file.

## Installation
The dependency for the current project includes the SystemC
[http://accellera.org/downloads/standards/systemc] and the Google Protocol
Buffers [https://developers.google.com/protocol-buffers/].

The Makfile is provided for GCC compilation on Linux. The variable $SYSTEMC_ROOT
shoud be changed accordingly based on the installation location of SystemC on
your system. There are 2 more vairables $DATA_PATH and $DEBUG, which can be
turned off to compile a faster version, but without the data path information in
the system.

## Usage
After compilation, an executable file `main` will be generated in the current
path. You need provide a configuration file to define the basic configuration of
the system, e.g. bit width, CNN prototxt file, etc. A sampled configuration is
provided in the main directory, named "config.prototxt".

The program can be invoked by typing the following command in the console:
>> ./main -c config.prototxt

During execution, the system will first analyze the Caffe network and do the
basic analysis on the operation complexity on each layer. The ASIC system will
be generated based on a naive algorithm (should be improved later). A thorough
cycle accurate simulation will be conducted to obtain the performance metric. By
far, the system will bypass the trivial layers, such as ReLU, normalization,
etc. It can support for a wide range of neural network architectures including
inception-v4 except ResNet.

A trace file (\*.vcd)  with the specified name will be generated during the
simulation, which can be viewed through common waveform viewer, such as Modelsim
and VCS.

At the end of simulation, a detailed throughput and area breakdown the generated
system will be exported.

## TODO list
- Analysis of throughput, buffer, area, power relationship.
- Optimization of hardware resource allocations.
