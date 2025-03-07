/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements TigerAltName, providing access to RT4 files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

#include <cinttypes>

static const char FOUR_FILE_CODE[] = "4";

static const TigerFieldInfo rt4_fields[] = {
    // fieldname    fmt  type  OFTType         beg  end  len  bDefine bSet
    {"MODULE", ' ', ' ', OFTString, 0, 0, 8, 1, 0},
    {"TLID", 'R', 'N', OFTInteger, 6, 15, 10, 1, 1},
    {"RTSQ", 'R', 'N', OFTInteger, 16, 18, 3, 1, 1},
    {"FEAT", ' ', ' ', OFTIntegerList, 0, 0, 8, 1, 0}
    // Note: we don't mention the FEAT1, FEAT2, FEAT3, FEAT4, FEAT5 fields
    // here because they're handled separately in the code below; they
    // correspond
    // to the FEAT array field here.
};

static const TigerRecordInfo rt4_info = {
    rt4_fields, sizeof(rt4_fields) / sizeof(TigerFieldInfo), 58};

/************************************************************************/
/*                            TigerAltName()                            */
/************************************************************************/

TigerAltName::TigerAltName(OGRTigerDataSource *poDSIn,
                           CPL_UNUSED const char *pszPrototypeModule)
    : TigerFileBase(&rt4_info, FOUR_FILE_CODE)
{
    OGRFieldDefn oField("", OFTInteger);

    poDS = poDSIn;
    poFeatureDefn = new OGRFeatureDefn("AltName");
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    /* -------------------------------------------------------------------- */
    /*      Fields from type 4 record.                                      */
    /* -------------------------------------------------------------------- */

    AddFieldDefns(psRTInfo, poFeatureDefn);
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *TigerAltName::GetFeature(int nRecordId)

{
    char achRecord[OGR_TIGER_RECBUF_LEN];

    if (nRecordId < 0 || nRecordId >= nFeatures)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Request for out-of-range feature %d of %s4", nRecordId,
                 pszModule);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read the raw record data from the file.                         */
    /* -------------------------------------------------------------------- */
    if (fpPrimary == nullptr)
        return nullptr;

    const auto nOffset = static_cast<uint64_t>(nRecordId) * nRecordLength;
    if (VSIFSeekL(fpPrimary, nOffset, SEEK_SET) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to seek to %" PRIu64 " of %s4", nOffset, pszModule);
        return nullptr;
    }

    // Overflow cannot happen since psRTInfo->nRecordLength is unsigned
    // char and sizeof(achRecord) == OGR_TIGER_RECBUF_LEN > 255
    if (VSIFReadL(achRecord, psRTInfo->nRecordLength, 1, fpPrimary) != 1)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Failed to read record %d of %s4",
                 nRecordId, pszModule);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Set fields.                                                     */
    /* -------------------------------------------------------------------- */

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    int anFeatList[5] = {0};
    int nFeatCount = 0;

    SetFields(psRTInfo, poFeature, achRecord);

    for (int iFeat = 0; iFeat < 5; iFeat++)
    {
        const char *pszFieldText =
            GetField(achRecord, 19 + iFeat * 8, 26 + iFeat * 8);

        if (*pszFieldText != '\0')
            anFeatList[nFeatCount++] = atoi(pszFieldText);
    }

    poFeature->SetField("FEAT", nFeatCount, anFeatList);

    return poFeature;
}
