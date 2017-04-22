//
// Created by ivan on 10/15/16.
//

#ifndef VTR_SETUPPENTLINE_H_H
#define VTR_SETUPPENTLINE_H_H

#endif //VTR_SETUPPENTLINE_H_H

#define PENTYPE 9
#define MAXLENGTH 6
extern float PentTypeFreq[PENTYPE];
extern bool PairedOrNot[PENTYPE];
extern int PentLengthSum[PENTYPE];

void SetupPentLine();
void SetPentFreq(int index, int freq);
void freePentMem();