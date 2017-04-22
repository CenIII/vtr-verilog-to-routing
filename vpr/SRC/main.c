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

#include "math.h"
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

int str_to_number(char* str){
    char* p=str;
    int result=0;
    while((*p)!='\0'){
        result = result*10 + (*p)-'0';
        p++;
    }
    return result;
}

void read_Pent_config(){
    char str[PENTYPE][16];
    FILE* fpt = fopen("/home/ivan/Desktop/2D_config.txt","rb");
    for(int line=0;line<PENTYPE;line++){
        fgets(str[line], 16, fpt);
        int i=0;
        while(str[line][i]!='\n')
            i++;
        str[line][i]='\0';
        PentTypeFreq[line] = str_to_number(str[line]);
    }
    fclose(fpt);
}

void print_tp_result_to_file(char* CircName){
    char* C_pt=CircName;
    while((*C_pt)!='\0')
        C_pt++;
    while((*C_pt)!='\/')
        C_pt--;
    C_pt++;

    char* filepath;
    char ini_path[50] = {"/home/ivan/Desktop/2D_tp_result/"};
    //filepath = (char* )vtr::malloc(256*sizeof(char));
    filepath = strcat(ini_path, C_pt);
    FILE* fpt = fopen(filepath,"wb");
    fprintf(fpt,"%g\n%g\n",1e9 * crit_delay_hook,area_hook);
    fflush(fpt);
    fclose(fpt);
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
    entire_flow_begin = clock();
    try {

        //todo:读取Ｐｅｎｔ配置
        read_Pent_config();
        //todo:读取ＳＢ配置

        /* Read options, architecture, and circuit netlist */
        vpr_init(argc, argv, &Options, &vpr_setup, &Arch);

        /* If the user requests packing, do packing */
        if (vpr_setup.PackerOpts.doPacking) {
            vpr_pack(vpr_setup, Arch);
        }

        if (vpr_setup.PlacerOpts.doPlacement || vpr_setup.RouterOpts.doRouting) {
            vpr_init_pre_place_and_route(vpr_setup, Arch);
            bool place_route_succeeded = vpr_place_and_route(&vpr_setup, Arch);
            //hook key data
            //get the data into array of 30, the data is critical_path_delay, lb_area, rt_area
            //todo:打印ｈｏｏｋ　ｄｅｌａｙ和ａｒｅａ
            print_tp_result_to_file(Options.CircuitName);
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

	/* Signal success to scripts */
	return SUCCESS_EXIT_CODE;
}




