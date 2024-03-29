/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbLexRoutines.c	*/
/* Author:  Marty Kraimer Date:    13JUL95*/

/*The routines in this module are serially reusable NOT reentrant*/

#ifdef vxWorks
#include <vxWorks.h>
#endif
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dbmf.h"

#include "dbDefs.h"
#include "dbFldTypes.h"
#include "epicsPrint.h"
#include "errMdef.h"
#include "ellLib.h"
#include "gpHash.h"
#include "freeList.h"
#include "guigroup.h"
#include "special.h"
#include "link.h"
#include "macLib.h"

#define epicsExportSharedSymbols
#include "shareLib.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"


/*private routines */
static void yyerrorAbort(char *str);
static void allocTemp(void *pvoid);
static void *popFirstTemp(void);
static void *getLastTemp(void);
static int db_yyinput(char *buf,int max_size);
static void dbIncludePrint(FILE *fp);
static void dbPathCmd(char *path);
static void dbAddPathCmd(char *path);
static void dbIncludeNew(char *include_file);
static void dbMenuHead(char *name);
static void dbMenuChoice(char *name,char *value);
static void dbMenuBody(void);

static void dbRecordtypeHead(char *name);
static void dbRecordtypeBody(void);
static void dbRecordtypeFieldHead(char *name,char *type);
static void dbRecordtypeFieldItem(char *name,char *value);

static void dbDevice(char *recordtype,char *linktype,
	char *dsetname,char *choicestring);
static void dbDriver(char *name);

static void dbBreakHead(char *name);
static void dbBreakItem(char *value);
static void dbBreakBody(void);

static void dbRecordHead(char *recordType,char*name,int visible);
static void dbRecordField(char *name,char *value);
static void dbRecordBody(void);

/*private declarations*/
#define MY_BUFFER_SIZE 1024
static char *my_buffer=NULL;
static char *mac_input_buffer=NULL;
static char *my_buffer_ptr=NULL;
static MAC_HANDLE *macHandle = NULL;
typedef struct inputFile{
	ELLNODE		node;
	char		*path;
	char		*filename;
	FILE		*fp;
	int		line_num;
}inputFile;
static ELLLIST inputFileList;

static inputFile *pinputFileNow = NULL;
static DBBASE *pdbbase = NULL;

typedef struct tempListNode {
	ELLNODE	node;
	void	*item;
}tempListNode;

static ELLLIST tempList;
static void *freeListPvt = NULL;
static int duplicate = FALSE;

static char *strduplicate(const char * str)
{
    char *tstr;
    tstr = dbCalloc(strlen(str) + 1,sizeof(char));
    strcpy(tstr,str);
    return(tstr);
}

static void yyerrorAbort(char *str)
{
    yyerror(str);
    yyAbort = TRUE;
}

static void allocTemp(void *pvoid)
{
    tempListNode	*ptempListNode;

    ptempListNode = freeListCalloc(freeListPvt);
    ptempListNode->item = pvoid;
    ellAdd(&tempList,&ptempListNode->node);
}

static void *popFirstTemp(void)
{
    tempListNode	*ptempListNode;
    void		*ptemp;

    ptempListNode = (tempListNode *)ellFirst(&tempList);
    ptemp = ptempListNode->item;
    ellDelete(&tempList,(ELLNODE *)ptempListNode);
    freeListFree(freeListPvt,ptempListNode);
    return(ptemp);
}

static void *getLastTemp(void)
{
    tempListNode	*ptempListNode;

    ptempListNode = (tempListNode *)ellLast(&tempList);
    return(ptempListNode->item);
}

static char *dbOpenFile(DBBASE *pdbbase,const char *filename,FILE **fp)
{
    ELLLIST	*ppathList = (ELLLIST *)pdbbase->pathPvt;
    dbPathNode	*pdbPathNode;
    char	*fullfilename;

    *fp = 0;
    if(!filename) return(0);
    if(!ppathList || (ellCount(ppathList)==0) 
    || strchr(filename,'/') || strchr(filename,'\\')){
	*fp = fopen(filename,"r");
	return(0);
    }
    pdbPathNode = (dbPathNode *)ellFirst(ppathList);
    while(pdbPathNode) {
	fullfilename = dbCalloc(strlen(filename)+strlen(pdbPathNode->directory)
	    +2,sizeof(char));
	strcpy(fullfilename,pdbPathNode->directory);
	strcat(fullfilename,"/");
	strcat(fullfilename,filename);
	*fp = fopen(fullfilename,"r");
	free((void *)fullfilename);
	if(*fp) return(pdbPathNode->directory);
	pdbPathNode = (dbPathNode *)ellNext(&pdbPathNode->node);
    }
    return(0);
}


static void freeInputFileList(void)
{
    inputFile *pinputFileNow;

    while((pinputFileNow=(inputFile *)ellFirst(&inputFileList))) {
	if(fclose(pinputFileNow->fp)) 
	    errPrintf(0,__FILE__, __LINE__,
			"Closing file %s",pinputFileNow->filename);
	free((void *)pinputFileNow->filename);
	ellDelete(&inputFileList,(ELLNODE *)pinputFileNow);
	free((void *)pinputFileNow);
    }
}

static long dbReadCOM(DBBASE **ppdbbase,const char *filename, FILE *fp,
	const char *path,const char *substitutions)
{
    long	status;
    inputFile	*pinputFile = NULL;
    char	*penv;
    char	**macPairs;
    
    if(*ppdbbase == 0) *ppdbbase = dbAllocBase();
    pdbbase = *ppdbbase;
    if(path && strlen(path)>0) {
	dbPath(pdbbase,path);
    } else {
	penv = getenv("EPICS_DB_INCLUDE_PATH");
	if(penv) {
	    dbPath(pdbbase,penv);
	} else {
	    dbPath(pdbbase,".");
	}
    }
    my_buffer = dbCalloc(MY_BUFFER_SIZE,sizeof(char));
    macHandle = NULL;
    if(substitutions) {
	if(macCreateHandle(&macHandle,NULL)) {
	    epicsPrintf("macCreateHandle error\n");
	    return(-1);
	}
	macParseDefns(macHandle,(char *)substitutions,&macPairs);
	if(macPairs ==NULL) {
	    macDeleteHandle(macHandle);
	    macHandle = NULL;
	} else {
	    macInstallMacros(macHandle,macPairs);
	    free((void *)macPairs);
	    mac_input_buffer = dbCalloc(MY_BUFFER_SIZE,sizeof(char));
	}
    }
    ellInit(&inputFileList);
    ellInit(&tempList);
    freeListInitPvt(&freeListPvt,sizeof(tempListNode),100);
    pinputFile = dbCalloc(1,sizeof(inputFile));
    if(filename) {
	pinputFile->filename = strduplicate(filename);
    }
    if(!fp) {
	FILE	*fp;

	if(filename) pinputFile->path = dbOpenFile(pdbbase,filename,&fp);
	if(!filename || !fp) {
	    errPrintf(0,__FILE__, __LINE__,
		"dbRead opening file %s",filename);
	    free((void *)my_buffer);
	    freeListCleanup(freeListPvt);
	    free((void *)pinputFile);
	    return(-1);
	}
	pinputFile->fp = fp;
    } else {
	pinputFile->fp = fp;
    }
    pinputFile->line_num = 0;
    pinputFileNow = pinputFile;
    my_buffer[0] = '\0';
    my_buffer_ptr = my_buffer;
    ellAdd(&inputFileList,&pinputFile->node);
    status = pvt_yy_parse();
    if(status) {
	fprintf(stderr,"db_parse returned %ld\n",status);
    }
    if(macHandle) {
	macDeleteHandle(macHandle);
	macHandle = NULL;
	free((void *)mac_input_buffer);
    }
    freeListCleanup(freeListPvt);
    free((void *)my_buffer);
    freeInputFileList();
    dbFreePath(pdbbase);
    if(!status) { /*add RTYP and VERS as an attribute */
	DBENTRY	dbEntry;
	DBENTRY	*pdbEntry = &dbEntry;
	long	localStatus;

	dbInitEntry(pdbbase,pdbEntry);
	localStatus = dbFirstRecordType(pdbEntry);
	while(!localStatus) {
	    localStatus = dbPutRecordAttribute(pdbEntry,"RTYP",
		dbGetRecordTypeName(pdbEntry));
	    if(!localStatus)  {
		localStatus = dbPutRecordAttribute(pdbEntry,"VERS",
		    "none specified");
	    }
	    if(localStatus) {
		fprintf(stderr,"dbPutRecordAttribute status %ld\n",status);
	    } else {
	        localStatus = dbNextRecordType(pdbEntry);
	    }
	}
	dbFinishEntry(pdbEntry);
    }
    return(status);
}

long epicsShareAPI dbReadDatabase(DBBASE **ppdbbase,const char *filename,
	const char *path,const char *substitutions)
{return (dbReadCOM(ppdbbase,filename,0,path,substitutions));}

long epicsShareAPI dbReadDatabaseFP(DBBASE **ppdbbase,FILE *fp,
	const char *path,const char *substitutions)
{return (dbReadCOM(ppdbbase,0,fp,path,substitutions));}

static int db_yyinput(char *buf, int max_size)
{
    int		l,n;
    char	*fgetsRtn;
    
    if(yyAbort) return(0);
    if(*my_buffer_ptr==0) {
	while(TRUE) { /*until we get some input*/
	    if(macHandle) {
		fgetsRtn = fgets(mac_input_buffer,MY_BUFFER_SIZE,
			pinputFileNow->fp);
		if(fgetsRtn) {
		    n = macExpandString(macHandle,mac_input_buffer,
			my_buffer,MY_BUFFER_SIZE-1);
		    if(n<0) {
			errPrintf(0,__FILE__, __LINE__,
			"macExpandString failed for file %s",
			pinputFileNow->filename);
		    }
		}
	    } else {
		fgetsRtn = fgets(my_buffer,MY_BUFFER_SIZE,pinputFileNow->fp);
	    }
	    if(fgetsRtn) break;
	    if(fclose(pinputFileNow->fp)) 
		errPrintf(0,__FILE__, __LINE__,
			"Closing file %s",pinputFileNow->filename);
	    free((void *)pinputFileNow->filename);
	    ellDelete(&inputFileList,(ELLNODE *)pinputFileNow);
	    free((void *)pinputFileNow);
	    pinputFileNow = (inputFile *)ellLast(&inputFileList);
	    if(!pinputFileNow) return(0);
	}
	if(dbStaticDebug) fprintf(stderr,"%s",my_buffer);
	pinputFileNow->line_num++;
	my_buffer_ptr = &my_buffer[0];
    }
    l = strlen(my_buffer_ptr);
    n = (l<=max_size ? l : max_size);
    memcpy(buf,my_buffer_ptr,n);
    my_buffer_ptr += n;
    return(n);
}

static void dbIncludePrint(FILE *fp)
{
    inputFile *pinputFile = pinputFileNow;

    fprintf(fp,"input line: %s",my_buffer);
    while(pinputFile) {
	if(pinputFile->filename) {
	    fprintf(fp,"   in:");
	    if(pinputFile->path)
	        fprintf(fp," path \"%s\" ",pinputFile->path);
	    fprintf(fp," file %s",pinputFile->filename);
	} else {
	    fprintf(fp,"stdin:");
	    if(pinputFile->path)
	        fprintf(fp," path \"%s\" ",pinputFile->path);
	}
	fprintf(fp," line %d\n",pinputFile->line_num);
	pinputFile = (inputFile *)ellPrevious(&pinputFile->node);
    }
    fprintf(fp,"\n");
    return;
}

static void dbPathCmd(char *path)
{
    dbPath(pdbbase,path);
}

static void dbAddPathCmd(char *path)
{
    dbAddPath(pdbbase,path);
}

static void dbIncludeNew(char *filename)
{
    inputFile	*pinputFile;
    FILE	*fp;

    pinputFile = dbCalloc(1,sizeof(inputFile));
    pinputFile->path = dbOpenFile(pdbbase,filename,&fp);
    if(!fp) {
	errPrintf(0,__FILE__, __LINE__,
		"dbIncludeNew opening file %s",filename);
	yyerror(NULL);
	free((void *)pinputFile);
	return;
    }
    pinputFile->filename = strduplicate(filename);
    pinputFile->fp = fp;
    ellAdd(&inputFileList,&pinputFile->node);
    pinputFileNow = pinputFile;
}

static void dbMenuHead(char *name)
{
    dbMenu		*pdbMenu;
    GPHENTRY		*pgphentry;

    pgphentry = gphFind(pdbbase->pgpHash,name,&pdbbase->menuList);
    if(pgphentry) {
	duplicate = TRUE;
	return;
    }
    if(ellCount(&tempList)) yyerrorAbort("dbMenuHead: tempList not empty");
    pdbMenu = dbCalloc(1,sizeof(dbMenu));
    pdbMenu->name = strduplicate(name);
    allocTemp(pdbMenu);
}

static void dbMenuChoice(char *name,char *value)
{
    if(duplicate) return;
    allocTemp(strduplicate(name));
    allocTemp(strduplicate(value));
}

static void dbMenuBody(void)
{
    dbMenu		*pnewMenu;
    dbMenu		*pMenu;
    int			nChoice;
    int			i;
    GPHENTRY		*pgphentry;

    if(duplicate) {
	duplicate = FALSE;
	return;
    }
    pnewMenu = (dbMenu *)popFirstTemp();
    pnewMenu->nChoice = nChoice = ellCount(&tempList)/2;
    pnewMenu->papChoiceName = dbCalloc(pnewMenu->nChoice,sizeof(char *));
    pnewMenu->papChoiceValue = dbCalloc(pnewMenu->nChoice,sizeof(char *));
    for(i=0; i<nChoice; i++) {
	pnewMenu->papChoiceName[i] = (char *)popFirstTemp();
	pnewMenu->papChoiceValue[i] = (char *)popFirstTemp();
    }
    if(ellCount(&tempList)) yyerrorAbort("dbMenuBody: tempList not empty");
    /* Add menu in sorted order */
    pMenu = (dbMenu *)ellFirst(&pdbbase->menuList);
    while(pMenu && strcmp(pMenu->name,pnewMenu->name) >0 )
	pMenu = (dbMenu *)ellNext(&pMenu->node);
    if(pMenu)
	ellInsert(&pdbbase->menuList,ellPrevious(&pMenu->node),&pnewMenu->node);
    else
	ellAdd(&pdbbase->menuList,&pnewMenu->node);
    pgphentry = gphAdd(pdbbase->pgpHash,pnewMenu->name,&pdbbase->menuList);
    if(!pgphentry) {
	yyerrorAbort("gphAdd failed");
    } else {
	pgphentry->userPvt = pnewMenu;
    }
}

static void dbRecordtypeHead(char *name)
{
    dbRecordType		*pdbRecordType;
    GPHENTRY		*pgphentry;

    pgphentry = gphFind(pdbbase->pgpHash,name,&pdbbase->recordTypeList);
    if(pgphentry) {
	duplicate = TRUE;
	return;
    }
    pdbRecordType = dbCalloc(1,sizeof(dbRecordType));
    pdbRecordType->name = strduplicate(name);
    if(ellCount(&tempList))
	yyerrorAbort("dbRecordtypeHead tempList not empty");
    allocTemp(pdbRecordType);
}

static void dbRecordtypeFieldHead(char *name,char *type)
{
    dbFldDes		*pdbFldDes;
    int			i;
    
    if(duplicate) return;
    pdbFldDes = dbCalloc(1,sizeof(dbFldDes));
    allocTemp(pdbFldDes);
    pdbFldDes->name = strduplicate(name);
    pdbFldDes->as_level = ASL1;
    for(i=0; i<DBF_NTYPES; i++) {
	if(strcmp(type,pamapdbfType[i].strvalue)==0) {
	    pdbFldDes->field_type = pamapdbfType[i].value;
	    return;
	}
    }
    yyerrorAbort("Illegal Field Type");
}

static void dbRecordtypeFieldItem(char *name,char *value)
{
    dbFldDes		*pdbFldDes;
    
    if(duplicate) return;
    pdbFldDes = (dbFldDes *)getLastTemp();
    if(strcmp(name,"asl")==0) {
	if(strcmp(value,"ASL0")==0) {
	    pdbFldDes->as_level = ASL0;
	} else if(strcmp(value,"ASL1")==0) {
	    pdbFldDes->as_level = ASL1;
	} else {
	    yyerror("Illegal Access Security value: Must be ASL0 or ASL1");
	}
	return;
    }
    if(strcmp(name,"initial")==0) {
	pdbFldDes->initial = strduplicate(value);
	return;
    }
    if(strcmp(name,"promptgroup")==0) {
	int	i;
	for(i=0; i<GUI_NTYPES; i++) {
	    if(strcmp(value,pamapguiGroup[i].strvalue)==0) {
		pdbFldDes->promptgroup = pamapguiGroup[i].value;
		return;
	    }
	}
	yyerror("Illegal promptgroup. See guigroup.h for legal values");
	return;
    }
    if(strcmp(name,"prompt")==0) {
	pdbFldDes->prompt = strduplicate(value);
	return;
    }
    if(strcmp(name,"special")==0) {
	int	i;
	for(i=0; i<SPC_NTYPES; i++) {
	    if(strcmp(value,pamapspcType[i].strvalue)==0) {
		pdbFldDes->special = pamapspcType[i].value;
		return;
	    }
	}
	if(sscanf(value,"%hd",&pdbFldDes->special)==1) {
	    return;
	}
	yyerror("Illegal special value.");
	return;
    }
    if(strcmp(name,"pp")==0) {
	if((strcmp(value,"YES")==0) || (strcmp(value,"TRUE")==0)) {
	    pdbFldDes->process_passive = TRUE;
	} else if((strcmp(value,"NO")==0) || (strcmp(value,"FALSE")==0)) {
	    pdbFldDes->process_passive = FALSE;
	} else {
	    yyerror("Illegal value. Must be NO or YES");
	}
	return;
    }
    if(strcmp(name,"interest")==0) {
	if(sscanf(value,"%hd",&pdbFldDes->interest)!=1) 
	    yyerror("Illegal value. Must be integer");
	return;
    }
    if(strcmp(name,"base")==0) {
	if(strcmp(value,"DECIMAL")==0) {
	    pdbFldDes->base = CT_DECIMAL;
	} else if(strcmp(value,"HEX")==0) {
	    pdbFldDes->base = CT_HEX;
	} else {
	    yyerror("Illegal value. Must be CT_DECIMAL or CT_HEX");
	}
	return;
    }
    if(strcmp(name,"size")==0) {
	if(sscanf(value,"%hd",&pdbFldDes->size)!=1) 
	    yyerror("Illegal value. Must be integer");
	return;
    }
    if(strcmp(name,"extra")==0) {
	pdbFldDes->extra = strduplicate(value);
	return;
    }
    if(strcmp(name,"menu")==0) {
	pdbFldDes->ftPvt = (dbMenu *)dbFindMenu(pdbbase,value);
	if(!pdbbase->ignoreMissingMenus && !pdbFldDes->ftPvt)
	    yyerrorAbort("menu not found");
	return;
    }
}

static void dbRecordtypeBody(void)
{
    dbRecordType		*pdbRecordType;
    dbFldDes		*pdbFldDes;
    int			i,j,ilink;
    GPHENTRY		*pgphentry;
    int			no_fields,no_prompt,no_links;
    dbfType		field_type;
    char		*psortFldNameTemp;
    short		psortFldIndTemp;
    char		**papsortFldName;
    short		*sortFldInd;

    if(duplicate) {
	duplicate = FALSE;
	return;
    }
    pdbRecordType= (dbRecordType *)popFirstTemp();
    pdbRecordType->no_fields = no_fields = ellCount(&tempList);
    pdbRecordType->papFldDes = dbCalloc(no_fields,sizeof(dbFldDes *));
    pdbRecordType->papsortFldName = dbCalloc(no_fields,sizeof(char *));
    pdbRecordType->sortFldInd = dbCalloc(no_fields,sizeof(short));
    no_prompt = no_links = 0;
    for(i=0; i<no_fields; i++) {
	pdbFldDes = (dbFldDes *)popFirstTemp();
	pdbFldDes->pdbRecordType = pdbRecordType;
	pdbFldDes->indRecordType = i;
	pdbRecordType->papFldDes[i] = pdbFldDes;
	if(pdbFldDes->promptgroup) no_prompt++;
	field_type = pdbFldDes->field_type;
	if((field_type>=DBF_INLINK) && (field_type<=DBF_FWDLINK))no_links++;
	if((field_type==DBF_STRING) && (pdbFldDes->size==0))
	    fprintf(stderr,"recordtype(%s).%s size not specified\n",
		pdbRecordType->name,pdbFldDes->name);
	if((field_type==DBF_NOACCESS) && (pdbFldDes->extra==0)) 
	    fprintf(stderr,"recordtype(%s).%s extra not specified\n",
		pdbRecordType->name,pdbFldDes->name);
    }
    if(ellCount(&tempList)) yyerrorAbort("dbMenuBody: tempList not empty");
    pdbRecordType->no_prompt = no_prompt;
    pdbRecordType->no_links = no_links;
    pdbRecordType->link_ind = dbCalloc(no_links,sizeof(short));
    ilink = 0;
    for(i=0; i<no_fields; i++) {
	pdbFldDes = pdbRecordType->papFldDes[i];
        /* if prompt is null make it a null string */
        if(!pdbFldDes->prompt) pdbFldDes->prompt = dbCalloc(1,sizeof(char));
	field_type = pdbFldDes->field_type;
	if((field_type>=DBF_INLINK) && (field_type<=DBF_FWDLINK))
	    pdbRecordType->link_ind[ilink++] = i;
	if(strcmp(pdbFldDes->name,"VAL")==0) {
	    pdbRecordType->pvalFldDes = pdbRecordType->papFldDes[i];
	    pdbRecordType->indvalFlddes = i;
	}
	pdbRecordType->papsortFldName[i] = pdbFldDes->name;
	pdbRecordType->sortFldInd[i] = i;
    }
   /*Now sort fields. Sorry dumb sort algorithm */
   papsortFldName = pdbRecordType->papsortFldName;
   sortFldInd = pdbRecordType->sortFldInd;
   for(i=0; i<no_fields; i++) {
	for(j=i+1; j<no_fields; j++) {
	    if(strcmp(papsortFldName[j],papsortFldName[i])<0 ) {
		psortFldNameTemp = papsortFldName[j];
		psortFldIndTemp = sortFldInd[j];
		papsortFldName[j] = papsortFldName[i];
		sortFldInd[j] = sortFldInd[i];
		papsortFldName[i] = psortFldNameTemp;
		sortFldInd[i] = psortFldIndTemp;
	    }
	}
    }
    /*Initialize lists*/
    ellInit(&pdbRecordType->attributeList);
    ellInit(&pdbRecordType->recList);
    ellInit(&pdbRecordType->devList);
    pgphentry = gphAdd(pdbbase->pgpHash,pdbRecordType->name,
	&pdbbase->recordTypeList);
    if(!pgphentry) {
	yyerrorAbort("gphAdd failed");
    } else {
	pgphentry->userPvt = pdbRecordType;
    }
    ellAdd(&pdbbase->recordTypeList,&pdbRecordType->node);
    dbGetRecordtypeSizeOffset(pdbRecordType);
}

static void dbDevice(char *recordtype,char *linktype,
	char *dsetname,char *choicestring)
{
    devSup	*pdevSup;
    dbRecordType	*pdbRecordType;
    GPHENTRY	*pgphentry;
    int		i,link_type;
    pgphentry = gphFind(pdbbase->pgpHash,recordtype,&pdbbase->recordTypeList);
    if(!pgphentry) {
	yyerror(" record type not found");
	return;
    }
    link_type=-1;
    for(i=0; i<LINK_NTYPES; i++) {
	if(strcmp(pamaplinkType[i].strvalue,linktype)==0) {
	    link_type = pamaplinkType[i].value;
	    break;
	}
    }
    if(link_type==-1) {
	yyerror("Illegal link type");
	return;
    }
    pdbRecordType = (dbRecordType *)pgphentry->userPvt;
    pgphentry = gphFind(pdbbase->pgpHash,choicestring,&pdbRecordType->devList);
    if(pgphentry) {
	return;
    }
    pdevSup = dbCalloc(1,sizeof(devSup));
    pdevSup->name = strduplicate(dsetname);
    pdevSup->choice = strduplicate(choicestring);
    pdevSup->link_type = link_type;
    pgphentry = gphAdd(pdbbase->pgpHash,pdevSup->choice,&pdbRecordType->devList);
    if(!pgphentry) {
	yyerror("gphAdd failed");
    } else {
	pgphentry->userPvt = pdevSup;
    }
    ellAdd(&pdbRecordType->devList,&pdevSup->node);
}

static void dbDriver(char *name)
{
    drvSup	*pdrvSup;
    GPHENTRY	*pgphentry;

    pgphentry = gphFind(pdbbase->pgpHash,name,&pdbbase->drvList);
    if(pgphentry) {
	return;
    }
    pdrvSup = dbCalloc(1,sizeof(drvSup));
    pdrvSup->name = strduplicate(name);
    pgphentry = gphAdd(pdbbase->pgpHash,pdrvSup->name,&pdbbase->drvList);
    if(!pgphentry) {
	yyerrorAbort("gphAdd failed");
    } 
    pgphentry->userPvt = pdrvSup;
    ellAdd(&pdbbase->drvList,&pdrvSup->node);
}

static void dbBreakHead(char *name)
{
    brkTable	*pbrkTable;
    GPHENTRY	*pgphentry;

    pgphentry = gphFind(pdbbase->pgpHash,name,&pdbbase->bptList);
    if(pgphentry) {
	duplicate = TRUE;
	return;
    }
    pbrkTable = dbCalloc(1,sizeof(brkTable));
    pbrkTable->name = strduplicate(name);
    if(ellCount(&tempList)) yyerrorAbort("dbBreakHead:tempList not empty");
    allocTemp(pbrkTable);
}

static void dbBreakItem(char *value)
{
    if(duplicate) return;
    allocTemp(strduplicate(value));
}

static void dbBreakBody(void)
{
    brkTable		*pnewbrkTable;
    brkTable		*pbrkTable;
    int			number;
    int			i,choice;
    GPHENTRY		*pgphentry;

    if(duplicate) {
	duplicate = FALSE;
	return;
    }
    pnewbrkTable = (brkTable *)popFirstTemp();
    pnewbrkTable->number = number = ellCount(&tempList)/2;
    if(number*2 != ellCount(&tempList))
	yyerrorAbort("dbBreakBody: Odd number of values");
    pnewbrkTable->papBrkInt = dbCalloc(number,sizeof(brkInt));
    for(i=0; i<number; i++) {
	double	raw,eng;
	char	*praw;
	char	*peng;
	
	praw = (char *)popFirstTemp();
	peng = (char *)popFirstTemp();
	if((sscanf(praw,"%lf",&raw)!=1) || (sscanf(peng,"%lf",&eng)!=1) ) {
	    yyerrorAbort("dbbrkTable: Illegal table value");
	}
	free((void *)praw);
	free((void *)peng);
	pnewbrkTable->papBrkInt[i] = dbCalloc(1,sizeof(brkInt));
	pnewbrkTable->papBrkInt[i]->raw = raw;
	pnewbrkTable->papBrkInt[i]->eng = eng;
    }
    /* Compute slopes */
    for(i=0; i<number-1; i++) {
	pnewbrkTable->papBrkInt[i]->slope =
	  (pnewbrkTable->papBrkInt[i+1]->eng - pnewbrkTable->papBrkInt[i]->eng)/
	  (pnewbrkTable->papBrkInt[i+1]->raw - pnewbrkTable->papBrkInt[i]->raw);
    }
    if(number>1) {
        pnewbrkTable->papBrkInt[number-1]->slope =
            pnewbrkTable->papBrkInt[number-2]->slope;
    }
    /* Add brkTable in sorted order */
    pbrkTable = (brkTable *)ellFirst(&pdbbase->bptList);
    while(pbrkTable) {
	choice = strcmp(pbrkTable->name,pnewbrkTable->name);
	if(choice==0) {
	    ellInsert(&pdbbase->bptList,ellPrevious((ELLNODE *)pbrkTable),
		(ELLNODE *)pnewbrkTable);
	    gphDelete(pdbbase->pgpHash,pbrkTable->name,&pdbbase->bptList);
	    ellDelete(&pdbbase->bptList,(ELLNODE *)pbrkTable);
	    break;
	} else if(choice>0) {
	    ellInsert(&pdbbase->bptList,ellPrevious((ELLNODE *)pbrkTable),
		(ELLNODE *)pnewbrkTable);
	    break;
	}
	pbrkTable = (brkTable *)ellNext(&pbrkTable->node);
    }
    pgphentry = gphAdd(pdbbase->pgpHash,pnewbrkTable->name,&pdbbase->bptList);
    if(!pgphentry) {
	yyerrorAbort("gphAdd failed");
    } else {
	pgphentry->userPvt = pnewbrkTable;
    }
    if(!pbrkTable) ellAdd(&pdbbase->bptList,&pnewbrkTable->node);
}

static void dbRecordHead(char *recordType,char *name, int visible)
{
    DBENTRY		*pdbentry;
    long		status;

    pdbentry = dbAllocEntry(pdbbase);
    if(ellCount(&tempList))
	yyerrorAbort("dbRecordHead: tempList not empty");
    allocTemp(pdbentry);
    status = dbFindRecordType(pdbentry,recordType);
    if(status) {
	errMessage(status,"");
	yyerrorAbort(NULL);
	return;
    }
    /*Duplicate records ok if the same type */
    status = dbCreateRecord(pdbentry,name);
    if(status==S_dbLib_recExists) {
	if(strcmp(recordType,dbGetRecordTypeName(pdbentry))!=0) {
	    yyerror("already defined for different record type");
	    duplicate = TRUE;
	    return;
	}
    } else if(status) {
	errMessage(status,"new record instance error");
	yyerrorAbort(NULL);
    }
    if(visible) dbVisibleRecord(pdbentry);
}

static void dbRecordField(char *name,char *value)
{
    DBENTRY		*pdbentry;
    tempListNode	*ptempListNode;
    long		status;

    if(duplicate) return;
    ptempListNode = (tempListNode *)ellFirst(&tempList);
    pdbentry = ptempListNode->item;
    status = dbFindField(pdbentry,name);
    if(status) {
	errMessage(status,"");
	yyerror(NULL);
	return;
    }
    status = dbPutString(pdbentry,value);
    if(status) {
	errMessage(status,"");
	yyerror(NULL);
	return;
    }
}

static void dbRecordBody(void)
{
    DBENTRY	*pdbentry;

    if(duplicate) {
	duplicate = FALSE;
	return;
    }
    pdbentry = (DBENTRY *)popFirstTemp();
    if(ellCount(&tempList))
	yyerrorAbort("dbRecordBody: tempList not empty");
    dbFreeEntry(pdbentry);
}
