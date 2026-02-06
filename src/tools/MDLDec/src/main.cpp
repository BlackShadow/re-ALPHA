/***
*
*    Copyright (c) 1996-1997, Valve LLC. All rights reserved.
*
*    This product contains software technology licensed from Id
*    Software, Inc ("Id Technology"). Id Technology (c) 1996 Id Software, Inc.
*    All Rights Reserved.
*
****/
//
// main.cpp: Half-Life Alpha MDL Decompiler.
//

#include "mdldec.h"

int main(int argc, char *argv[])
{
    CMDLDecompiler *mdl = new CMDLDecompiler;

    printf("\nHalf-Life Alpha MDL Decompiler v%s.\n", MDLDEC_VERSION);
    printf("Format: studio model v%d (IDST).\n", STUDIO_VERSION_ALPHA);
    printf("--------------------------------------------------\n");

    if (argc == 1)
    {
        printf("Usage: mdldec_alpha.exe <path\\mdlfile.mdl> [<destpath>]\n");
        printf("--------------------------------------------------\n");
        delete mdl;
        return 1;
    }

    if (argc >= 3)
        strcpy(mdl->DestPath, argv[2]);

    if (mdl->LoadModel(argv[1]))
    {
        mdl->FixRepeatedSequenceNames();
        mdl->QC_GenerateScript();
        mdl->SMD_GenerateReferences();
        mdl->SMD_GenerateSequences();
        mdl->BMP_GenerateTextures();
        mdl->LogMessage(MDLDEC_MSG_INFO, "Done.\r\n");
    }

    printf("--------------------------------------------------\n");

    delete mdl;
    return 0;
}
