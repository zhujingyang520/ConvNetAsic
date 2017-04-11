#######################################
# Sampled Makefile for the simulation
# (TODO) modify the path
#######################################
VCS_BIN := vcs -full64
VCS_OPTION := +v2k -debug_all
MACRO := BEHAV_SIM
FLIST := 
SEARCH_PATH := 
LIB_PATH := 
TESTBENCH_RTL := Inception_v4_top.v testbench.v
TIME_SCALE := 1ns/10ps
run:
	$(VCS_BIN) $(VCS_OPTION) -top Inception_v4_top -f $(FLIST) $(TESTBENCH_RTL) +define+$(MACRO) -timescale=$(TIME_SCALE) +incdir+$(SEARCH_PATH) -v $(LIB_PATH)/rom_full_ones.v -v $(LIB_PATH)/rom_full_zeros.v

clean:
	rm -fr csrc simv simv.daidir
	rm -fr DVE* ucli* *.vpd
