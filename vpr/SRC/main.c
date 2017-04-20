/**
 VPR is a CAD tool used to conduct FPGA architecture exploration.  It takes, as input, a technology-mapped netlist and a description of the FPGA architecture being investigated.  
 VPR then generates a packed, placed, and routed FPGA (in .net, .place, and .route files respectively) that implements the input netlist.
 
 This file is where VPR starts execution.

 Key files in VPR:
 1.  libarchfpga/physical_types.h - Data structures that define the properties of the FPGA architecture
 2.  vpr_types.h - Very major file that defines the core data structures used in VPR.  This includes detailed architecture information, user netlist data structures, and data structures that describe the mapping between those two.
 3.  globals.h - Defines the global variables used by VPR.
 */

#include <cstdio>
#include <cstring>
#include <ctime>
using namespace std;

#include "vtr_error.h"
#include "vtr_memory.h"
#include "vtr_log.h"

#include "vpr_error.h"
#include "vpr_api.h"
#include "path_delay.h" /* for timing_analysis_runtime */
#include "SetupPentLine.h"
#include "globals.h"
/*
 * Exit codes to signal success/failure to scripts
 * calling vpr
 */
constexpr int SUCCESS_EXIT_CODE = 0; //Everything OK
constexpr int ERROR_EXIT_CODE = 1; //Something went wrong internally
constexpr int UNIMPLEMENTABLE_EXIT_CODE = 2; //Could not implement (e.g. unroutable)

#define CIRC_NUM 30
#define DE 1
#define B 1

bool ExitCriterion(bool pass, int freeze_count, float changeRate){
	if(pass)
		return true;
	if(freeze_count>10)
		return false;
	if(changeRate<0.01)
		return false;
}

bool InnerLoopCriterion(int changes, int trial){
	if(changes>20 || trial >100)
		return false;
}
void read_file_name(char **file_names){
	file_names= (char**)vtr::malloc(30 * sizeof(char *));
	FILE* fpt = fopen("benchmarks.txt","rb");
	for(int line=0;line<30;line++){
		file_names[line] = (char *)vtr::malloc(256*sizeof(char));
		fgets(file_names[line], 256, fpt);
	}
}

void init_seg_rates(int *seg_rates, float *PentFreq){

}

void newSegorSB(){

}

float EvaluateThisResult(float* crit_delay, float* area){
	float crit_delay_sum=0, area_sum=0;
	for(int i;i<CIRC_NUM;i++){
		crit_delay_sum+=crit_delay[i];
		area_sum+=area[i];
	}
	return  crit_delay_sum*area_sum;

}

bool acceptOrNot(){

}
/**
 * VPR program
 * Generate FPGA architecture given architecture description
 * Pack, place, and route circuit into FPGA architecture
 * Electrical timing analysis on results
 *
 * Overall steps
 * 1.  Initialization
 * 2.  Pack
 * 3.  Place-and-route and timing analysis
 * 4.  Clean up
 */
int main(int argc, const char **argv) {
	t_options Options;
	t_arch Arch;
	t_vpr_setup vpr_setup;
	clock_t entire_flow_begin,entire_flow_end;

	char** file_names;
	int seg_rates[6];
	bool pass;
	int freeze_count=0;
	int changes=0, trial=0;
	float C_star=0,C=0,dC=0;
	float crit_delay[CIRC_NUM];
	float area[CIRC_NUM];
	float T=100;
	//parameters initialization
	read_file_name(file_names);
	init_seg_rates(seg_rates, PentTypeFreq);

	while(ExitCriterion(pass, freeze_count, (float)changes/(float)trial)) {

		while(InnerLoopCriterion(changes, trial)) {

			newSegorSB(seg_rates, PentTypeFreq, dC>0, T);//change the Seg or SB

			entire_flow_begin = clock();


			//main loop for every benchmarks
			for(int bmks=0;bmks<30;bmks++) {
				//change file name

				try {

					/* Read options, architecture, and circuit netlist */
					vpr_init(argc, argv, &Options, &vpr_setup, &Arch, file_names[bmks]);

					/* If the user requests packing, do packing */
					if (vpr_setup.PackerOpts.doPacking) {
						vpr_pack(vpr_setup, Arch);
					}

					if (vpr_setup.PlacerOpts.doPlacement || vpr_setup.RouterOpts.doRouting) {
						vpr_init_pre_place_and_route(vpr_setup, Arch);
						bool place_route_succeeded = vpr_place_and_route(&vpr_setup, Arch);
						//hook key data
						crit_delay[bmks]=crit_delay_hook;
						area[bmks]=area_hook;
						/* Signal implementation failure */
						if (!place_route_succeeded) {
							return UNIMPLEMENTABLE_EXIT_CODE;
						}
					}

					if (vpr_setup.PowerOpts.do_power) {
						vpr_power_estimation(vpr_setup, Arch);
					}

					entire_flow_end = clock();

					vtr::printf_info("Timing analysis took %g seconds.\n",
									 float(timing_analysis_runtime) / CLOCKS_PER_SEC);
					vtr::printf_info("The entire flow of VPR took %g seconds.\n",
									 (float) (entire_flow_end - entire_flow_begin) / CLOCKS_PER_SEC);

					/* free data structures */
					vpr_free_all(Arch, Options, vpr_setup);

				} catch (VprError &vpr_error) {
					vpr_print_error(vpr_error);
					/* Signal error to scripts */
					return ERROR_EXIT_CODE;
				} catch (vtr::VtrError &vtr_error) {
					vtr::printf_error(__FILE__, __LINE__, "%s:%d %s\n", vtr_error.filename_c_str(), vtr_error.line(),
									  vtr_error.what());
					/* Signal error to scripts */
					return ERROR_EXIT_CODE;
				}
				//get the data into array of 30, the data is critical_path_delay, lb_area, rt_area

				//free all data except the data
			}
			//calc the evaluation
			C_star=EvaluateThisResult(crit_delay, area);
			dC=C_star-C;
			//trial
			trial++;
			//ac or not  "SA"
			acceptOrNot(dC, T, B, &changes);
			//changes

			//clear the 30 data
		}//InnerLoop
		//Temperature update
		T*=0.95;
		if(dC>DE){
			freeze_count = 0;
			pass = true;
		}else {
			freeze_count++;
			pass = false;
		}

	}//outer loop
	/* Signal success to scripts */
	return SUCCESS_EXIT_CODE;
}




