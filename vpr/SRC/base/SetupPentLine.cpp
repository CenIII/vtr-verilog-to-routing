//
// Created by ivan on 10/15/16.
//

#include "SetupPentLine.h"
#include "globals.h"
#include "vtr_assert.h"




void build_one_rod(char* ptr, t_pent_seg_type_info *pent_ptr, int edgenum, int NoInPat, int ind_type);
void build_pent_type();
void add_one_map_ptr(t_pent_seg_type_info* pent_ptr);
void map_len_to_pent();

/*straight or pent, subnum, length, x or y, inc or dec. Deep First Traverse. Notice: NoInPat is Bread First*/
char PentTypeData[PENTYPE][50] = {"-s02xi",
                                  "-p12xi-p12yi-p02xi","-p12xi-p12yd-p02xi",
                                  "-p12xi-p02yi","-p12xi-p02yd",

                                  "-p12xi-p04yi","-p12xi-p04yd",
                                  "-p14xi-p02yi","-p14xi-p02yd",

                                  "-s04xi",
                                  "-s06xi"

};
float PentTypeFreq[PENTYPE] = {0,

                               65,65,
                               15,15,

                               15,15,
                               15,15,

                               10,
                               20
};


void build_one_rod(char* ptr, t_pent_seg_type_info *pent_ptr, int edgenum, int NoInPat, int ind_type){
    for(int EgeN=0;EgeN<edgenum;EgeN++) {
        for (int i = 0; i < 6; i++) {
            switch (i) {
                case 0:
                    break;
                case 1:
                    pent_ptr[EgeN].isPent = ((*ptr) != 's');
                    break;
                case 2:
                    if ((*ptr - '0') != 0) {
                        pent_ptr[EgeN].numSubEdges = (*ptr - '0');
                        pent_ptr[EgeN].SubEdges = (t_pent_seg_type_info *) malloc( pent_ptr[EgeN].numSubEdges * sizeof(t_pent_seg_type_info));
                    } else {
                        pent_ptr[EgeN].numSubEdges = 0;
                        pent_ptr[EgeN].SubEdges = NULL;
                    }
                    break;
                case 3:
                    pent_ptr[EgeN].Length = (*ptr) - '0';
                    break;
                case 4:
                    pent_ptr[EgeN].XorY = (((*ptr) == 'x') ? CHANX : CHANY);
                    break;
                case 5:
                    pent_ptr[EgeN].Direction = (((*ptr) == 'i') ? INC_DIRECTION : DEC_DIRECTION);
                default:
                    break;
            }
            ptr++;
        }
        pent_ptr[EgeN].occ=0;
        pent_ptr[EgeN].cap=0;
        pent_ptr[EgeN].start_p_array = (int *)calloc(0, (pent_ptr[EgeN].Length + 1) * sizeof(int));
        pent_ptr[EgeN].NoInPatt = NoInPat + EgeN;
        pent_ptr[EgeN].PatFreq=PentTypeFreq[ind_type];
        lp_map[pent_ptr[EgeN].Length].rodNum = lp_map[pent_ptr[EgeN].Length].rodNum + 1;
        if(pent_ptr[EgeN].numSubEdges > 0) {
            VTR_ASSERT(*ptr == '-');
            build_one_rod(ptr, pent_ptr[EgeN].SubEdges, pent_ptr[EgeN].numSubEdges, NoInPat + edgenum,ind_type);
        }
    }
}

void build_pent_type(){
    lp_map = (t_len_to_pent_map *)malloc( (MAXLENGTH+1) * sizeof(t_len_to_pent_map));
    for(int k=0;k<MAXLENGTH+1;k++)
        lp_map[k].rodNum=0;
    pent_type = (t_pent_seg_type_info *)malloc( PENTYPE * sizeof(t_pent_seg_type_info));
    t_pent_seg_type_info *pent_ptr = pent_type;
    char* ptr = PentTypeData[0];
    for(int i=0;i<PENTYPE;i++){
        ptr=PentTypeData[i];
        pent_ptr = pent_type + i;
        VTR_ASSERT((*ptr) == '-');
        build_one_rod(ptr,pent_ptr,1,0,i);
    }
}

void add_one_map_ptr(t_pent_seg_type_info* pent_ptr){
    int i=0;
    while(lp_map[pent_ptr->Length].ptype_list[i] != NULL){i++;}
    lp_map[pent_ptr->Length].ptype_list[i] = pent_ptr;
    if(pent_ptr->numSubEdges > 0){
        for(int j=0;j<pent_ptr->numSubEdges;j++)
        add_one_map_ptr(&(pent_ptr->SubEdges[j]));
    }
}
void map_len_to_pent(){
    for(int i=0;i<MAXLENGTH+1;i++){
        lp_map[i].ptype_list=NULL;
        if(lp_map[i].rodNum > 0){
            lp_map[i].ptype_list = (t_pent_seg_type_info**)malloc(lp_map[i].rodNum * sizeof(t_pent_seg_type_info*));
            for(int k=0;k<lp_map[i].rodNum;k++)
                lp_map[i].ptype_list[k]=NULL;
        }else
            lp_map[i].ptype_list = NULL;
    }
    /* Traverse the Pats and fill the map. */
    for(int i=0;i<PENTYPE;i++){
        add_one_map_ptr(&pent_type[i]);
    }
}

void calc_freq(){
    /* calc for each length. In lp_map*/
    int freq_sum=0;
    for(int i=0;i<MAXLENGTH+1;i++){
        if(lp_map[i].rodNum > 0){
            for(int j=0;j<lp_map[i].rodNum;j++){
                lp_map[i].freq += lp_map[i].ptype_list[j]->PatFreq * lp_map[i].ptype_list[j]->Length;
            }
        } else{
            lp_map[i].freq = 0;
        }
        freq_sum += lp_map[i].freq;
    }
    for(int i=0;i<MAXLENGTH+1;i++){
        if(lp_map[i].rodNum > 0) {
            for (int j = 0; j < lp_map[i].rodNum; j++) {
                lp_map[i].ptype_list[j]->freq_in_chan = lp_map[i].ptype_list[j]->PatFreq * lp_map[i].ptype_list[j]->Length / lp_map[i].freq;
            }
        }
        lp_map[i].freq /= freq_sum;
    }

    /* calc for each Pat in each chan. In pent_type */
}
void SetupPentLine(){
    pent_type=NULL;
    lp_map=NULL;
    build_pent_type();
    map_len_to_pent();
    calc_freq();
}

void SetPentFreq(int index, int freq){
    PentTypeFreq[index]=freq;
}

void freePentMem(){
    free(lp_map);
    free(pent_type);
}