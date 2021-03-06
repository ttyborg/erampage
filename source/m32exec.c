//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers

This file is part of EDuke32

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

#include <time.h>
#include <stdlib.h>

#include "m32script.h"
#include "m32def.h"
#include "macros.h"
#include "sounds_mapster32.h"
#include "fx_man.h"
//#include "scriplib.h"

//#include "osdcmds.h"
#include "osd.h"
#include "keys.h"

vmstate_t vm;
vmstate_t vm_default = {
    -1,
    0,
    NULL,
    0
};

int32_t g_errorLineNum, g_tw;

uint8_t aEventEnabled[MAXEVENTS];

uint32_t m32_drawlinepat=0xffffffff;

instype *insptr;
int32_t X_DoExecute(int32_t once);
static instype *x_sortingstateptr;

#include "m32structures.c"

extern void message(const char *fmt, ...);

// from sector.c vvv
static int32_t ldist(spritetype *s1,spritetype *s2) {
    int32_t x= klabs(s1->x-s2->x);
    int32_t y= klabs(s1->y-s2->y);

    if (x<y) swaplong(&x,&y);

    {
        int32_t t = y + (y>>1);
        return (x - (x>>5) - (x>>7)  + (t>>2) + (t>>6));
    }
}

static int32_t dist(spritetype *s1,spritetype *s2) {
    int32_t x= klabs(s1->x-s2->x);
    int32_t y= klabs(s1->y-s2->y);
    int32_t z= klabs((s1->z-s2->z)>>4);

    if (x<y) swaplong(&x,&y);
    if (x<z) swaplong(&x,&z);

    {
        int32_t t = y + z;
        return (x - (x>>4) + (t>>2) + (t>>3));
    }
}
///

void X_Disasm(ofstype beg, int32_t size) {
    instype *p;

    if (!script) return;
    if (beg<0 || beg+size>g_scriptSize) return;

    initprintf("beg=%d, size=%d:  ", beg, size);
    for (p=script+beg; p<script+beg+size; p++) {
        if (*p>>12 && (*p&0xFFF)<CON_END)
            initprintf("%s ", keyw[*p&0xFFF]);
        else
            initprintf("%d ", *p);
    }
    initprintf("\n");
}

void X_ScriptInfo(void) {
    if (script) {
        instype *p;
        if (insptr)
            for (p=max(insptr-20,script); p<min(insptr+20, script+g_scriptSize); p++) {
                if (p==insptr) initprintf("<<");

                if (*p>>12 && (*p&0xFFF)<CON_END)
                    initprintf("\n%5d: L%5d:  %s ",p-script,*p>>12,keyw[*p&0xFFF]);
                else initprintf(" %d",*p);

                if (p==insptr) initprintf(">>");
            }
        initprintf(" \n");
        if (vm.g_i >= 0)
            initprintf("current sprite: %d\n",vm.g_i);
        if (g_tw>=0 && g_tw<CON_END)
            initprintf("g_errorLineNum: %d, g_tw: %s\n",g_errorLineNum,keyw[g_tw]);
        else
            initprintf("g_errorLineNum: %d, g_tw: %d\n",g_errorLineNum,g_tw);
    }
}

void X_OnEvent(register int32_t iEventID, register int32_t iActor) {
    if (iEventID < 0 || iEventID >= MAXEVENTS) {
        OSD_Printf(CON_ERROR "invalid event ID",g_errorLineNum,keyw[g_tw]);
        return;
    }

    if (aEventOffsets[iEventID] < 0 || !aEventEnabled[iEventID]) {
        //Bsprintf(g_szBuf,"No event found for %d",iEventID);
        //AddLog(g_szBuf);
        return;
    }

    {
        instype *oinsptr=insptr;
        vmstate_t vm_backup;

        Bmemcpy(&vm_backup, &vm, sizeof(vmstate_t));

        vm.g_i = iActor;    // current sprite ID
        if (vm.g_i >= 0)
            vm.g_sp = &sprite[vm.g_i];

        vm.g_st = 1+iEventID;

        vm.flags = 0;

        insptr = script + aEventOffsets[iEventID];
        X_DoExecute(0);

        if (vm.flags&VMFLAG_ERROR) {
            aEventEnabled[iEventID] = 0;
            message("ERROR executing %s. Event disabled.", label+(iEventID*MAXLABELLEN));
        }

        // restore old values...
        Bmemcpy(&vm, &vm_backup, sizeof(vmstate_t));
        insptr = oinsptr;

        //AddLog("End of Execution");
    }
}

static int32_t G_GetAngleDelta(int32_t a,int32_t na) {
    a &= 2047;
    na &= 2047;

    if (klabs(a-na) < 1024) {
//        OSD_Printf("G_GetAngleDelta() returning %d\n",na-a);
        return (na-a);
    }

    if (na > 1024) na -= 2048;
    if (a > 1024) a -= 2048;

//    OSD_Printf("G_GetAngleDelta() returning %d\n",na-a);
    return (na-a);
}

static inline void __fastcall X_DoConditional(register int32_t condition) {
    if (condition) {
        // skip 'else' pointer.. and...
        insptr+=2;
        X_DoExecute(1);
        return;
    }

    insptr++;
    insptr += *insptr;
    if (((*insptr)&0xFFF) == CON_ELSE) {
        // else...
        // skip 'else' and...
        insptr+=2;
        X_DoExecute(1);
    }
}

static int32_t X_DoSortDefault(const int32_t *lv, const int32_t *rv) {
    return (*rv - *lv);
}

static int32_t X_DoSort(const int32_t *lv, const int32_t *rv) {
    m32_sortvar1 = *lv;
    m32_sortvar2 = *rv;
    insptr = x_sortingstateptr;
    X_DoExecute(0);
    return g_iReturnVar;
}

#define X_ERROR_INVALIDCI()                                             \
    if ((vm.g_i < 0 || vm.g_i>=MAXSPRITES) &&                           \
        (vm.g_st!=0 || searchstat!=3 || (vm.g_i=searchwall, vm.g_sp=&sprite[vm.g_i], 0))) \
    {                                                                   \
        OSD_Printf(CON_ERROR "Current sprite index invalid!\n", g_errorLineNum, keyw[g_tw]); \
        vm.flags |= VMFLAG_ERROR;                                       \
        continue;                                                       \
    }

#define X_ERROR_INVALIDSPRI(dasprite)                                   \
    if (dasprite < 0 || dasprite>=MAXSPRITES)                           \
    {                                                                   \
        OSD_Printf(CON_ERROR "Invalid sprite index %d!\n", g_errorLineNum, keyw[g_tw], dasprite); \
        vm.flags |= VMFLAG_ERROR;                                       \
        continue;                                                       \
    }

#define X_ERROR_INVALIDSECT(dasect)                                     \
    if (dasect < 0 || dasect>=numsectors)                               \
    {                                                                   \
        OSD_Printf(CON_ERROR "Invalid sector index %d!\n", g_errorLineNum, keyw[g_tw], dasect); \
        vm.flags |= VMFLAG_ERROR;                                       \
        continue;                                                       \
    }

#define X_ERROR_INVALIDSP()                                             \
    if (!vm.g_sp && (vm.g_st!=0 || searchstat!=3 || (vm.g_sp=&sprite[searchwall], 0))) \
    {                                                                   \
        OSD_Printf(CON_ERROR "Current sprite invalid!\n", g_errorLineNum, keyw[g_tw]); \
        vm.flags |= VMFLAG_ERROR;                                       \
        continue;                                                       \
    }

#define X_ERROR_INVALIDQUOTE(q, array)                                  \
    if (q<0 || q>=MAXQUOTES)                                            \
    {                                                                   \
        OSD_Printf(CON_ERROR "Invalid quote number %d!\n", g_errorLineNum, keyw[g_tw], q); \
        vm.flags |= VMFLAG_ERROR;                                       \
        continue;                                                       \
    }                                                                   \
    else if (array[q] == NULL)                                          \
    {                                                                   \
        OSD_Printf(CON_ERROR "Null quote %d!\n", g_errorLineNum, keyw[g_tw], q); \
        vm.flags |= VMFLAG_ERROR;                                       \
        continue;                                                       \
    }                                                                   \
 
int32_t X_DoExecute(int32_t once) {
    register int32_t tw = *insptr;

    // jump directly into the loop, saving us from the checks during the first iteration
    goto skip_check;

    while (!once) {
        if (vm.flags)
            return 1;

        tw = *insptr;

skip_check:
        //      Bsprintf(g_szBuf,"Parsing: %d",*insptr);
        //      AddLog(g_szBuf);

        g_errorLineNum = tw>>12;
        g_tw = (tw &= 0xFFF);

        switch (tw) {
// *** basic commands
        case CON_NULLOP:
            insptr++;
            continue;

        case CON_STATE: {
            instype *tempscrptr = insptr+2;
            int32_t stateidx = *(insptr+1), o_g_st = vm.g_st, oret=vm.flags&VMFLAG_RETURN;

            insptr = script + statesinfo[stateidx].ofs;
            vm.g_st = 1+MAXEVENTS+stateidx;
            X_DoExecute(0);
            vm.g_st = o_g_st;
            vm.flags &= ~VMFLAG_RETURN;
            vm.flags |= oret;
            insptr = tempscrptr;
        }
        continue;

        case CON_RETURN:
            vm.flags |= VMFLAG_RETURN;
            return 1;
        case CON_BREAK:
            vm.flags |= VMFLAG_BREAK;
        case CON_ENDS:
            return 1;

        case CON_ELSE:
            insptr++;
            insptr += *insptr;
            continue;

        case CON_ENDSWITCH:
            vm.flags &= ~VMFLAG_BREAK;
        case CON_ENDEVENT:
            insptr++;
            return 1;

        case CON_SWITCH:
            insptr++; // p-code
            {
                // command format:
                // variable ID to check
                // script offset to 'end'
                // count of case statements
                // script offset to default case (null if none)
                // For each case: value, ptr to code
                //AddLog("Processing Switch...");
                int32_t lValue=Gv_GetVarX(*insptr++), lEnd=*insptr++, lCases=*insptr++;
                instype *lpDefault=insptr++, *lpCases=insptr, *lCodeInsPtr;
                int32_t bMatched=0, lCheckCase;
                int32_t left,right;

                insptr += lCases*2;
                lCodeInsPtr = insptr;
                //Bsprintf(g_szBuf,"lEnd= %d *lpDefault=%d",lEnd,*lpDefault); AddLog(g_szBuf);
                //Bsprintf(g_szBuf,"Checking %d cases for %d",lCases, lValue); AddLog(g_szBuf);
                left = 0;
                right = lCases-1;
                while (!bMatched) {
                    //Bsprintf(g_szBuf,"Checking #%d Value= %d",lCheckCase, lpCases[lCheckCase*2]); AddLog(g_szBuf);
                    lCheckCase=(left+right)/2;
                    //                initprintf("(%2d..%2d..%2d) [%2d..%2d..%2d]==%2d\n",left,lCheckCase,right,lpCases[left*2],lpCases[lCheckCase*2],lpCases[right*2],lValue);
                    if (lpCases[lCheckCase*2] > lValue)
                        right = lCheckCase-1;
                    else if (lpCases[lCheckCase*2] < lValue)
                        left = lCheckCase+1;
                    else if (lpCases[lCheckCase*2] == lValue) {
                        //AddLog("Found Case Match");
                        //Bsprintf(g_szBuf,"insptr=%d. lCheckCase=%d, offset=%d, &script[0]=%d", (int32_t)insptr,(int32_t)lCheckCase,lpCases[lCheckCase*2+1],(int32_t)&script[0]); AddLog(g_szBuf);
                        // fake a 2-d Array
                        insptr = lCodeInsPtr + lpCases[lCheckCase*2+1];
                        //Bsprintf(g_szBuf,"insptr=%d. ",     (int32_t)insptr); AddLog(g_szBuf);
                        X_DoExecute(0);
                        //AddLog("Done Executing Case");
                        bMatched=1;
                    }
                    if (right-left < 0)
                        break;
                }
                if (!bMatched) {
                    if (*lpDefault >= 0) {
                        //AddLog("No Matching Case: Using Default");
                        insptr = lCodeInsPtr + *lpDefault;
                        X_DoExecute(0);
                    }
//                    else
//                    {
//                        //AddLog("No Matching Case: No Default to use");
//                    }
                }
                insptr = (instype *)(lCodeInsPtr + lEnd);
                //Bsprintf(g_szBuf,"insptr=%d. ",     (int32_t)insptr); AddLog(g_szBuf);
                //AddLog("Done Processing Switch");
                continue;
            }

        case CON_GETCURRADDRESS:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, insptr-script);
            }
            continue;

        case CON_JUMP:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                if (j<0 || j>=(g_scriptPtr-script)) {
                    OSD_Printf(CON_ERROR "script index out of bounds (%d)\n", g_errorLineNum, keyw[g_tw], j);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }
                insptr = (instype *)(j+script);
            }
            continue;

        case CON_RIGHTBRACE:
            insptr++;
            return 1;
        case CON_LEFTBRACE:
            insptr++;
            X_DoExecute(0);
            continue;

// *** more basic commands
        case CON_SETSECTOR:
        case CON_GETSECTOR:
            insptr++;
            {
                // syntax [gs]etsector[<var>].x <VAR>
                // <varid> <xxxid> <varid>
                int32_t lVar1=*insptr++, lLabelID=*insptr++, lVar2=*insptr++;

                X_AccessSector((tw==CON_SETSECTOR)|2, lVar1, lLabelID, lVar2);
                continue;
            }
        case CON_SETWALL:
        case CON_GETWALL:
            insptr++;
            {
                // syntax [gs]etwall[<var>].x <VAR>
                // <varid> <xxxid> <varid>
                int32_t lVar1=*insptr++, lLabelID=*insptr++, lVar2=*insptr++;
                X_AccessWall((tw==CON_SETWALL)|2, lVar1, lLabelID, lVar2);
                continue;
            }
        case CON_SETSPRITE:
        case CON_GETSPRITE:
            insptr++;
            {
                // syntax [gs]etsprite[<var>].x <VAR>
                // <varid> <xxxid> <varid>
                int32_t lVar1=*insptr++, lLabelID=*insptr++, lVar2=*insptr++;
                X_AccessSprite((tw==CON_SETSPRITE)|2, lVar1, lLabelID, lVar2);
                continue;
            }

        case CON_SETTSPR:
        case CON_GETTSPR:
            insptr++;
            {
                // syntax [gs]ettspr[<var>].x <VAR>
                // <varid> <xxxid> <varid>
                int32_t lVar1=*insptr++, lLabelID=*insptr++, lVar2=*insptr++;
                X_AccessTsprite((tw==CON_SETTSPR)|2|4, lVar1, lLabelID, lVar2);
                continue;
            }
#if 0
        case CON_SETSPRITE:
            insptr++;
            {
                // syntax [gs]etsprite[<var>].x <VAR>
                // <varid> <xxxid> <varid>
                int32_t lVar1=*insptr++, lLabelID=*insptr++, lVar2=*insptr++;
                X_SetSprite(lVar1, lLabelID, lVar2);
                continue;
            }

        case CON_GETSPRITE:
            insptr++;
            {
                // syntax [gs]etsprite[<var>].x <VAR>
                // <varid> <xxxid> <varid>
                int32_t lVar1=*insptr++, lLabelID=*insptr++, lVar2=*insptr++;
                X_GetSprite(lVar1, lLabelID, lVar2);
                continue;
            }
#endif

// *** arrays
        case CON_SETARRAY:
            insptr++;
            {
                int32_t j=*insptr++;
                int32_t index = Gv_GetVarX(*insptr++);
                int32_t value = Gv_GetVarX(*insptr++);

                if (j<0 || j >= g_gameArrayCount) {
                    OSD_Printf(CON_ERROR "Tried to set invalid array ID (%d)\n", g_errorLineNum, keyw[g_tw], j);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (aGameArrays[j].dwFlags & GAMEARRAY_READONLY) {
                    OSD_Printf(CON_ERROR "Tried to set on read-only array `%s'\n", g_errorLineNum, keyw[g_tw], aGameArrays[j].szLabel);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (index >= aGameArrays[j].size || index < 0) {
                    OSD_Printf(CON_ERROR "Array index %d out of bounds\n", g_errorLineNum, keyw[g_tw], index);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (vm.flags&VMFLAG_ERROR) continue;
                ((int32_t *)aGameArrays[j].vals)[index]=value;  // REM: other array types not implemented, since they're read-only
                continue;
            }

        case CON_GETARRAYSIZE:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(*insptr++, (aGameArrays[j].dwFlags&GAMEARRAY_VARSIZE) ?
                           Gv_GetVarN(aGameArrays[j].size) : aGameArrays[j].size);
            }
            continue;

        case CON_RESIZEARRAY:
            insptr++;
            {
                int32_t j=*insptr++;
                int32_t asize = Gv_GetVarX(*insptr++);

                if (asize<=0 || asize>65536) {
                    OSD_Printf(CON_ERROR "Invalid array size %d (max: 65536)\n",g_errorLineNum,keyw[g_tw]);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

//                OSD_Printf(OSDTEXT_GREEN "CON_RESIZEARRAY: resizing array %s from %d to %d\n", aGameArrays[j].szLabel, aGameArrays[j].size, asize);
                aGameArrays[j].vals = Brealloc(aGameArrays[j].vals, sizeof(int32_t) * asize);
                if (aGameArrays[j].vals == NULL) {
                    aGameArrays[j].size = 0;
                    OSD_Printf(CON_ERROR "Out of memory!\n",g_errorLineNum,keyw[g_tw]);
                    vm.flags |= VMFLAG_ERROR;
                    return 1;
                }
                aGameArrays[j].size = asize;

                continue;
            }

        case CON_COPY:
            insptr++;
            {
                int32_t si=*insptr++, ssiz;
                int32_t sidx = Gv_GetVarX(*insptr++); //, vm.g_i, vm.g_p);
                int32_t di=*insptr++, dsiz;
                int32_t didx = Gv_GetVarX(*insptr++);
                int32_t numelts = Gv_GetVarX(*insptr++);

                if (si<0 || si>=g_gameArrayCount) {
                    OSD_Printf(CON_ERROR "Invalid array %d!\n",g_errorLineNum,keyw[g_tw],si);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (di<0 || di>=g_gameArrayCount) {
                    OSD_Printf(CON_ERROR "Invalid array %d!\n",g_errorLineNum,keyw[g_tw],di);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (aGameArrays[di].dwFlags & GAMEARRAY_READONLY) {
                    OSD_Printf(CON_ERROR "Array %d is read-only!\n",g_errorLineNum,keyw[g_tw],di);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (vm.flags&VMFLAG_ERROR) continue;

                ssiz = (aGameArrays[si].dwFlags&GAMEARRAY_VARSIZE) ?
                       Gv_GetVarN(aGameArrays[si].size) : aGameArrays[si].size;
                dsiz = (aGameArrays[di].dwFlags&GAMEARRAY_VARSIZE) ?
                       Gv_GetVarN(aGameArrays[si].size) : aGameArrays[di].size;

                if (sidx > ssiz || didx > dsiz) continue;
                if ((sidx+numelts) > ssiz) numelts = ssiz-sidx;
                if ((didx+numelts) > dsiz) numelts = dsiz-didx;

                switch (aGameArrays[si].dwFlags & GAMEARRAY_TYPEMASK) {
                case 0:
                case GAMEARRAY_OFINT:
                    Bmemcpy((int32_t*)aGameArrays[di].vals + didx, (int32_t *)aGameArrays[si].vals + sidx, numelts * sizeof(int32_t));
                    break;
                case GAMEARRAY_OFSHORT:
                    for (; numelts>0; numelts--)
                        ((int32_t *)aGameArrays[di].vals)[didx++] = ((int16_t *)aGameArrays[si].vals)[sidx++];
                    break;
                case GAMEARRAY_OFCHAR:
                    for (; numelts>0; numelts--)
                        ((int32_t *)aGameArrays[di].vals)[didx++] = ((uint8_t *)aGameArrays[si].vals)[sidx++];
                    break;
                }
                continue;
            }

// *** var & varvar ops
        case CON_RANDVAR:
            insptr++;
            Gv_SetVarX(*insptr, mulscale16(krand(), *(insptr+1)+1));
            insptr += 2;
            continue;

        case CON_DISPLAYRANDVAR:
            insptr++;
            Gv_SetVarX(*insptr, mulscale15((uint16_t)rand(), *(insptr+1)+1));
            insptr += 2;
            continue;

        case CON_SETVAR:
            insptr++;
            Gv_SetVarX(*insptr, *(insptr+1));
            insptr += 2;
            continue;

        case CON_SETVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_MULVAR:
            insptr++;
            Gv_SetVarX(*insptr, Gv_GetVarX(*insptr) * *(insptr+1));
            insptr += 2;
            continue;

        case CON_DIVVAR:
            insptr++;
            if (*(insptr+1) == 0) {
                OSD_Printf(CON_ERROR "Divide by zero.\n",g_errorLineNum,keyw[g_tw]);
                insptr += 2;
                continue;
            }
            Gv_SetVarX(*insptr, Gv_GetVarX(*insptr) / *(insptr+1));
            insptr += 2;
            continue;

        case CON_MODVAR:
            insptr++;
            if (*(insptr+1) == 0) {
                OSD_Printf(CON_ERROR "Mod by zero.\n",g_errorLineNum,keyw[g_tw]);
                insptr += 2;
                continue;
            }
            Gv_SetVarX(*insptr,Gv_GetVarX(*insptr)%*(insptr+1));
            insptr += 2;
            continue;

        case CON_ANDVAR:
            insptr++;
            Gv_SetVarX(*insptr,Gv_GetVarX(*insptr) & *(insptr+1));
            insptr += 2;
            continue;

        case CON_ORVAR:
            insptr++;
            Gv_SetVarX(*insptr,Gv_GetVarX(*insptr) | *(insptr+1));
            insptr += 2;
            continue;

        case CON_XORVAR:
            insptr++;
            Gv_SetVarX(*insptr,Gv_GetVarX(*insptr) ^ *(insptr+1));
            insptr += 2;
            continue;

        case CON_RANDVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j,mulscale(krand(), Gv_GetVarX(*insptr++)+1, 16));
            }
            continue;

        case CON_DISPLAYRANDVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j,mulscale((uint16_t)rand(), Gv_GetVarX(*insptr++)+1, 15));
            }
            continue;

        case CON_MULVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(j)*Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_DIVVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                int32_t l2=Gv_GetVarX(*insptr++);

                if (l2==0) {
                    OSD_Printf(CON_ERROR "Divide by zero.\n",g_errorLineNum,keyw[g_tw]);
                    continue;
                }
                Gv_SetVarX(j, Gv_GetVarX(j)/l2);
                continue;
            }

        case CON_MODVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                int32_t l2=Gv_GetVarX(*insptr++);

                if (l2==0) {
                    OSD_Printf(CON_ERROR "Mod by zero.\n",g_errorLineNum,keyw[g_tw]);
                    continue;
                }

                Gv_SetVarX(j, Gv_GetVarX(j) % l2);
                continue;
            }

        case CON_ANDVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(j) & Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_XORVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(j) ^ Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_ORVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(j) | Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_SUBVAR:
            insptr++;
            Gv_SetVarX(*insptr, Gv_GetVarX(*insptr) - *(insptr+1));
            insptr += 2;
            continue;

        case CON_SUBVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(j) - Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_ADDVAR:
            insptr++;
            Gv_SetVarX(*insptr, Gv_GetVarX(*insptr) + *(insptr+1));
            insptr += 2;
            continue;

        case CON_ADDVARVAR:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, Gv_GetVarX(j) + Gv_GetVarX(*insptr++));
            }
            continue;

        case CON_SHIFTVARL:
            insptr++;
            Gv_SetVarX(*insptr, Gv_GetVarX(*insptr) << *(insptr+1));
            insptr += 2;
            continue;

        case CON_SHIFTVARR:
            insptr++;
            Gv_SetVarX(*insptr, Gv_GetVarX(*insptr) >> *(insptr+1));
            insptr += 2;
            continue;

        case CON_SIN:
            insptr++;
            Gv_SetVarX(*insptr, sintable[Gv_GetVarX(*(insptr+1))&2047]);
            insptr += 2;
            continue;

        case CON_COS:
            insptr++;
            Gv_SetVarX(*insptr, sintable[(Gv_GetVarX(*(insptr+1))+512)&2047]);
            insptr += 2;
            continue;

        case CON_DISPLAYRAND:
            insptr++;
            Gv_SetVarX(*insptr++, rand());
            continue;

// *** other math
        case CON_CLAMP:
            insptr++;
            {
                int32_t var=*insptr++, min=Gv_GetVarX(*insptr++), max=Gv_GetVarX(*insptr++);
                int32_t val=Gv_GetVarX(var);

                if (val<min) Gv_SetVarX(var, min);
                else if (val>max) Gv_SetVarX(var, max);
            }
            continue;

        case CON_INV:
            Gv_SetVarX(*(insptr+1), -Gv_GetVarX(*(insptr+1)));
            insptr += 2;
            continue;

        case CON_SQRT:
            insptr++;
            {
                // syntax sqrt <invar> <outvar>
                int32_t lInVarID=*insptr++, lOutVarID=*insptr++;

                Gv_SetVarX(lOutVarID, ksqrt(Gv_GetVarX(lInVarID)));
                continue;
            }

        case CON_LDIST:
        case CON_DIST:
            insptr++;
            {
                int32_t distvar = *insptr++, xvar = Gv_GetVarX(*insptr++), yvar = Gv_GetVarX(*insptr++);

                if (xvar < 0 || xvar >= MAXSPRITES || sprite[xvar].statnum==MAXSTATUS) {
                    OSD_Printf(CON_ERROR "invalid sprite %d\n",g_errorLineNum,keyw[g_tw],xvar);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (yvar < 0 || yvar >= MAXSPRITES || sprite[yvar].statnum==MAXSTATUS) {
                    OSD_Printf(CON_ERROR "invalid sprite %d\n",g_errorLineNum,keyw[g_tw],yvar);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (vm.flags&VMFLAG_ERROR) continue;

                if (tw==CON_DIST)
                    Gv_SetVarX(distvar, dist(&sprite[xvar],&sprite[yvar]));
                else
                    Gv_SetVarX(distvar, ldist(&sprite[xvar],&sprite[yvar]));
                continue;
            }

        case CON_GETANGLE:
            insptr++;
            {
                int32_t angvar = *insptr++;
                int32_t xvar = Gv_GetVarX(*insptr++);
                int32_t yvar = Gv_GetVarX(*insptr++);

                Gv_SetVarX(angvar, getangle(xvar,yvar));
                continue;
            }

        case CON_GETINCANGLE:
            insptr++;
            {
                int32_t angvar = *insptr++;
                int32_t xvar = Gv_GetVarX(*insptr++);
                int32_t yvar = Gv_GetVarX(*insptr++);

                Gv_SetVarX(angvar, G_GetAngleDelta(xvar,yvar));
                continue;
            }

        case CON_MULSCALE:
            insptr++;
            {
                int32_t var1 = *insptr++, var2 = Gv_GetVarX(*insptr++);
                int32_t var3 = Gv_GetVarX(*insptr++), var4 = Gv_GetVarX(*insptr++);

                Gv_SetVarX(var1, mulscale(var2, var3, var4));
                continue;
            }

// *** if & while
        case CON_IFVARVARAND:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j &= Gv_GetVarX(*insptr++);
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVAROR:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j |= Gv_GetVarX(*insptr++);
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVARXOR:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j ^= Gv_GetVarX(*insptr++);
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVAREITHER:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                int32_t l = Gv_GetVarX(*insptr++);
                insptr--;
                X_DoConditional(j || l);
            }
            continue;

        case CON_IFVARVARBOTH:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                int32_t l = Gv_GetVarX(*insptr++);
                insptr--;
                X_DoConditional(j && l);
            }
            continue;

        case CON_IFVARVARN:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j = (j != Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVARE:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j = (j == Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVARG:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j = (j > Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVARGE:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j = (j >= Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVARL:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j = (j < Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARVARLE:
            insptr++;
            {
                int32_t j = Gv_GetVarX(*insptr++);
                j = (j <= Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            }
            continue;

        case CON_IFVARE:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j == *insptr);
            }
            continue;

        case CON_IFVARN:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j != *insptr);
            }
            continue;

        case CON_WHILEVARN: {
            instype *savedinsptr=insptr+2;
            int32_t j;
            do {
                insptr=savedinsptr;
                j = (Gv_GetVarX(*(insptr-1)) != *insptr);
                X_DoConditional(j);
            } while (j && !vm.flags);
            vm.flags &= ~VMFLAG_BREAK;
            continue;
        }

        case CON_WHILEVARL: {
            instype *savedinsptr=insptr+2;
            int32_t j;
            do {
                insptr=savedinsptr;
                j = (Gv_GetVarX(*(insptr-1)) < *insptr);
                X_DoConditional(j);
            } while (j && !vm.flags);
            vm.flags &= ~VMFLAG_BREAK;
            continue;
        }

        case CON_WHILEVARVARN: {
            int32_t j;
            instype *savedinsptr=insptr+2;
            do {
                insptr=savedinsptr;
                j = Gv_GetVarX(*(insptr-1));
                j = (j != Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            } while (j && !vm.flags);
            vm.flags &= ~VMFLAG_BREAK;
            continue;
        }

        case CON_WHILEVARVARL: {
            int32_t j;
            instype *savedinsptr=insptr+2;
            do {
                insptr=savedinsptr;
                j = Gv_GetVarX(*(insptr-1));
                j = (j < Gv_GetVarX(*insptr++));
                insptr--;
                X_DoConditional(j);
            } while (j && !vm.flags);
            vm.flags &= ~VMFLAG_BREAK;
            continue;
        }

        case CON_SORT:
            insptr++;
            {
                int32_t aridx=*insptr++, count=Gv_GetVarX(*insptr++), state=*insptr++;
                int32_t o_g_st=vm.g_st;
                instype *end=insptr;

                if (count<=0) continue;
                if (count > aGameArrays[aridx].size) {
                    OSD_Printf(CON_ERROR "Count of elements to sort (%d) exceeds array size (%d)!\n",g_errorLineNum,keyw[g_tw],count,aGameArrays[aridx].size);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

                if (state < 0) {
                    qsort(aGameArrays[aridx].vals, count, sizeof(int32_t), (int32_t(*)(const void*,const void*))X_DoSortDefault);
                } else {
                    x_sortingstateptr = script + statesinfo[state].ofs;
                    vm.g_st = 1+MAXEVENTS+state;
                    qsort(aGameArrays[aridx].vals, count, sizeof(int32_t), (int32_t(*)(const void*,const void*))X_DoSort);
                    vm.g_st = o_g_st;
                    insptr = end;
                }
            }
            continue;

        case CON_FOR:  // special-purpose iteration
            insptr++;
            {
                int32_t var = *insptr++, how=*insptr++, ii, jj;
                int32_t parm2 = how<=ITER_DRAWNSPRITES ? 0 : Gv_GetVarX(*insptr++);
                instype *end = insptr + *insptr, *beg = ++insptr;
                int32_t vm_i_bak = vm.g_i;
                spritetype *vm_sp_bak = vm.g_sp;
                int16_t endwall;

                if (vm.flags&VMFLAG_ERROR) continue;

                switch (how) {
                case ITER_ALLSPRITES:
                    for (jj=0; jj<MAXSPRITES && !vm.flags; jj++) {
                        if (sprite[jj].statnum == MAXSTATUS)
                            continue;
                        Gv_SetVarX(var, jj);
                        vm.g_i = jj;
                        vm.g_sp = &sprite[jj];
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_ALLSECTORS:
                    for (jj=0; jj<numsectors && !vm.flags; jj++) {
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_ALLWALLS:
                    for (jj=0; jj<numwalls && !vm.flags; jj++) {
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_SELSPRITES:
                    for (ii=0; ii<highlightcnt && !vm.flags; ii++) {
                        jj = highlight[ii];
                        if (jj&0xc000) {
                            jj &= (MAXSPRITES-1);
                            Gv_SetVarX(var, jj);
                            vm.g_i = jj;
                            vm.g_sp = &sprite[jj];
                            insptr = beg;
                            X_DoExecute(1);
                        }
                    }
                    break;
                case ITER_SELSECTORS:
                    for (ii=0; ii<highlightsectorcnt && !vm.flags; ii++) {
                        jj=highlightsector[ii];
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_SELWALLS:
                    for (ii=0; ii<highlightcnt && !vm.flags; ii++) {
                        jj=highlight[ii];
                        if (jj&0xc000)
                            continue;
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_DRAWNSPRITES:
                    for (ii=0; ii<spritesortcnt && !vm.flags; ii++) {
                        vm.g_sp = &tsprite[ii];
                        Gv_SetVarX(var, ii);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_SPRITESOFSECTOR:
                    if (parm2 < 0 || parm2 >= MAXSECTORS)
                        goto badindex;
                    for (jj=headspritesect[parm2]; jj>=0 && !vm.flags; jj=nextspritesect[jj]) {
                        Gv_SetVarX(var, jj);
                        vm.g_i = jj;
                        vm.g_sp = &sprite[jj];
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_WALLSOFSECTOR:
                    if (parm2 < 0 || parm2 >= MAXSECTORS)
                        goto badindex;
                    for (jj=sector[parm2].wallptr, endwall=jj+sector[parm2].wallnum-1;
                            jj<=endwall && !vm.flags; jj++) {
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                case ITER_LOOPOFWALL:
                    if (parm2 < 0 || parm2 >= numwalls)
                        goto badindex;
                    jj = parm2;
                    do {
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                        jj = wall[jj].point2;
                    } while (jj != parm2 && !vm.flags);
                    break;
                case ITER_RANGE:
                    for (jj=0; jj<parm2 && !vm.flags; jj++) {
                        Gv_SetVarX(var, jj);
                        insptr = beg;
                        X_DoExecute(1);
                    }
                    break;
                default:
                    OSD_Printf(CON_ERROR "Unknown iteration type %d!\n",g_errorLineNum,keyw[g_tw],how);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
badindex:
                    OSD_Printf(OSD_ERROR "Line %d, %s %s: index %d out of range!\n",g_errorLineNum,keyw[g_tw],
                               iter_tokens[how], parm2);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }
                vm.g_i = vm_i_bak;
                vm.g_sp = vm_sp_bak;
                vm.flags &= ~VMFLAG_BREAK;
                insptr = end;
            }
            continue;

        case CON_IFVARAND:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j & *insptr);
            }
            continue;

        case CON_IFVAROR:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j | *insptr);
            }
            continue;

        case CON_IFVARXOR:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j ^ *insptr);
            }
            continue;

        case CON_IFVAREITHER:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j || *insptr);
            }
            continue;

        case CON_IFVARBOTH:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j && *insptr);
            }
            continue;

        case CON_IFVARG:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j > *insptr);
            }
            continue;

        case CON_IFVARGE:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j >= *insptr);
            }
            continue;

        case CON_IFVARL:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j < *insptr);
            }
            continue;

        case CON_IFVARLE:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);
                X_DoConditional(j <= *insptr);
            }
            continue;

        case CON_IFRND:
            X_DoConditional(rnd(Gv_GetVarX(*(++insptr))));
            continue;

        case CON_IFHITKEY:
        case CON_IFHOLDKEY:
        case CON_RESETKEY:
            insptr++;
            {
                int32_t key=Gv_GetVarX(*insptr);
                if (key<0 || key >= (int32_t)(sizeof(keystatus)/sizeof(keystatus[0]))) {
                    OSD_Printf(CON_ERROR "Invalid key %d!\n",g_errorLineNum,keyw[g_tw],key);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

                if (tw != CON_RESETKEY)
                    X_DoConditional(keystatus[key]);
                else
                    insptr++;

                if (tw != CON_IFHOLDKEY) {
                    if (!(key==0 || key==KEYSC_ESC || key==KEYSC_TILDE || key==KEYSC_gENTER ||
                            key==KEYSC_LALT || key==KEYSC_RALT || key==KEYSC_LCTRL || key==KEYSC_RCTRL ||
                            key==KEYSC_LSHIFT || key==KEYSC_RSHIFT))
                        keystatus[key] = 0;
                }
            }
            continue;

        case CON_IFEITHERALT:
            X_DoConditional(keystatus[KEYSC_LALT]||keystatus[KEYSC_RALT]);
            continue;

        case CON_IFEITHERCTRL:
            X_DoConditional(keystatus[KEYSC_LCTRL]||keystatus[KEYSC_RCTRL]);
            continue;

        case CON_IFEITHERSHIFT:
            X_DoConditional(keystatus[KEYSC_LSHIFT]||keystatus[KEYSC_RSHIFT]);
            continue;

// vvv CURSPR
        case CON_IFSPRITEPAL:
            insptr++;
            X_ERROR_INVALIDSP();
            X_DoConditional(vm.g_sp->pal == Gv_GetVarX(*insptr));
            continue;

        case CON_IFANGDIFFL:
            insptr++;
            {
                int32_t j;
                X_ERROR_INVALIDSP();
                j = klabs(G_GetAngleDelta(ang, vm.g_sp->ang));
                X_DoConditional(j <= Gv_GetVarX(*insptr));
            }
            continue;

        case CON_IFAWAYFROMWALL: {
            int16_t s1;
            int32_t j = 0;

            X_ERROR_INVALIDSP();
            s1 = vm.g_sp->sectnum;
            updatesector(vm.g_sp->x+108,vm.g_sp->y+108,&s1);
            if (s1 == vm.g_sp->sectnum) {
                updatesector(vm.g_sp->x-108,vm.g_sp->y-108,&s1);
                if (s1 == vm.g_sp->sectnum) {
                    updatesector(vm.g_sp->x+108,vm.g_sp->y-108,&s1);
                    if (s1 == vm.g_sp->sectnum) {
                        updatesector(vm.g_sp->x-108,vm.g_sp->y+108,&s1);
                        if (s1 == vm.g_sp->sectnum)
                            j = 1;
                    }
                }
            }
            X_DoConditional(j);
        }
        continue;

        case CON_IFCANSEE: {
            int32_t j;

            X_ERROR_INVALIDSP();
            j = cansee(vm.g_sp->x,vm.g_sp->y,vm.g_sp->z/*-((krand()&41)<<8)*/,vm.g_sp->sectnum,
                       pos.x, pos.y, pos.z /*-((krand()&41)<<8)*/, cursectnum);
            X_DoConditional(j);
        }
        continue;

        case CON_IFONWATER:
            X_ERROR_INVALIDSP();
            X_DoConditional(sector[vm.g_sp->sectnum].lotag == 1 && klabs(vm.g_sp->z-sector[vm.g_sp->sectnum].floorz) < (32<<8));
            continue;

        case CON_IFINWATER:
            X_ERROR_INVALIDSP();
            X_DoConditional(sector[vm.g_sp->sectnum].lotag == 2);
            continue;

        case CON_IFACTOR:
            insptr++;
            X_ERROR_INVALIDSP();
            X_DoConditional(vm.g_sp->picnum == Gv_GetVarX(*insptr));
            continue;

        case CON_IFINSIDE:
            insptr++;
            {
                int32_t x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), sectnum=Gv_GetVarX(*insptr++), res;

                res = inside(x, y, sectnum);
                if (res == -1) {
                    OSD_Printf(CON_ERROR "Sector index %d out of range!\n",g_errorLineNum,keyw[g_tw],
                               sectnum);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }
                insptr--;
                X_DoConditional(res);
            }
            continue;

        case CON_IFOUTSIDE:
            X_ERROR_INVALIDSP();
            X_DoConditional(sector[vm.g_sp->sectnum].ceilingstat&1);
            continue;

        case CON_IFPDISTL:
            insptr++;
            {
                X_ERROR_INVALIDSP();
                X_DoConditional(dist((spritetype *)&pos, vm.g_sp) < Gv_GetVarX(*insptr));
            }
            continue;

        case CON_IFPDISTG:
            insptr++;
            {
                X_ERROR_INVALIDSP();
                X_DoConditional(dist((spritetype *)&pos, vm.g_sp) > Gv_GetVarX(*insptr));
            }
            continue;
// ^^^

// *** BUILD functions
        case CON_INSERTSPRITE:
            insptr++;
            {
                int32_t dasectnum = Gv_GetVarX(*insptr++), ret;

                X_ERROR_INVALIDSECT(dasectnum);
                if (numsprites >= MAXSPRITES) {
                    OSD_Printf(CON_ERROR "Maximum number of sprites reached.\n",g_errorLineNum,keyw[g_tw]);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

                ret = insertsprite(dasectnum, 0);
                vm.g_i = ret;
                vm.g_sp = &sprite[ret];
                numsprites++;
            }
            continue;

        case CON_DUPSPRITE:
        case CON_TDUPSPRITE:
            insptr++;
            {
                int32_t ospritenum = Gv_GetVarX(*insptr++), nspritenum;

                if (ospritenum<0 || ospritenum>=MAXSPRITES || sprite[ospritenum].statnum==MAXSTATUS) {
                    OSD_Printf(CON_ERROR "Tried to duplicate nonexistent sprite %d\n",g_errorLineNum,keyw[g_tw],ospritenum);
                    vm.flags |= VMFLAG_ERROR;
                }
                if ((tw==CON_DUPSPRITE && numsprites >= MAXSPRITES) ||
                        (tw==CON_DUPSPRITE && spritesortcnt >= MAXSPRITESONSCREEN)) {
                    OSD_Printf(CON_ERROR "Maximum number of sprites reached.\n",g_errorLineNum,keyw[g_tw]);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (vm.flags&VMFLAG_ERROR) continue;

                if (tw==CON_DUPSPRITE) {
                    nspritenum = insertsprite(sprite[ospritenum].sectnum, sprite[ospritenum].statnum);

                    if (nspritenum < 0) {
                        OSD_Printf(CON_ERROR "Internal error.\n",g_errorLineNum,keyw[g_tw]);
                        vm.flags |= VMFLAG_ERROR;
                        continue;
                    }

                    Bmemcpy(&sprite[nspritenum], &sprite[ospritenum], sizeof(spritetype));
                    vm.g_i = nspritenum;
                    vm.g_sp = &sprite[nspritenum];
                    numsprites++;
                } else {
                    Bmemcpy(&tsprite[spritesortcnt], &sprite[ospritenum], sizeof(spritetype));
                    tsprite[spritesortcnt].owner = ospritenum;
                    vm.g_i = -1;
                    vm.g_sp = &tsprite[spritesortcnt];
                    spritesortcnt++;
                }
            }
            continue;

        case CON_DELETESPRITE:
            insptr++;
            {
                int32_t daspritenum = Gv_GetVarX(*insptr++), ret;

                X_ERROR_INVALIDSPRI(daspritenum);
                ret = deletesprite(daspritenum);
                g_iReturnVar = ret;
                if (ret==0)
                    numsprites--;
            }
            continue;

        case CON_LASTWALL:
            insptr++;
            {
                int32_t dapoint = Gv_GetVarX(*insptr++), resvar=*insptr++;

                if (dapoint<0 || dapoint>=numwalls) {
                    OSD_Printf(CON_ERROR "Invalid wall %d\n",g_errorLineNum,keyw[g_tw],dapoint);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

                Gv_SetVarX(resvar, lastwall(dapoint));
            }
            continue;

        case CON_GETZRANGE:
            insptr++;
            {
                vec3_t vect;

                vect.x = Gv_GetVarX(*insptr++);
                vect.y = Gv_GetVarX(*insptr++);
                vect.z = Gv_GetVarX(*insptr++);

                {
                    int32_t sectnum=Gv_GetVarX(*insptr++);
                    int32_t ceilzvar=*insptr++, ceilhitvar=*insptr++, florzvar=*insptr++, florhitvar=*insptr++;
                    int32_t walldist=Gv_GetVarX(*insptr++), clipmask=Gv_GetVarX(*insptr++);
                    int32_t ceilz, ceilhit, florz, florhit;

                    X_ERROR_INVALIDSECT(sectnum);
                    getzrange(&vect, sectnum, &ceilz, &ceilhit, &florz, &florhit, walldist, clipmask);
                    Gv_SetVarX(ceilzvar, ceilz);
                    Gv_SetVarX(ceilhitvar, ceilhit);
                    Gv_SetVarX(florzvar, florz);
                    Gv_SetVarX(florhitvar, florhit);
                }
                continue;
            }

        case CON_HITSCAN:
            insptr++;
            {
                vec3_t vect;
                hitdata_t hitinfo;

                vect.x = Gv_GetVarX(*insptr++);
                vect.y = Gv_GetVarX(*insptr++);
                vect.z = Gv_GetVarX(*insptr++);

                {
                    int32_t sectnum=Gv_GetVarX(*insptr++);
                    int32_t vx=Gv_GetVarX(*insptr++), vy=Gv_GetVarX(*insptr++), vz=Gv_GetVarX(*insptr++);
                    int32_t hitsectvar=*insptr++, hitwallvar=*insptr++, hitspritevar=*insptr++;
                    int32_t hitxvar=*insptr++, hityvar=*insptr++, hitzvar=*insptr++, cliptype=Gv_GetVarX(*insptr++);

                    X_ERROR_INVALIDSECT(sectnum);
                    hitscan((const vec3_t *)&vect, sectnum, vx, vy, vz, &hitinfo, cliptype);
                    Gv_SetVarX(hitsectvar, hitinfo.hitsect);
                    Gv_SetVarX(hitwallvar, hitinfo.hitwall);
                    Gv_SetVarX(hitspritevar, hitinfo.hitsprite);
                    Gv_SetVarX(hitxvar, hitinfo.pos.x);
                    Gv_SetVarX(hityvar, hitinfo.pos.y);
                    Gv_SetVarX(hitzvar, hitinfo.pos.z);
                }
                continue;
            }

        case CON_CANSEE:
            insptr++;
            {
                int32_t x1=Gv_GetVarX(*insptr++), y1=Gv_GetVarX(*insptr++), z1=Gv_GetVarX(*insptr++);
                int32_t sect1=Gv_GetVarX(*insptr++);
                int32_t x2=Gv_GetVarX(*insptr++), y2=Gv_GetVarX(*insptr++), z2=Gv_GetVarX(*insptr++);
                int32_t sect2=Gv_GetVarX(*insptr++), rvar=*insptr++;

                X_ERROR_INVALIDSECT(sect1);
                X_ERROR_INVALIDSECT(sect2);

                Gv_SetVarX(rvar, cansee(x1,y1,z1,sect1,x2,y2,z2,sect2));
                continue;
            }

        case CON_ROTATEPOINT:
            insptr++;
            {
                int32_t xpivot=Gv_GetVarX(*insptr++), ypivot=Gv_GetVarX(*insptr++);
                int32_t x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), daang=Gv_GetVarX(*insptr++);
                int32_t x2var=*insptr++, y2var=*insptr++;
                int32_t x2, y2;

                rotatepoint(xpivot,ypivot,x,y,daang,&x2,&y2);
                Gv_SetVarX(x2var, x2);
                Gv_SetVarX(y2var, y2);
                continue;
            }

        case CON_NEARTAG:
            insptr++;
            {
                // neartag(int32_t x, int32_t y, int32_t z, short sectnum, short ang,  //Starting position & angle
                //         short *neartagsector,    //Returns near sector if sector[].tag != 0
                //         short *neartagwall,      //Returns near wall if wall[].tag != 0
                //         short *neartagsprite,    //Returns near sprite if sprite[].tag != 0
                //         int32_t *neartaghitdist, //Returns actual distance to object (scale: 1024=largest grid size)
                //         int32_t neartagrange,    //Choose maximum distance to scan (scale: 1024=largest grid size)
                //         char tagsearch)          //1-lotag only, 2-hitag only, 3-lotag&hitag

                int32_t x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++), z=Gv_GetVarX(*insptr++);
                int32_t sectnum=Gv_GetVarX(*insptr++), ang=Gv_GetVarX(*insptr++);
                int32_t neartagsectorvar=*insptr++, neartagwallvar=*insptr++, neartagspritevar=*insptr++, neartaghitdistvar=*insptr++;
                int32_t neartagrange=Gv_GetVarX(*insptr++), tagsearch=Gv_GetVarX(*insptr++);

                int16_t neartagsector, neartagwall, neartagsprite;
                int32_t neartaghitdist;

                X_ERROR_INVALIDSECT(sectnum);
                neartag(x, y, z, sectnum, ang, &neartagsector, &neartagwall, &neartagsprite, &neartaghitdist, neartagrange, tagsearch);

                Gv_SetVarX(neartagsectorvar, neartagsector);
                Gv_SetVarX(neartagwallvar, neartagwall);
                Gv_SetVarX(neartagspritevar, neartagsprite);
                Gv_SetVarX(neartaghitdistvar, neartaghitdist);
                continue;
            }

        case CON_BSETSPRITE:  // was CON_SETSPRITE
            insptr++;
            {
                int32_t spritenum = Gv_GetVarX(*insptr++);
                vec3_t davector;

                davector.x = Gv_GetVarX(*insptr++);
                davector.y = Gv_GetVarX(*insptr++);
                davector.z = Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSPRI(spritenum);
                setsprite(spritenum, &davector);
                continue;
            }

        case CON_GETFLORZOFSLOPE:
        case CON_GETCEILZOFSLOPE:
            insptr++;
            {
                int32_t sectnum = Gv_GetVarX(*insptr++), x = Gv_GetVarX(*insptr++), y = Gv_GetVarX(*insptr++);
                int32_t var=*insptr++;

                X_ERROR_INVALIDSECT(sectnum);
                if (tw == CON_GETFLORZOFSLOPE)
                    Gv_SetVarX(var, getflorzofslope(sectnum,x,y));
                else
                    Gv_SetVarX(var, getceilzofslope(sectnum,x,y));
                continue;
            }

        case CON_ALIGNFLORSLOPE:
        case CON_ALIGNCEILSLOPE:
            insptr++;
            {
                int32_t sectnum = Gv_GetVarX(*insptr++), x = Gv_GetVarX(*insptr++), y = Gv_GetVarX(*insptr++);
                int32_t z=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSECT(sectnum);
                if (tw == CON_ALIGNFLORSLOPE)
                    alignflorslope(sectnum, x,y,z);
                else
                    alignceilslope(sectnum, x,y,z);
                continue;
            }

// CURSPR
        case CON_SETFIRSTWALL:
            insptr++;
            {
                int32_t sect=Gv_GetVarX(*insptr++), wal=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSECT(sect);
                setfirstwall(sect, wal);
            }
            continue;

        case CON_UPDATECURSECTNUM:
            insptr++;
            updatesectorz(pos.x, pos.y, pos.z, &cursectnum);
            continue;

        case CON_UPDATESECTOR:
        case CON_UPDATESECTORZ:
            insptr++;
            {
                int32_t x=Gv_GetVarX(*insptr++), y=Gv_GetVarX(*insptr++);
                int32_t z=(tw==CON_UPDATESECTORZ)?Gv_GetVarX(*insptr++):0;
                int32_t var=*insptr++;
                int16_t w;

                X_ERROR_INVALIDCI();
                w=sprite[vm.g_i].sectnum;

                if (tw==CON_UPDATESECTOR) updatesector(x,y,&w);
                else updatesectorz(x,y,z,&w);

                Gv_SetVarX(var, w);
                continue;
            }

        case CON_HEADSPRITESTAT:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);
                if (j < 0 || j > MAXSTATUS) {
                    OSD_Printf(CON_ERROR "invalid status list %d\n",g_errorLineNum,keyw[g_tw],j);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }
                Gv_SetVarX(i,headspritestat[j]);
                continue;
            }

        case CON_PREVSPRITESTAT:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSPRI(j);
                Gv_SetVarX(i,prevspritestat[j]);
                continue;
            }

        case CON_NEXTSPRITESTAT:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSPRI(j);
                Gv_SetVarX(i,nextspritestat[j]);
                continue;
            }

        case CON_HEADSPRITESECT:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSECT(j);
                Gv_SetVarX(i,headspritesect[j]);
                continue;
            }

        case CON_PREVSPRITESECT:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSPRI(j);
                Gv_SetVarX(i,prevspritesect[j]);
                continue;
            }

        case CON_NEXTSPRITESECT:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSPRI(j);
                Gv_SetVarX(i,nextspritesect[j]);
                continue;
            }

        case CON_CANSEESPR:
            insptr++;
            {
                int32_t lVar1 = Gv_GetVarX(*insptr++), lVar2 = Gv_GetVarX(*insptr++), res;

                if (lVar1<0 || lVar1>=MAXSPRITES || sprite[lVar1].statnum==MAXSTATUS) {
                    OSD_Printf(CON_ERROR "Invalid sprite %d\n",g_errorLineNum,keyw[g_tw],lVar1);
                    vm.flags |= VMFLAG_ERROR;
                }
                if (lVar2<0 || lVar2>=MAXSPRITES || sprite[lVar2].statnum==MAXSTATUS) {
                    OSD_Printf(CON_ERROR "Invalid sprite %d\n",g_errorLineNum,keyw[g_tw],lVar2);
                    vm.flags |= VMFLAG_ERROR;
                }

                if (vm.flags&VMFLAG_ERROR) res=0;
                else res=cansee(sprite[lVar1].x,sprite[lVar1].y,sprite[lVar1].z,sprite[lVar1].sectnum,
                                    sprite[lVar2].x,sprite[lVar2].y,sprite[lVar2].z,sprite[lVar2].sectnum);

                Gv_SetVarX(*insptr++, res);
                continue;
            }

        case CON_CHANGESPRITESTAT:
        case CON_CHANGESPRITESECT:
            insptr++;
            {
                int32_t i = Gv_GetVarX(*insptr++);
                int32_t j = Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDSPRI(i);
                if (j<0 || j >= (tw==CON_CHANGESPRITESTAT?MAXSTATUS:numsectors)) {
                    OSD_Printf(CON_ERROR "Invalid %s: %d\n", tw==CON_CHANGESPRITESTAT?"statnum":"sector",
                               g_errorLineNum,keyw[g_tw],j);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

                if (tw == CON_CHANGESPRITESTAT) {
                    if (sprite[i].statnum == j) continue;
                    changespritestat(i,j);
                } else {
                    if (sprite[i].sectnum == j) continue;
                    changespritesect(i,j);
                }
                continue;
            }

        case CON_DRAGPOINT:
            insptr++;
            {
                int32_t wallnum = Gv_GetVarX(*insptr++), newx = Gv_GetVarX(*insptr++), newy = Gv_GetVarX(*insptr++);

                if (wallnum<0 || wallnum>=numwalls) {
                    OSD_Printf(CON_ERROR "Invalid wall %d\n",g_errorLineNum,keyw[g_tw],wallnum);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }
                dragpoint(wallnum,newx,newy);
                continue;
            }

        case CON_SECTOROFWALL:
            insptr++;
            {
                int32_t j = *insptr++;
                Gv_SetVarX(j, sectorofwall(Gv_GetVarX(*insptr++)));
            }
            continue;

        case CON_FIXREPEATS:
            insptr++;
            {
                extern void fixrepeats(int16_t i);
                fixrepeats(Gv_GetVarX(*insptr++));
            }
            continue;

// *** stuff
        case CON_GETTIMEDATE:
            insptr++;
            {
                int32_t v1=*insptr++,v2=*insptr++,v3=*insptr++,v4=*insptr++,v5=*insptr++,v6=*insptr++,v7=*insptr++,v8=*insptr++;
                time_t rawtime;
                struct tm * ti;

                time(&rawtime);
                ti = localtime(&rawtime);
                // initprintf("Time&date: %s\n",asctime (ti));

                Gv_SetVarX(v1, ti->tm_sec);
                Gv_SetVarX(v2, ti->tm_min);
                Gv_SetVarX(v3, ti->tm_hour);
                Gv_SetVarX(v4, ti->tm_mday);
                Gv_SetVarX(v5, ti->tm_mon);
                Gv_SetVarX(v6, ti->tm_year+1900);
                Gv_SetVarX(v7, ti->tm_wday);
                Gv_SetVarX(v8, ti->tm_yday);
                continue;
            }

        case CON_ADDLOG: {
            insptr++;

            OSD_Printf("L=%d\n", g_errorLineNum);
            continue;
        }

        case CON_ADDLOGVAR:
            insptr++;
            {
                char buf[80] = "", buf2[80] = "";
                int32_t code = (int32_t)*insptr, val = Gv_GetVarX(code);
                int32_t negate=code&(MAXGAMEVARS<<1);

                if (code & (0xFFFFFFFF-(MAXGAMEVARS-1))) {
                    char pp1[4][8] = {"sprite","sector","wall","tsprite"};
                    const memberlabel_t *pp2[4] = {SpriteLabels, SectorLabels, WallLabels, SpriteLabels};

                    if ((code&(MAXGAMEVARS<<2)) || (code&(MAXGAMEVARS<<3))) {
                        if (code&MAXGAMEVARS)
                            Bsprintf(buf2, "%d", (code>>16)&0xffff);
                        else
                            Bsprintf(buf2, "%s", aGameVars[(code>>16)&(MAXGAMEVARS-1)].szLabel?
                                     aGameVars[(code>>16)&(MAXGAMEVARS-1)].szLabel:"???");
                    }

                    if ((code&0x0000FFFC) == MAXGAMEVARS) { // addlogvar for a constant.. why not? :P
                        switch (code&3) {
                        case 0:
                            Bsprintf(buf, "(immediate constant)");
                            break;
                        case 1:
                            Bsprintf(buf, "(indirect constant)");
                            break;
                        case 2:
                            Bsprintf(buf, "(label constant)");
                            break;
                        default:
                            Bsprintf(buf, "(??? constant)");
                            break;
                        }
                    } else if (code&(MAXGAMEVARS<<2))
                        Bsprintf(buf, "%s[%s]", aGameArrays[code&(MAXGAMEARRAYS-1)].szLabel?
                                 aGameArrays[code&(MAXGAMEARRAYS-1)].szLabel:"???", buf2);
                    else if (code&(MAXGAMEVARS<<3))
                        Bsprintf(buf, "%s[%s].%s", pp1[code&3], buf2, pp2[code&3][(code>>2)&31].name);
                    else
                        Bsprintf(buf, "???");
                } else {
                    if (aGameVars[code].dwFlags & GAMEVAR_PERBLOCK) {
                        Bsprintf(buf2, "(%s", vm.g_st==0? "top-level) " : vm.g_st<=MAXEVENTS? "event" : "state");
                        if (vm.g_st >= 1+MAXEVENTS && vm.g_st <1+MAXEVENTS+g_stateCount)
                            Bsprintf(buf, " `%s') ", statesinfo[vm.g_st-1-MAXEVENTS].name);
                        else if (vm.g_st > 0)
                            Bsprintf(buf, " %d) ", vm.g_st-1);
                        Bstrcat(buf2, buf);
                    }

                    Bsprintf(buf, "%s%s", buf2, aGameVars[code].szLabel ? aGameVars[code].szLabel : "???");
                }

                OSD_Printf("L%d: %s%s=%d\n", g_errorLineNum, negate?"-":"", buf, val);

                insptr++;
                continue;
            }

        case CON_DEBUG:
            insptr++;
            initprintf("%d\n",*insptr++);
            continue;

// *** strings
        case CON_REDEFINEQUOTE:
            insptr++;
            {
                int32_t q = *insptr++, i = *insptr++;
                X_ERROR_INVALIDQUOTE(q, ScriptQuotes);
                X_ERROR_INVALIDQUOTE(i, ScriptQuoteRedefinitions);
                Bstrcpy(ScriptQuotes[q],ScriptQuoteRedefinitions[i]);
                continue;
            }

            insptr++;
            X_ERROR_INVALIDQUOTE(*insptr, ScriptQuotes);
            OSD_Printf("%s", ScriptQuotes[*insptr++]);
            continue;

        case CON_GETNUMBER16:
        case CON_GETNUMBER256:
            insptr++;
            {
                int32_t var=*insptr++, quote=Gv_GetVarX(*insptr++), max=Gv_GetVarX(*insptr++), sign=ksgn(max)<0?1:0;
                char buf[60];

                X_ERROR_INVALIDQUOTE(quote, ScriptQuotes);
                Bmemcpy(buf, ScriptQuotes[quote], sizeof(buf)-1);
                buf[sizeof(buf)-1]='\0';

                if (tw==CON_GETNUMBER16)
                    Gv_SetVarX(var, getnumber16(ScriptQuotes[quote], Gv_GetVarX(var), max, sign));
                else
                    Gv_SetVarX(var, getnumber256(ScriptQuotes[quote], Gv_GetVarX(var), max, sign));
            }
            continue;

        case CON_PRINT:
        case CON_QUOTE:
        case CON_ERRORINS:
        case CON_PRINTMESSAGE16:
        case CON_PRINTMESSAGE256:
        case CON_PRINTEXT256:
        case CON_PRINTEXT16:
            insptr++;
            {
                int32_t i=Gv_GetVarX(*insptr++);
                int32_t x=(tw>=CON_PRINTMESSAGE256)?Gv_GetVarX(*insptr++):0;
                int32_t y=(tw>=CON_PRINTMESSAGE256)?Gv_GetVarX(*insptr++):0;
                int32_t col=(tw>=CON_PRINTEXT256)?Gv_GetVarX(*insptr++):0;
                int32_t backcol=(tw>=CON_PRINTEXT256)?Gv_GetVarX(*insptr++):0;
                int32_t fontsize=(tw>=CON_PRINTEXT256)?Gv_GetVarX(*insptr++):0;

                if (tw==CON_ERRORINS) vm.flags |= VMFLAG_ERROR;

                X_ERROR_INVALIDQUOTE(i, ScriptQuotes);

                if (tw==CON_PRINT)
                    OSD_Printf("%s\n", ScriptQuotes[i]);
                if (tw==CON_QUOTE)
                    message("%s", ScriptQuotes[i]);
                else if (tw==CON_PRINTMESSAGE16)
                    printmessage16("%s", ScriptQuotes[i]);
                else if (tw==CON_PRINTMESSAGE256)
                    printmessage256(x, y, ScriptQuotes[i]);
                else if (tw==CON_PRINTEXT256) {
                    if (col<0 || col>=256) col=0;
                    if (backcol<0 || backcol>=256) backcol=-1;
                    printext256(x, y, col, backcol, ScriptQuotes[i], fontsize);
                } else if (tw==CON_PRINTEXT16) {
                    printext16(x, y, editorcolors[col&15], backcol<0 ? -1 : editorcolors[backcol&15],
                               ScriptQuotes[i], fontsize);
                }
            }
            continue;

        case CON_QSTRLEN:
            insptr++;
            {
                int32_t i=*insptr++;
                int32_t j=Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDQUOTE(j, ScriptQuotes);
                Gv_SetVarX(i, Bstrlen(ScriptQuotes[j]));
                continue;
            }

        case CON_QSUBSTR:
            insptr++;
            {
                int32_t q1 = Gv_GetVarX(*insptr++);
                int32_t q2 = Gv_GetVarX(*insptr++);
                int32_t st = Gv_GetVarX(*insptr++);
                int32_t ln = Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDQUOTE(q1, ScriptQuotes);
                X_ERROR_INVALIDQUOTE(q2, ScriptQuotes);

                {
                    char *s1 = ScriptQuotes[q1];
                    char *s2 = ScriptQuotes[q2];

                    while (*s2 && st--) s2++;
                    while ((*s1 = *s2) && ln--) {
                        s1++;
                        s2++;
                    }
                    *s1=0;
                }
                continue;
            }

        case CON_QSTRNCAT:
        case CON_QSTRCAT:
        case CON_QSTRCPY:
///        case CON_QGETSYSSTR:
            insptr++;
            {
                int32_t i = Gv_GetVarX(*insptr++);
                int32_t j = Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDQUOTE(i, ScriptQuotes);
                X_ERROR_INVALIDQUOTE(j, ScriptQuotes);

                switch (tw) {
#if 0
                case CON_QGETSYSSTR:
                    if (ScriptQuotes[i] == NULL) {
                        OSD_Printf(CON_ERROR "null quote %d %d\n",g_errorLineNum,keyw[g_tw],i,j);
                        break;
                    }
                    switch (j) {
                    case STR_MAPFILENAME:
                        Bstrcpy(ScriptQuotes[i], boardfilename);
                        break;
                    case STR_VERSION:
                        Bstrcpy(ScriptQuotes[i], "Mapster32"VERSION BUILDDATE);
                        break;
                    default:
                        OSD_Printf(CON_ERROR "unknown str ID %d %d\n",g_errorLineNum,keyw[g_tw],i,j);
                    }
                    break;
#endif
                case CON_QSTRCAT:
                    Bstrncat(ScriptQuotes[i],ScriptQuotes[j],(MAXQUOTELEN-1)-Bstrlen(ScriptQuotes[i]));
                    break;
                case CON_QSTRNCAT:
                    Bstrncat(ScriptQuotes[i],ScriptQuotes[j],Gv_GetVarX(*insptr++));
                    break;
                case CON_QSTRCPY:
                    Bstrcpy(ScriptQuotes[i],ScriptQuotes[j]);
                    break;
                }
                continue;
            }

        case CON_QSPRINTF:
            insptr++;
            {
                int32_t dq = Gv_GetVarX(*insptr++), sq = Gv_GetVarX(*insptr++);

                X_ERROR_INVALIDQUOTE(dq, ScriptQuotes);
                X_ERROR_INVALIDQUOTE(sq, ScriptQuotes);

                {
                    int32_t arg[32], numvals=0, i=0, j=0, k=0;
                    int32_t len = Bstrlen(ScriptQuotes[sq]);
                    char tmpbuf[MAXQUOTELEN<<1];

                    while (*insptr != -1 && numvals < 32)
                        arg[numvals++] = Gv_GetVarX(*insptr++);

                    insptr++; // skip the NOP

                    i = 0;
                    do {
                        while (k < len && j < MAXQUOTELEN && ScriptQuotes[sq][k] != '%')
                            tmpbuf[j++] = ScriptQuotes[sq][k++];

                        if (ScriptQuotes[sq][k] == '%') {
                            k++;

                            if (i>=numvals) goto dodefault;

                            switch (ScriptQuotes[sq][k]) {
                            case 'l':
                                if (ScriptQuotes[sq][k+1] != 'd') {
                                    // write the % and l
                                    tmpbuf[j++] = ScriptQuotes[sq][k-1];
                                    tmpbuf[j++] = ScriptQuotes[sq][k++];
                                    break;
                                }
                                k++;
                            case 'd': {
                                char buf[16];
                                int32_t ii = 0;

                                Bsprintf(buf, "%d", arg[i++]);

                                ii = Bstrlen(buf);
                                Bmemcpy(&tmpbuf[j], buf, ii);
                                j += ii;
                                k++;
                            }
                            break;

                            case 's': {
                                if (arg[i]>=0 && arg[i]<MAXQUOTES && ScriptQuotes[arg[i]]) {
                                    int32_t ii = Bstrlen(ScriptQuotes[arg[i]]);
                                    Bmemcpy(&tmpbuf[j], ScriptQuotes[arg[i]], ii);
                                    j += ii;
                                }
                                k++;
                            }
                            break;

dodefault:
                            default:
                                tmpbuf[j++] = ScriptQuotes[sq][k-1];
                                break;
                            }
                        }
                    } while (k < len && j < MAXQUOTELEN);

                    tmpbuf[j] = '\0';
                    Bmemcpy(ScriptQuotes[dq], tmpbuf, MAXQUOTELEN);
                    ScriptQuotes[dq][MAXQUOTELEN-1] = '\0';
                    continue;
                }
            }

// *** findnear*
// CURSPR vvv
        case CON_FINDNEARSPRITE:
        case CON_FINDNEARSPRITE3D:
        case CON_FINDNEARSPRITEVAR:
        case CON_FINDNEARSPRITE3DVAR:
            insptr++;
            {
                // syntax findnearactor(var) <type> <maxdist(var)> <getvar>
                // gets the sprite ID of the nearest actor within max dist
                // that is of <type> into <getvar>
                // -1 for none found
                // <type> <maxdist(varid)> <varid>
                int32_t lType=*insptr++;
                int32_t lMaxDist = (tw==CON_FINDNEARSPRITE || tw==CON_FINDNEARSPRITE3D)?
                                   *insptr++ : Gv_GetVarX(*insptr++);
                int32_t lVarID=*insptr++;
                int32_t lFound=-1, j, k = MAXSTATUS-1;

                X_ERROR_INVALIDCI();
                do {
                    j=headspritestat[k];    // all sprites
                    if (tw==CON_FINDNEARSPRITE3D || tw==CON_FINDNEARSPRITE3DVAR) {
                        while (j>=0) {
                            if (sprite[j].picnum == lType && j != vm.g_i && dist(&sprite[vm.g_i], &sprite[j]) < lMaxDist) {
                                lFound=j;
                                j = MAXSPRITES;
                                break;
                            }
                            j = nextspritestat[j];
                        }
                        if (j == MAXSPRITES)
                            break;
                        continue;
                    }

                    while (j>=0) {
                        if (sprite[j].picnum == lType && j != vm.g_i && ldist(&sprite[vm.g_i], &sprite[j]) < lMaxDist) {
                            lFound=j;
                            j = MAXSPRITES;
                            break;
                        }
                        j = nextspritestat[j];
                    }

                    if (j == MAXSPRITES)
                        break;
                } while (k--);
                Gv_SetVarX(lVarID, lFound);
                continue;
            }

        case CON_FINDNEARSPRITEZVAR:
        case CON_FINDNEARSPRITEZ:
            insptr++;
            {
                // syntax findnearactor(var) <type> <maxdist(var)> <getvar>
                // gets the sprite ID of the nearest actor within max dist
                // that is of <type> into <getvar>
                // -1 for none found
                // <type> <maxdist(varid)> <varid>
                int32_t lType=*insptr++;
                int32_t lMaxDist = (tw==CON_FINDNEARSPRITEZVAR) ? Gv_GetVarX(*insptr++) : *insptr++;
                int32_t lMaxZDist = (tw==CON_FINDNEARSPRITEZVAR) ? Gv_GetVarX(*insptr++) : *insptr++;
                int32_t lVarID=*insptr++;
                int32_t lFound=-1, lTemp, lTemp2, j, k=MAXSTATUS-1;

                X_ERROR_INVALIDCI();
                do {
                    j=headspritestat[k];    // all sprites
                    if (j == -1) continue;
                    do {
                        if (sprite[j].picnum == lType && j != vm.g_i) {
                            lTemp=ldist(&sprite[vm.g_i], &sprite[j]);
                            if (lTemp < lMaxDist) {
                                lTemp2=klabs(sprite[vm.g_i].z-sprite[j].z);
                                if (lTemp2 < lMaxZDist) {
                                    lFound=j;
                                    j = MAXSPRITES;
                                    break;
                                }
                            }
                        }
                        j = nextspritestat[j];
                    } while (j>=0);
                    if (j == MAXSPRITES)
                        break;
                } while (k--);
                Gv_SetVarX(lVarID, lFound);

                continue;
            }
// ^^^

        case CON_GETTICKS:
            insptr++;
            {
                int32_t j=*insptr++;
                Gv_SetVarX(j, getticks());
            }
            continue;

        case CON_SETASPECT:
            insptr++;
            {
                int32_t daxrange = Gv_GetVarX(*insptr++), dayxaspect = Gv_GetVarX(*insptr++);
                if (daxrange < (1<<12)) daxrange = (1<<12);
                if (daxrange > (1<<20)) daxrange = (1<<20);
                if (dayxaspect < (1<<12)) dayxaspect = (1<<12);
                if (dayxaspect > (1<<20)) dayxaspect = (1<<20);
                setaspect(daxrange, dayxaspect);
                continue;
            }

// vvv CURSPR
        case CON_SETI:
            insptr++;
            vm.g_i = Gv_GetVarX(*insptr++);
            X_ERROR_INVALIDCI();
            vm.g_sp = &sprite[vm.g_i];
            continue;

        case CON_SIZEAT:
            insptr++;
            X_ERROR_INVALIDSP();
            vm.g_sp->xrepeat = (uint8_t) Gv_GetVarX(*insptr++);
            vm.g_sp->yrepeat = (uint8_t) Gv_GetVarX(*insptr++);
            continue;

        case CON_CSTAT:
            insptr++;
            X_ERROR_INVALIDSP();
            vm.g_sp->cstat = (int16_t) *insptr++;
            continue;

        case CON_CSTATOR:
            insptr++;
            X_ERROR_INVALIDSP();
            vm.g_sp->cstat |= (int16_t) Gv_GetVarX(*insptr++);
            continue;

        case CON_CLIPDIST:
            insptr++;
            X_ERROR_INVALIDSP();
            vm.g_sp->clipdist = (int16_t) Gv_GetVarX(*insptr++);
            continue;

        case CON_SPRITEPAL:
            insptr++;
            X_ERROR_INVALIDSP();
            vm.g_sp->pal = Gv_GetVarX(*insptr++);
            continue;

        case CON_CACTOR:
            insptr++;
            X_ERROR_INVALIDSP();
            vm.g_sp->picnum = Gv_GetVarX(*insptr++);
            continue;

        case CON_SPGETLOTAG:
            insptr++;
            X_ERROR_INVALIDSP();
            Gv_SetVarX(g_iLoTagID, vm.g_sp->lotag);
            continue;

        case CON_SPGETHITAG:
            insptr++;
            X_ERROR_INVALIDSP();
            Gv_SetVarX(g_iHiTagID, vm.g_sp->hitag);
            continue;

        case CON_SECTGETLOTAG:
            insptr++;
            X_ERROR_INVALIDSP();
            Gv_SetVarX(g_iLoTagID, sector[vm.g_sp->sectnum].lotag);
            continue;

        case CON_SECTGETHITAG:
            insptr++;
            X_ERROR_INVALIDSP();
            Gv_SetVarX(g_iHiTagID, sector[vm.g_sp->sectnum].hitag);
            continue;

        case CON_GETTEXTUREFLOOR:
            insptr++;
            X_ERROR_INVALIDSP();
            Gv_SetVarX(g_iTextureID, sector[vm.g_sp->sectnum].floorpicnum);
            continue;

        case CON_GETTEXTURECEILING:
            insptr++;
            X_ERROR_INVALIDSP();
            Gv_SetVarX(g_iTextureID, sector[vm.g_sp->sectnum].ceilingpicnum);
            continue;
// ^^^
        case CON_DRAWLINE16:
        case CON_DRAWLINE16B:
            insptr++;
            {
                int32_t x1=Gv_GetVarX(*insptr++), y1=Gv_GetVarX(*insptr++);
                int32_t x2=Gv_GetVarX(*insptr++), y2=Gv_GetVarX(*insptr++);
                int32_t col=Gv_GetVarX(*insptr++), odrawlinepat=drawlinepat;
                int32_t xofs=0, yofs=0;

                if (tw==CON_DRAWLINE16B) {
                    x1 = mulscale14(x1-pos.x,zoom);
                    y1 = mulscale14(y1-pos.y,zoom);
                    x2 = mulscale14(x2-pos.x,zoom);
                    y2 = mulscale14(y2-pos.y,zoom);
                    xofs = halfxdim16;
                    yofs = midydim16;
                }

                drawlinepat = m32_drawlinepat;
                drawline16(xofs+x1,yofs+y1, xofs+x2,yofs+y2, editorcolors[col&15]);
                drawlinepat = odrawlinepat;
                continue;
            }

        case CON_DRAWCIRCLE16:
        case CON_DRAWCIRCLE16B:
            insptr++;
            {
                int32_t x1=Gv_GetVarX(*insptr++), y1=Gv_GetVarX(*insptr++);
                int32_t r=Gv_GetVarX(*insptr++);
                int32_t col=Gv_GetVarX(*insptr++), odrawlinepat=drawlinepat;
                int32_t xofs=0, yofs=0;

                if (tw==CON_DRAWCIRCLE16B) {
                    x1 = mulscale14(x1-pos.x,zoom);
                    y1 = mulscale14(y1-pos.y,zoom);
                    r = mulscale14(r,zoom);
                    xofs = halfxdim16;
                    yofs = midydim16;
                }

                drawlinepat = m32_drawlinepat;
                drawcircle16(xofs+x1, yofs+y1, r, editorcolors[col&15]);
                drawlinepat = odrawlinepat;
                continue;
            }

        case CON_ROTATESPRITE16:
        case CON_ROTATESPRITE:
            insptr++;
            {
                int32_t x=Gv_GetVarX(*insptr++),   y=Gv_GetVarX(*insptr++),           z=Gv_GetVarX(*insptr++);
                int32_t a=Gv_GetVarX(*insptr++),   tilenum=Gv_GetVarX(*insptr++),     shade=Gv_GetVarX(*insptr++);
                int32_t pal=Gv_GetVarX(*insptr++), orientation=Gv_GetVarX(*insptr++);
                int32_t x1=Gv_GetVarX(*insptr++),  y1=Gv_GetVarX(*insptr++);
                int32_t x2=Gv_GetVarX(*insptr++),  y2=Gv_GetVarX(*insptr++);

                if (tw == CON_ROTATESPRITE && !(orientation & 256)) {
                    x<<=16;
                    y<<=16;
                }
                rotatesprite(x,y,z,a,tilenum,shade,pal,2|orientation,x1,y1,x2,y2);
                continue;
            }

        case CON_SETGAMEPALETTE:
            insptr++;
            switch (Gv_GetVarX(*insptr++)) {
            default:
            case 0:
                SetGAMEPalette();
                break;
            case 1:
                SetWATERPalette();
                break;
            case 2:
                SetSLIMEPalette();
                break;
            case 3:
                SetBOSS1Palette();
                break;
            }
            continue;

// *** sounds
        case CON_IFSOUND:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr);
                if (j<0 || j>=MAXSOUNDS) {
                    OSD_Printf(CON_ERROR "Invalid sound %d\n",g_errorLineNum,keyw[g_tw],j);
                    vm.flags |= VMFLAG_ERROR;
                    insptr++;
                    continue;
                }
                X_DoConditional(S_CheckSoundPlaying(vm.g_i,j));
            }
            continue;

        case CON_IFNOSOUNDS: {
            int32_t j = MAXSOUNDS-1;
            for (; j>=0; j--)
                if (g_sounds[j].SoundOwner[0].i == vm.g_i)
                    break;

            X_DoConditional(j < 0);
        }
        continue;

        case CON_GETSOUNDFLAGS:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++), var=*insptr++;
                if (j<0 || j>=MAXSOUNDS) {
                    OSD_Printf(CON_ERROR "Invalid sound %d\n",g_errorLineNum,keyw[g_tw],j);
                    vm.flags |= VMFLAG_ERROR;
                    insptr++;
                    continue;
                }

                Gv_SetVarX(var, g_sounds[j].m);
            }
            continue;

        case CON_SOUNDVAR:
        case CON_STOPSOUNDVAR:
        case CON_SOUNDONCEVAR:
        case CON_GLOBALSOUNDVAR:
            insptr++;
            {
                int32_t j=Gv_GetVarX(*insptr++);

                if (j<0 || j>=MAXSOUNDS) {
                    OSD_Printf(CON_ERROR "Invalid sound %d\n",g_errorLineNum,keyw[g_tw],j);
                    vm.flags |= VMFLAG_ERROR;
                    continue;
                }

                switch (tw) {
                case CON_SOUNDONCEVAR:
                    if (!S_CheckSoundPlaying(vm.g_i,j))
                        A_PlaySound((int16_t)j,vm.g_i);
                    break;
                case CON_GLOBALSOUNDVAR:
                    A_PlaySound((int16_t)j,-1);
                    break;
                case CON_STOPSOUNDVAR:
                    if (S_CheckSoundPlaying(vm.g_i,j))
                        S_StopSound((int16_t)j);
                    break;
                case CON_SOUNDVAR:
                    A_PlaySound((int16_t)j,vm.g_i);
                    break;
                }
            }
            continue;

        case CON_STOPALLSOUNDS:
            insptr++;
            FX_StopAllSounds();
            continue;

        default:
            X_ScriptInfo();

            OSD_Printf("\nAn error has occurred in the Mapster32 virtual machine.\n\n"
                       "Please e-mail the file mapster32.log along with every M32 file\n"
                       "you're using and instructions how to reproduce this error to\n"
                       "helixhorned@gmail.com.\n\n"
                       "Thank you!");
            vm.flags |= VMFLAG_ERROR;
            return 1;
        }
    }

    return 0;
}
