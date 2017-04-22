/**
 General API for VPR
 Other software tools should generally call just the functions defined here
 For advanced/power users, you can call functions defined elsewhere in VPR or modify the data structures directly at your discretion but be aware that doing so can break the correctness of VPR

 Author: Jason Luu
 June 21, 2012
 */

#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
using namespace std;


#include "vtr_assert.h"
#include "vtr_list.h"
#include "vtr_matrix.h"
#include "vtr_math.h"
#include "vtr_log.h"
#include "vtr_version.h"

#include "vpr_types.h"
#include "vpr_utils.h"
#include "globals.h"
#include "graphics.h"
#include "read_netlist.h"
#include "check_netlist.h"
#include "print_netlist.h"
#include "read_blif.h"
#include "draw.h"
#include "place_and_route.h"
#include "pack.h"
#include "SetupGrid.h"
#include "stats.h"
#include "path_delay.h"
#include "OptionTokens.h"
#include "ReadOptions.h"
#include "read_xml_arch_file.h"
#include "SetupPentLine.h"
#include "SetupVPR.h"
#include "ShowSetup.h"
#include "CheckArch.h"
#include "CheckSetup.h"
#include "CheckOptions.h"
#include "rr_graph.h"
#include "pb_type_graph.h"
#include "ReadOptions.h"
#include "route_common.h"
#include "timing_place_lookup.h"
#include "route_export.h"
#include "vpr_api.h"
#include "read_sdc.h"
#include "power.h"
#include "pack_types.h"
#include "lb_type_rr_graph.h"
#include "output_blif.h"

#include "log.h"

/* Local subroutines */
static void free_pb_type(t_pb_type *pb_type);
static void free_complex_block_types(void);

static void free_arch(t_arch* Arch);
static void free_options(const t_options *options);
static void free_circuit(void);

static void get_intercluster_switch_fanin_estimates(const t_vpr_setup& vpr_setup, const int wire_segment_length,
			int *opin_switch_fanin, int *wire_switch_fanin, int *ipin_switch_fanin);

/* For resync of clustered netlist to the post-route solution. This function adds local nets to cluster */
static void resync_pb_graph_nodes_in_pb(t_pb_graph_node *pb_graph_node, t_pb *pb);

void vpr_print_args(int argc, const char** argv);

/* Local subroutines end */

/* Display general VPR information */
void vpr_print_title(void) {

	vtr::printf_info("\n");
	vtr::printf_info("VPR FPGA Placement and Routing.\n");
	vtr::printf_info("Version: %s\n", vtr::VERSION_SHORT);
	vtr::printf_info("Revision: %s\n", vtr::VCS_REVISION);
	vtr::printf_info("Compiled: %s\n", vtr::BUILD_TIMESTAMP);
	vtr::printf_info("University of Toronto\n");
	vtr::printf_info("vtr-users@googlegroups.com\n");
	vtr::printf_info("This is free open source code under MIT license.\n");
	vtr::printf_info("\n");

}

/* Display help screen */
void vpr_print_usage(void) {

	vtr::printf_info("Usage:  vpr fpga_architecture.xml circuit_name [Options ...]\n");
	vtr::printf_info("\n");
	vtr::printf_info("General Options:  [--nodisp] [--auto <int>] [--pack]\n");
	vtr::printf_info("\t[--place] [--route] [--timing_analyze_only_with_net_delay <float>]\n");
	vtr::printf_info("\t[--fast] [--full_stats] [--timing_analysis on | off] [--outfile_prefix <string>]\n");
	vtr::printf_info("\t[--blif_file <string>] [--net_file <string>] [--place_file <string>]\n");
	vtr::printf_info("\t[--route_file <string>] [--sdc_file <string>] [--echo_file on | off]\n");
	vtr::printf_info("\n");
	vtr::printf_info("Packer Options:\n");
	/* vtr::printf_info("\t[-global_clocks on | off]\n"); */
	/* vtr::printf_info("\t[-hill_climbing on | off]\n"); */
	/* vtr::printf_info("\t[-sweep_hanging_nets_and_inputs on | off]\n"); */
	vtr::printf_info("\t[--timing_driven_clustering on | off]\n");
	vtr::printf_info("\t[--cluster_seed_type blend|timing|max_inputs] [--alpha_clustering <float>] [--beta_clustering <float>]\n");
	/* vtr::printf_info("\t[-recompute_timing_after <int>] [-cluster_block_delay <float>]\n"); */
	vtr::printf_info("\t[--allow_unrelated_clustering on | off]\n");
	/* vtr::printf_info("\t[-allow_early_exit on | off]\n"); */
	/* vtr::printf_info("\t[-intra_cluster_net_delay <float>] \n"); */
	/* vtr::printf_info("\t[-inter_cluster_net_delay <float>] \n"); */
	vtr::printf_info("\t[--connection_driven_clustering on | off] \n");
	vtr::printf_info("\n");
	vtr::printf_info("Placer Options:\n");
	vtr::printf_info("\t[--place_algorithm bounding_box | path_timing_driven]\n");
	vtr::printf_info("\t[--init_t <float>] [--exit_t <float>]\n");
	vtr::printf_info("\t[--alpha_t <float>] [--inner_num <float>] [--seed <int>]\n");
	vtr::printf_info("\t[--place_cost_exp <float>]\n");
	vtr::printf_info("\t[--place_chan_width <int>] \n");
	vtr::printf_info("\t[--fix_pins random | <file.pads>]\n");
	vtr::printf_info("\t[--enable_timing_computations on | off]\n");
	vtr::printf_info("\n");
	vtr::printf_info("Placement Options Valid Only for Timing-Driven Placement:\n");
	vtr::printf_info("\t[--timing_tradeoff <float>]\n");
	vtr::printf_info("\t[--recompute_crit_iter <int>]\n");
	vtr::printf_info("\t[--inner_loop_recompute_divider <int>]\n");
	vtr::printf_info("\t[--td_place_exp_first <float>]\n");
	vtr::printf_info("\t[--td_place_exp_last <float>]\n");
	vtr::printf_info("\n");
	vtr::printf_info("Router Options:  [-max_router_iterations <int>] [-bb_factor <int>]\n");
	vtr::printf_info("\t[--initial_pres_fac <float>] [--pres_fac_mult <float>]\n");
	vtr::printf_info("\t[--acc_fac <float>] [--first_iter_pres_fac <float>]\n");
	vtr::printf_info("\t[--bend_cost <float>] [--route_type global | detailed]\n");
	vtr::printf_info("\t[--min_incremental_reroute_fanout <int>]\n");
	vtr::printf_info("\t[--verify_binary_search] [--route_chan_width <int>] [--route_chan_trim on | off]\n");
	vtr::printf_info("\t[--router_algorithm breadth_first | timing_driven]\n");
	vtr::printf_info("\t[--base_cost_type delay_normalized | demand_only]\n");
	vtr::printf_info("\n");
	vtr::printf_info("Routing options valid only for timing-driven routing:\n");
	vtr::printf_info("\t[--astar_fac <float>] [--max_criticality <float>]\n");
	vtr::printf_info("\t[--criticality_exp <float>]\n");
	vtr::printf_info("\t[--routing_failure_predictor safe | aggressive | off]\n");
	vtr::printf_info("\n");
	vtr::printf_info("VPR Developer Options:\n");
	vtr::printf_info("\t[--gen_netlist_as_blif]\n");
	vtr::printf_info("\n");
}

void vpr_print_args(int argc, const char** argv) {
    vtr::printf_info("VPR was run with the following command-line:\n");
    for(int i = 0; i < argc; i++) {
        if(i != 0) {
            vtr::printf_info(" ");
        }
        vtr::printf_info("%s", argv[i]);
    }
    vtr::printf_info("\n\n");
}

/* Initialize VPR
 1. Read Options
 2. Read Arch
 3. Read Circuit
 4. Sanity check all three
 */
void vpr_init(const int argc, const char **argv,
		t_options *options,
		t_vpr_setup *vpr_setup,
		t_arch *arch) {

	log_set_output_file("vpr_stdout.log");

	/* Print title message */
	vpr_print_title();

    //Print out the arguments passed to VPR.
    //This provides a reference in the log file to exactly
    //how VPR was run, aiding in re-producibility
    vpr_print_args(argc, argv);

	if (argc >= 3) {

		memset(options, 0, sizeof(t_options));
		memset(vpr_setup, 0, sizeof(t_vpr_setup));
		memset(arch, 0, sizeof(t_arch));

		/* Read in user options */
		ReadOptions(argc, argv, options);

		/* Timing option priorities */
		vpr_setup->TimingEnabled = IsTimingEnabled(options);

        /* Verify the rest of the options */
		CheckOptions(*options, vpr_setup->TimingEnabled);

        vtr::printf_info("\n");
        vtr::printf_info("Architecture file: %s\n", options->ArchFile);
        vtr::printf_info("Circuit name: %s.blif\n", options->CircuitName);
        vtr::printf_info("\n");

		/* Determine whether echo is on or off */
		setEchoEnabled(IsEchoEnabled(options));
		SetPostSynthesisOption(IsPostSynthesisEnabled(options));
		vpr_setup->constant_net_delay = options->constant_net_delay;
		vpr_setup->gen_netlist_as_blif = (options->Count[OT_GEN_NELIST_AS_BLIF] > 0);

        /*TODO: fill the function*/
		SetupPentLine();

		/* Read in arch and circuit */
		SetupVPR(options, vpr_setup->TimingEnabled, true, &vpr_setup->FileNameOpts,
				arch, &vpr_setup->user_models,
				&vpr_setup->library_models, &vpr_setup->PackerOpts,
				&vpr_setup->PlacerOpts, &vpr_setup->AnnealSched,
				&vpr_setup->RouterOpts, &vpr_setup->RoutingArch,
				&vpr_setup->PackerRRGraph, &vpr_setup->Segments,
				&vpr_setup->Timing, &vpr_setup->ShowGraphics,
				&vpr_setup->GraphPause, &vpr_setup->PowerOpts);

		/* Check inputs are reasonable */
		CheckArch(*arch);

		/* Verify settings don't conflict or otherwise not make sense */
		CheckSetup(vpr_setup->PlacerOpts,
				vpr_setup->RouterOpts,
				vpr_setup->RoutingArch, vpr_setup->Segments, vpr_setup->Timing,
				arch->Chans);

		/* flush any messages to user still in stdout that hasn't gotten displayed */
		fflush(stdout);

		/* Read blif file and sweep unused components */
		read_and_process_blif(vpr_setup->PackerOpts.blif_file_name,
				vpr_setup->PackerOpts.sweep_hanging_nets_and_inputs,
				vpr_setup->PackerOpts.absorb_buffer_luts,
				vpr_setup->user_models, vpr_setup->library_models,
				vpr_setup->PowerOpts.do_power, vpr_setup->FileNameOpts.ActFile);
		fflush(stdout);

		ShowSetup(*options, *vpr_setup);


	} else {
		/* Print usage message if no args */
		vpr_print_usage();
		vtr::printf_error(__FILE__, __LINE__,
			"Missing arguments, see above and try again!\n");
		exit(1);
	}
}

/*
 * Sets globals: nx, ny
 * Allocs globals: chan_width_x, chan_width_y, grid
 * Depends on num_clbs, pins_per_clb */
void vpr_init_pre_place_and_route(const t_vpr_setup& vpr_setup, const t_arch& Arch) {

	/* Read in netlist file for placement and routing */
	if (vpr_setup.FileNameOpts.NetFile) {
		read_netlist(vpr_setup.FileNameOpts.NetFile, &Arch,
				&num_blocks, &block, &num_nets, &clb_net);

		/* This is done so that all blocks have subblocks and can be treated the same */
		check_netlist();

		if(vpr_setup.gen_netlist_as_blif) {
			char *name = (char*)vtr::malloc((strlen(vpr_setup.FileNameOpts.CircuitName) + 16) * sizeof(char));
			sprintf(name, "%s.preplace.blif", vpr_setup.FileNameOpts.CircuitName);
			output_blif(&Arch, block, num_blocks, name);
			free(name);
		}
	}

	/* Output the current settings to console. */
	printClusteredNetlistStats();

    int current = vtr::nint((float)sqrt((float)num_blocks)); /* current is the value of the smaller side of the FPGA */
    int low = 1;
    int high = -1;

    int *num_instances_type = (int*) vtr::calloc(num_types, sizeof(int));
    int *num_blocks_type = (int*) vtr::calloc(num_types, sizeof(int));

    for (int i = 0; i < num_blocks; ++i) {
        num_blocks_type[block[i].type->index]++;
    }

    if (Arch.clb_grid.IsAuto) {

        /* Auto-size FPGA, perform a binary search */
        while (high == -1 || low < high) {

            /* Generate grid */
            if (Arch.clb_grid.Aspect >= 1.0) {
                ny = current;
                nx = vtr::nint(current * Arch.clb_grid.Aspect);
            } else {
                nx = current;
                ny = vtr::nint(current / Arch.clb_grid.Aspect);
            }
#if DEBUG
            vtr::printf_info("Auto-sizing FPGA at x = %d y = %d\n", nx, ny);
#endif
            alloc_and_load_grid(num_instances_type);
            freeGrid();

            /* Test if netlist fits in grid */
            bool fit = true;
            for (int i = 0; i < num_types; ++i) {
                if (num_blocks_type[i] > num_instances_type[i]) {
                    fit = false;
                    break;
                }
            }

            /* get next value */
            if (!fit) {
                /* increase size of max */
                if (high == -1) {
                    current = current * 2;
                    if (current > MAX_SHORT) {
                        vtr::printf_error(__FILE__, __LINE__,
                                "FPGA required is too large for current architecture settings.\n");
                        exit(1);
                    }
                } else {
                    if (low == current)
                        current++;
                    low = current;
                    current = low + ((high - low) / 2);
                }
            } else {
                high = current;
                current = low + ((high - low) / 2);
            }
        }

        /* Generate grid */
        if (Arch.clb_grid.Aspect >= 1.0) {
            ny = current;
            nx = vtr::nint(current * Arch.clb_grid.Aspect);
        } else {
            nx = current;
            ny = vtr::nint(current / Arch.clb_grid.Aspect);
        }
        alloc_and_load_grid(num_instances_type);
        vtr::printf_info("FPGA auto-sized to x = %d y = %d\n", nx, ny);
    } else {
        nx = Arch.clb_grid.W;
        ny = Arch.clb_grid.H;
        alloc_and_load_grid(num_instances_type);
    }

    vtr::printf_info("The circuit will be mapped into a %d x %d array of clbs.\n", nx, ny);

    /* Test if netlist fits in grid */
    for (int i = 0; i < num_types; ++i) {
        if (num_blocks_type[i] > num_instances_type[i]) {

            vtr::printf_error(__FILE__, __LINE__,
                    "Not enough physical locations for type %s, "
                    "number of blocks is %d but number of locations is %d.\n",
                    type_descriptors[i].name, num_blocks_type[i],
                    num_instances_type[i]);
            exit(1);
        }
    }

    vtr::printf_info("\n");
    vtr::printf_info("Resource usage...\n");
    for (int i = 0; i < num_types; ++i) {
        vtr::printf_info("\tNetlist      %d\tblocks of type: %s\n",
                num_blocks_type[i], type_descriptors[i].name);
        vtr::printf_info("\tArchitecture %d\tblocks of type: %s\n",
                num_instances_type[i], type_descriptors[i].name);
    }
    vtr::printf_info("\n");
    chan_width.x_max = chan_width.y_max = 0;
    chan_width.x_min = chan_width.y_min = 0;
    chan_width.x_list = (int *) vtr::malloc((ny + 1) * sizeof(int));
    chan_width.y_list = (int *) vtr::malloc((nx + 1) * sizeof(int));

    free(num_blocks_type);
    free(num_instances_type);
}


void vpr_pack(t_vpr_setup& vpr_setup, const t_arch& arch) {
	std::chrono::high_resolution_clock::time_point end, begin;
	begin = std::chrono::high_resolution_clock::now();
	vtr::printf_info("Initialize packing.\n");

	/* If needed, estimate inter-cluster delay. Assume the average routing hop goes out of
	 a block through an opin switch to a length-4 wire, then through a wire switch to another
	 length-4 wire, then through a wire-to-ipin-switch into another block. */
	int wire_segment_length = 4;

	float inter_cluster_delay = UNDEFINED;
	if (vpr_setup.PackerOpts.timing_driven
			&& vpr_setup.PackerOpts.auto_compute_inter_cluster_net_delay) {

		/* We want to determine a reasonable fan-in to the opin, wire, and ipin switches, based
		   on which the intercluster delays can be estimated. The fan-in of a switch influences its
		   delay.

		   The fan-in of the switch depends on the architecture (unidirectional/bidirectional), as
		   well as Fc_in/out and Fs */
		int opin_switch_fanin, wire_switch_fanin, ipin_switch_fanin;
		get_intercluster_switch_fanin_estimates(vpr_setup, wire_segment_length, &opin_switch_fanin,
				&wire_switch_fanin, &ipin_switch_fanin);


		float Tdel_opin_switch, R_opin_switch, Cout_opin_switch;
		float opin_switch_del = get_arch_switch_info(arch.Segments[0].arch_opin_switch, opin_switch_fanin,
				Tdel_opin_switch, R_opin_switch, Cout_opin_switch);

		float Tdel_wire_switch, R_wire_switch, Cout_wire_switch;
		float wire_switch_del = get_arch_switch_info(arch.Segments[0].arch_wire_switch, wire_switch_fanin,
				Tdel_wire_switch, R_wire_switch, Cout_wire_switch);

		float Tdel_wtoi_switch, R_wtoi_switch, Cout_wtoi_switch;
		float wtoi_switch_del = get_arch_switch_info(
				vpr_setup.RoutingArch.wire_to_arch_ipin_switch, ipin_switch_fanin,
				Tdel_wtoi_switch, R_wtoi_switch, Cout_wtoi_switch);

		float Rmetal = arch.Segments[0].Rmetal;
		float Cmetal = arch.Segments[0].Cmetal;

		/* The delay of a wire with its driving switch is the switch delay plus the
		 product of the equivalent resistance and capacitance experienced by the wire. */

		float first_wire_seg_delay = opin_switch_del
				+ (R_opin_switch + Rmetal * (float)wire_segment_length / 2)
						* (Cout_opin_switch + Cmetal * (float)wire_segment_length);
		float second_wire_seg_delay = wire_switch_del
				+ (R_wire_switch + Rmetal * (float)wire_segment_length / 2)
						* (Cout_wire_switch + Cmetal * (float)wire_segment_length);
		inter_cluster_delay = 4
				* (first_wire_seg_delay + second_wire_seg_delay
						+ wtoi_switch_del); /* multiply by 4 to get a more conservative estimate */
	}

	try_pack(&vpr_setup.PackerOpts, &arch, vpr_setup.user_models,
			vpr_setup.library_models, vpr_setup.Timing, inter_cluster_delay, vpr_setup.PackerRRGraph);

	end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin);
	printf("Packing took %g seconds\n", time_span.count());
	fflush(stdout);
}

/* Since the parameters of a switch may change as a function of its fanin,
   to get an estimation of inter-cluster delays we need a reasonable estimation
   of the fan-ins of switches that connect clusters together. These switches are
	1) opin to wire switch
	2) wire to wire switch
	3) wire to ipin switch
   We can estimate the fan-in of these switches based on the Fc_in/Fc_out of
   a logic block, and the switch block Fs value */
static void get_intercluster_switch_fanin_estimates(const t_vpr_setup& vpr_setup, const int wire_segment_length,
			int *opin_switch_fanin, int *wire_switch_fanin, int *ipin_switch_fanin){
	e_directionality directionality;
	int Fs;
	float Fc_in, Fc_out;
	int W = 100;	//W is unknown pre-packing, so *if* we need W here, we will assume a value of 100

	directionality = vpr_setup.RoutingArch.directionality;
	Fs = vpr_setup.RoutingArch.Fs;
	Fc_in = 0, Fc_out = 0;

	/* get Fc_in/out for FILL_TYPE block (i.e. logic blocks) */
	int num_pins = FILL_TYPE->num_pins;
	for (int ipin = 0; ipin < num_pins; ipin++){
		int iclass = FILL_TYPE->pin_class[ipin];
		e_pin_type pin_type = FILL_TYPE->class_inf[iclass].type;
		/* Get Fc of the pin to the first wire segment it connects to. TODO: In the future can
		   iterate over all segments as well if necessary */
		int Fc = FILL_TYPE->Fc[ipin][0];
		bool is_fractional = FILL_TYPE->is_Fc_frac[ipin];
		bool is_full_flex = FILL_TYPE->is_Fc_full_flex[ipin];

		if (pin_type == DRIVER){
			if (is_full_flex){
				/* opin connects to all wire segments in channel */
				Fc = 1.0;
			} else if (!is_fractional) {
				/* convert to fractional representation if necessary */
				Fc /= W;
			}
			if (Fc > Fc_out){
				Fc_out = Fc;
			}
		}
		if (pin_type == RECEIVER){
			if (!is_fractional){
				Fc /= W;
			}
			if (Fc > Fc_in){
				Fc_in = Fc;
			}
		}
	}

	/* Estimates of switch fan-in are done as follows:
	   1) opin to wire switch:
		2 CLBs connect to a channel, each with #opins/4 pins. Each pin has Fc_out*W
		switches, and then we assume the switches are distributed evenly over the W wires.
		In the unidirectional case, all these switches are then crammed down to W/wire_segment_length wires.

			Unidirectional: 2 * #opins_per_side * Fc_out * wire_segment_length
			Bidirectional:  2 * #opins_per_side * Fc_out

	   2) wire to wire switch
		A wire segment in a switchblock connects to Fs other wires. Assuming these connections are evenly
		distributed, each target wire receives Fs connections as well. In the unidirectional case,
		source wires can only connect to W/wire_segment_length wires.

			Unidirectional: Fs * wire_segment_length
			Bidirectional:  Fs

	   3) wire to ipin switch
		An input pin of a CLB simply receives Fc_in connections.

			Unidirectional: Fc_in
			Bidirectional:  Fc_in
	*/


	/* Fan-in to opin/ipin/wire switches depends on whether the architecture is unidirectional/bidirectional */
	(*opin_switch_fanin) = 2 * FILL_TYPE->num_drivers/4 * Fc_out;
	(*wire_switch_fanin) = Fs;
	(*ipin_switch_fanin) = Fc_in;
	if (directionality == UNI_DIRECTIONAL){
		/* adjustments to opin-to-wire and wire-to-wire switch fan-ins */
		(*opin_switch_fanin) *= wire_segment_length;
		(*wire_switch_fanin) *= wire_segment_length;
	} else if (directionality == BI_DIRECTIONAL){
		/* no adjustments need to be made here */
	} else {
		vpr_throw(VPR_ERROR_PACK, __FILE__, __LINE__, "Unrecognized directionality: %d\n", (int)directionality);
	}
}

bool vpr_place_and_route(t_vpr_setup *vpr_setup, const t_arch& arch) {

	/* Startup X graphics */
	init_graphics_state(vpr_setup->ShowGraphics, vpr_setup->GraphPause,
			vpr_setup->RouterOpts.route_type);
	if (vpr_setup->ShowGraphics) {
		init_graphics("VPR:  Versatile Place and Route for FPGAs", WHITE);
		alloc_draw_structs();
	}

	/* Do placement and routing */
	bool success = place_and_route(vpr_setup->PlacerOpts,
			vpr_setup->FileNameOpts.PlaceFile, vpr_setup->FileNameOpts.NetFile,
			vpr_setup->FileNameOpts.ArchFile, vpr_setup->FileNameOpts.RouteFile,
			vpr_setup->AnnealSched, vpr_setup->RouterOpts, &vpr_setup->RoutingArch,
			vpr_setup->Segments, vpr_setup->Timing, arch.Chans,
			arch.Directs, arch.num_directs);
	fflush(stdout);

	/* Close down X Display */
	if (vpr_setup->ShowGraphics)
		close_graphics();
	free_draw_structs();

	return(success);
}

/* Free architecture data structures */
void free_arch(t_arch* Arch) {

	freeGrid();
	free(chan_width.x_list);
	free(chan_width.y_list);

	chan_width.x_list = chan_width.y_list = NULL;
	chan_width.max = chan_width.x_max = chan_width.y_max = chan_width.x_min = chan_width.y_min = 0;

	for (int i = 0; i < Arch->num_switches; ++i) {
		if (Arch->Switches->name != NULL) {
			free(Arch->Switches[i].name);
		}
	}
	delete[] Arch->Switches;
	delete[] g_arch_switch_inf;
	Arch->Switches = NULL;
	g_arch_switch_inf = NULL;
	for (int i = 0; i < Arch->num_segments; ++i) {
		free(Arch->Segments[i].cb);
		Arch->Segments[i].cb = NULL;
		free(Arch->Segments[i].sb);
		Arch->Segments[i].sb = NULL;
		free(Arch->Segments[i].name);
		Arch->Segments[i].name = NULL;
	}
	free(Arch->Segments);
	t_model *model = Arch->models;
	while (model) {
		t_model_ports *input_port = model->inputs;
		while (input_port) {
			t_model_ports *prev_port = input_port;
			input_port = input_port->next;
			free(prev_port->name);
			free(prev_port);
		}
		t_model_ports *output_port = model->outputs;
		while (output_port) {
			t_model_ports *prev_port = output_port;
			output_port = output_port->next;
			free(prev_port->name);
			free(prev_port);
		}
		vtr::t_linked_vptr *vptr = model->pb_types;
		while (vptr) {
			vtr::t_linked_vptr *vptr_prev = vptr;
			vptr = vptr->next;
			free(vptr_prev);
		}
		t_model *prev_model = model;

		model = model->next;
		if (prev_model->instances)
			free(prev_model->instances);
		free(prev_model->name);
		free(prev_model);
	}

	for (int i = 0; i < 4; ++i) {
		vtr::t_linked_vptr *vptr = Arch->model_library[i].pb_types;
		while (vptr) {
			vtr::t_linked_vptr *vptr_prev = vptr;
			vptr = vptr->next;
			free(vptr_prev);
		}
	}

	for (int i = 0; i < Arch->num_directs; ++i) {
		free(Arch->Directs[i].name);
		free(Arch->Directs[i].from_pin);
		free(Arch->Directs[i].to_pin);
	}
	free(Arch->Directs);

	free(Arch->model_library[0].name);
	free(Arch->model_library[0].outputs->name);
	free(Arch->model_library[0].outputs);
	free(Arch->model_library[1].inputs->name);
	free(Arch->model_library[1].inputs);
	free(Arch->model_library[1].name);
	free(Arch->model_library[2].name);
	free(Arch->model_library[2].inputs[0].name);
	free(Arch->model_library[2].inputs[1].name);
	free(Arch->model_library[2].inputs);
	free(Arch->model_library[2].outputs->name);
	free(Arch->model_library[2].outputs);
	free(Arch->model_library[3].name);
	free(Arch->model_library[3].inputs->name);
	free(Arch->model_library[3].inputs);
	free(Arch->model_library[3].outputs->name);
	free(Arch->model_library[3].outputs);
	free(Arch->model_library);

	if (Arch->clocks) {
		free(Arch->clocks->clock_inf);
	}

	free_complex_block_types();
	free_chunk_memory_trace();

	free(Arch->ipin_cblock_switch_name);
}

void free_options(const t_options *options) {

	free(options->ArchFile);
	free(options->CircuitName);
	if (options->ActFile)
		free(options->ActFile);
	if (options->BlifFile)
		free(options->BlifFile);
	if (options->NetFile)
		free(options->NetFile);
	if (options->PlaceFile)
		free(options->PlaceFile);
	if (options->PowerFile)
		free(options->PowerFile);
	if (options->CmosTechFile)
		free(options->CmosTechFile);
	if (options->RouteFile)
		free(options->RouteFile);
	if (options->out_file_prefix)
		free(options->out_file_prefix);
	if (options->PinFile)
		free(options->PinFile);
}

static void free_complex_block_types(void) {

	free_all_pb_graph_nodes();

	for (int i = 0; i < num_types; ++i) {
		free(type_descriptors[i].name);

		if (&type_descriptors[i] == EMPTY_TYPE) {
			continue;
		}

		for (int width = 0; width < type_descriptors[i].width; ++width) {
			for (int height = 0; height < type_descriptors[i].height; ++height) {
				for (int side = 0; side < 4; ++side) {
					for (int pin = 0; pin < type_descriptors[i].num_pin_loc_assignments[width][height][side]; ++pin) {
						if (type_descriptors[i].pin_loc_assignments[width][height][side][pin])
							free(type_descriptors[i].pin_loc_assignments[width][height][side][pin]);
					}
					free(type_descriptors[i].pinloc[width][height][side]);
					free(type_descriptors[i].pin_loc_assignments[width][height][side]);
				}
				free(type_descriptors[i].pinloc[width][height]);
				free(type_descriptors[i].pin_loc_assignments[width][height]);
				free(type_descriptors[i].num_pin_loc_assignments[width][height]);
			}
			free(type_descriptors[i].pinloc[width]);
			free(type_descriptors[i].pin_loc_assignments[width]);
			free(type_descriptors[i].num_pin_loc_assignments[width]);
		}
		free(type_descriptors[i].pinloc);
		free(type_descriptors[i].pin_loc_assignments);
		free(type_descriptors[i].num_pin_loc_assignments);

		free(type_descriptors[i].pin_width);
		free(type_descriptors[i].pin_height);

		for (int j = 0; j < type_descriptors[i].num_class; ++j) {
			free(type_descriptors[i].class_inf[j].pinlist);
		}
		free(type_descriptors[i].class_inf);
		free(type_descriptors[i].is_global_pin);
		free(type_descriptors[i].pin_class);
		free(type_descriptors[i].grid_loc_def);

		free(type_descriptors[i].is_Fc_frac);
		free(type_descriptors[i].is_Fc_full_flex);
        vtr::free_matrix(type_descriptors[i].Fc, 0, type_descriptors[i].num_pins-1, 0);

		free_pb_type(type_descriptors[i].pb_type);
		free(type_descriptors[i].pb_type);
	}
	free(type_descriptors);
}

static void free_pb_type(t_pb_type *pb_type) {

	free(pb_type->name);
	if (pb_type->blif_model)
		free(pb_type->blif_model);

	for (int i = 0; i < pb_type->num_modes; ++i) {
		for (int j = 0; j < pb_type->modes[i].num_pb_type_children; ++j) {
			free_pb_type(&pb_type->modes[i].pb_type_children[j]);
		}
		free(pb_type->modes[i].pb_type_children);
		free(pb_type->modes[i].name);
		for (int j = 0; j < pb_type->modes[i].num_interconnect; ++j) {
			free(pb_type->modes[i].interconnect[j].input_string);
			free(pb_type->modes[i].interconnect[j].output_string);
			free(pb_type->modes[i].interconnect[j].name);

			for (int k = 0; k < pb_type->modes[i].interconnect[j].num_annotations; ++k) {
				if (pb_type->modes[i].interconnect[j].annotations[k].clock)
					free(pb_type->modes[i].interconnect[j].annotations[k].clock);
				if (pb_type->modes[i].interconnect[j].annotations[k].input_pins) {
					free(pb_type->modes[i].interconnect[j].annotations[k].input_pins);
				}
				if (pb_type->modes[i].interconnect[j].annotations[k].output_pins) {
					free(pb_type->modes[i].interconnect[j].annotations[k].output_pins);
				}
				for (int m = 0; m < pb_type->modes[i].interconnect[j].annotations[k].num_value_prop_pairs; ++m) {
					free(pb_type->modes[i].interconnect[j].annotations[k].value[m]);
				}
				free(pb_type->modes[i].interconnect[j].annotations[k].prop);
				free(pb_type->modes[i].interconnect[j].annotations[k].value);
			}
			free(pb_type->modes[i].interconnect[j].annotations);
			if (pb_type->modes[i].interconnect[j].interconnect_power)
				free(pb_type->modes[i].interconnect[j].interconnect_power);
		}
		if (pb_type->modes[i].interconnect)
			free(pb_type->modes[i].interconnect);
		if (pb_type->modes[i].mode_power)
			free(pb_type->modes[i].mode_power);
	}
	if (pb_type->modes)
		free(pb_type->modes);

	for (int i = 0; i < pb_type->num_annotations; ++i) {
		for (int j = 0; j < pb_type->annotations[i].num_value_prop_pairs; ++j) {
			free(pb_type->annotations[i].value[j]);
		}
		free(pb_type->annotations[i].value);
		free(pb_type->annotations[i].prop);
		if (pb_type->annotations[i].input_pins) {
			free(pb_type->annotations[i].input_pins);
		}
		if (pb_type->annotations[i].output_pins) {
			free(pb_type->annotations[i].output_pins);
		}
		if (pb_type->annotations[i].clock) {
			free(pb_type->annotations[i].clock);
		}
	}
	if (pb_type->num_annotations > 0) {
		free(pb_type->annotations);
	}

	if (pb_type->pb_type_power) {
		free(pb_type->pb_type_power);
	}

	for (int i = 0; i < pb_type->num_ports; ++i) {
		free(pb_type->ports[i].name);
		if (pb_type->ports[i].port_class) {
			free(pb_type->ports[i].port_class);
		}
		if (pb_type->ports[i].port_power) {
			free(pb_type->ports[i].port_power);
		}
	}
	free(pb_type->ports);
}

void free_circuit() {

	/* Free netlist reference tables for nets */
	free(clb_to_vpack_net_mapping);
	free(vpack_to_clb_net_mapping);
	clb_to_vpack_net_mapping = NULL;
	vpack_to_clb_net_mapping = NULL;

	/* Free logical blocks and nets */
	if (logical_block != NULL) {
		free_logical_blocks();
		free_logical_nets();
	}

	if (clb_net != NULL) {
		for (int i = 0; i < num_nets; ++i) {
			free(clb_net[i].name);
			free(clb_net[i].node_block);
			free(clb_net[i].node_block_pin);
			free(clb_net[i].node_block_port);
		}
	}
	free(clb_net);
	clb_net = NULL;

	//Free new net structures
	free_global_nlist_net(&g_clbs_nlist);
	free_global_nlist_net(&g_atoms_nlist);

	if (block != NULL) {
		for (int i = 0; i < num_blocks; ++i) {
			if (block[i].pb != NULL) {
				free_cb(block[i].pb);
				free(block[i].pb);
			}
			free(block[i].nets);
			free(block[i].name);
			delete [] block[i].pb_route;
		}
	}
	free(block);
	block = NULL;

	free(blif_circuit_name);
	free(default_output_name);
	blif_circuit_name = NULL;

	vtr::t_linked_vptr *p_io_removed = circuit_p_io_removed;
	while (p_io_removed != NULL) {
		circuit_p_io_removed = p_io_removed->next;
		free(p_io_removed->data_vptr);
		free(p_io_removed);
		p_io_removed = circuit_p_io_removed;
	}
}

void vpr_free_vpr_data_structures(t_arch& Arch,
		const t_options& options,
		t_vpr_setup& vpr_setup) {

	if (vpr_setup.Timing.SDCFile != NULL) {
		free(vpr_setup.Timing.SDCFile);
		vpr_setup.Timing.SDCFile = NULL;
	}

	free_all_lb_type_rr_graph(vpr_setup.PackerRRGraph);
	free_options(&options);
	free_circuit();
	free_arch(&Arch);
	free_echo_file_info();
	free_output_file_names();
	free_timing_stats();
	free_sdc_related_structs();
}

void vpr_free_all(t_arch& Arch,
		const t_options& options,
		t_vpr_setup& vpr_setup) {

	//freePentMem();
	free_rr_graph();
	if (vpr_setup.RouterOpts.doRouting) {
		free_route_structs();
	}
	free_trace_structs();
	vpr_free_vpr_data_structures(Arch, options, vpr_setup);
}


/****************************************************************************************************
 * Advanced functions
 *  Used when you need fine-grained control over VPR that the main VPR operations do not enable
 ****************************************************************************************************/
/* Read in user options */
//void vpr_read_options(const int argc, const char **argv, t_options * options) {
//	ReadOptions(argc, argv, options);
//}

/* Read in arch and circuit */
void vpr_setup_vpr(t_options *Options, const bool TimingEnabled,
		const bool readArchFile, struct s_file_name_opts *FileNameOpts,
		t_arch * Arch,
		t_model ** user_models, t_model ** library_models,
		struct s_packer_opts *PackerOpts,
		struct s_placer_opts *PlacerOpts,
		struct s_annealing_sched *AnnealSched,
		struct s_router_opts *RouterOpts,
		struct s_det_routing_arch *RoutingArch,
		vector <t_lb_type_rr_node> **PackerRRGraph,
		t_segment_inf ** Segments, t_timing_inf * Timing,
		bool * ShowGraphics, int *GraphPause,
		t_power_opts * PowerOpts) {
	SetupVPR(Options, TimingEnabled, readArchFile, FileNameOpts, Arch,
			user_models, library_models, PackerOpts, PlacerOpts,
			AnnealSched, RouterOpts, RoutingArch, PackerRRGraph, Segments, Timing,
			ShowGraphics, GraphPause, PowerOpts);
}
/* Check inputs are reasonable */
void vpr_check_options(const t_options& Options, const bool TimingEnabled) {
	CheckOptions(Options, TimingEnabled);
}
void vpr_check_arch(const t_arch& Arch) {
	CheckArch(Arch);
}
/* Verify settings don't conflict or otherwise not make sense */
void vpr_check_setup(const struct s_placer_opts PlacerOpts,
		const struct s_router_opts RouterOpts,
		const struct s_det_routing_arch RoutingArch, const t_segment_inf * Segments,
		const t_timing_inf Timing, const t_chan_width_dist Chans) {
	CheckSetup(PlacerOpts, RouterOpts, RoutingArch,
			Segments, Timing, Chans);
}
/* Read blif file and sweep unused components */
void vpr_read_and_process_blif(const char *blif_file,
		const bool sweep_hanging_nets_and_inputs, bool absorb_buffer_luts,
        const t_model *user_models,
		const t_model *library_models, bool read_activity_file,
		char * activity_file) {
	read_and_process_blif(blif_file, sweep_hanging_nets_and_inputs, absorb_buffer_luts, user_models,
			library_models, read_activity_file, activity_file);
}
/* Show current setup */
void vpr_show_setup(const t_options& options, const t_vpr_setup& vpr_setup) {
	ShowSetup(options, vpr_setup);
}

/* Output file names management */
void vpr_alloc_and_load_output_file_names(const char* default_name) {
	alloc_and_load_output_file_names(default_name);
}
void vpr_set_output_file_name(enum e_output_files ename, const char *name,
		const char* default_name) {
	setOutputFileName(ename, name, default_name);
}
char *vpr_get_output_file_name(enum e_output_files ename) {
	return getOutputFileName(ename);
}

static void resync_pb_graph_nodes_in_pb(t_pb_graph_node *pb_graph_node,
		t_pb *pb) {

	if (pb->name == NULL) {
		return;
	}

	VTR_ASSERT(strcmp(pb->pb_graph_node->pb_type->name, pb_graph_node->pb_type->name) == 0);

	pb->pb_graph_node = pb_graph_node;
	if (pb->child_pbs != NULL) {
		for (int i = 0; i < pb_graph_node->pb_type->modes[pb->mode].num_pb_type_children; ++i) {
			if (pb->child_pbs[i] != NULL) {
				for (int j = 0; j < pb_graph_node->pb_type->modes[pb->mode].pb_type_children[i].num_pb; ++j) {
					resync_pb_graph_nodes_in_pb(
							&pb_graph_node->child_pb_graph_nodes[pb->mode][i][j],
							&pb->child_pbs[i][j]);
				}
			}
		}
	}
}

/* This function performs power estimation, and must be called
 * after packing, placement AND routing. Currently, this
 * will not work when running a partial flow (ex. only routing).
 */
void vpr_power_estimation(const t_vpr_setup& vpr_setup, const t_arch& Arch) {

	/* Ensure we are only using 1 clock */
	VTR_ASSERT(count_netlist_clocks() == 1);

	/* Get the critical path of this clock */
	g_solution_inf.T_crit = get_critical_path_delay() / 1e9;
	VTR_ASSERT(g_solution_inf.T_crit > 0.);

	vtr::printf_info("\n\nPower Estimation:\n");
	vtr::printf_info("-----------------\n");

	vtr::printf_info("Initializing power module\n");

	/* Initialize the power module */
	bool power_error = power_init(vpr_setup.FileNameOpts.PowerFile,
			vpr_setup.FileNameOpts.CmosTechFile, &Arch, &vpr_setup.RoutingArch);
	if (power_error) {
		vtr::printf_error(__FILE__, __LINE__,
				"Power initialization failed.\n");
	}

	if (!power_error) {
		float power_runtime_s;

		vtr::printf_info("Running power estimation\n");

		/* Run power estimation */
		e_power_ret_code power_ret_code = power_total(&power_runtime_s, vpr_setup,
				&Arch, &vpr_setup.RoutingArch);

		/* Check for errors/warnings */
		if (power_ret_code == POWER_RET_CODE_ERRORS) {
			vtr::printf_error(__FILE__, __LINE__,
					"Power estimation failed. See power output for error details.\n");
		} else if (power_ret_code == POWER_RET_CODE_WARNINGS) {
			vtr::printf_warning(__FILE__, __LINE__,
					"Power estimation completed with warnings. See power output for more details.\n");
		} else if (power_ret_code == POWER_RET_CODE_SUCCESS) {
		}
		vtr::printf_info("Power estimation took %g seconds\n", power_runtime_s);
	}

	/* Uninitialize power module */
	if (!power_error) {
		vtr::printf_info("Uninitializing power module\n");
		power_error = power_uninit();
		if (power_error) {
			vtr::printf_error(__FILE__, __LINE__,
					"Power uninitialization failed.\n");
		} else {

		}
	}
	vtr::printf_info("\n");
}

void vpr_print_error(const VprError& vpr_error){

	/* Determine the type of VPR error, To-do: can use some enum-to-string mechanism */
    char* error_type = NULL;
    try {
        switch(vpr_error.type()){
        case VPR_ERROR_UNKNOWN:
            error_type = vtr::strdup("Unknown");
            break;
        case VPR_ERROR_ARCH:
            error_type = vtr::strdup("Architecture file");
            break;
        case VPR_ERROR_PACK:
            error_type = vtr::strdup("Packing");
            break;
        case VPR_ERROR_PLACE:
            error_type = vtr::strdup("Placement");
            break;
        case VPR_ERROR_ROUTE:
            error_type = vtr::strdup("Routing");
            break;
        case VPR_ERROR_TIMING:
            error_type = vtr::strdup("Timing");
            break;
        case VPR_ERROR_SDC:
            error_type = vtr::strdup("SDC file");
            break;
        case VPR_ERROR_NET_F:
            error_type = vtr::strdup("Netlist file");
            break;
        case VPR_ERROR_BLIF_F:
            error_type = vtr::strdup("Blif file");
            break;
        case VPR_ERROR_PLACE_F:
            error_type = vtr::strdup("Placement file");
            break;
        case VPR_ERROR_IMPL_NETLIST_WRITER:
            error_type = vtr::strdup("Implementation Netlist Writer");
            break;
        case VPR_ERROR_OTHER:
            error_type = vtr::strdup("Other");
            break;
        default:
            error_type = vtr::strdup("Unrecognized Error");
            break;
        }
    } catch(const vtr::VtrError& e) {
        error_type = NULL;
    }

    //We can't pass std::string's through va_args functions,
    //so we need to copy them and pass via c_str()
    std::string msg = vpr_error.what();
    std::string filename = vpr_error.filename();

	vtr::printf_error(__FILE__, __LINE__,
		"\nType: %s\nFile: %s\nLine: %d\nMessage: %s\n",
		error_type, filename.c_str(), vpr_error.line(),
		msg.c_str());
}
