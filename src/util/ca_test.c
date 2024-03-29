/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*	@(#)ca_test.c	$Id$
 *	Author:	Jeff Hill
 *	Date:	07-01-91
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 */

/*
 * ANSI
 */
#include <string.h>
#include <stdio.h>

#ifdef vxWorks
#include <vxWorks.h>
#endif

#ifndef ERROR
#define ERROR -1
#endif

#ifndef OK
#define OK 0
#endif

#ifndef LOCAL
#define LOCAL static
#endif

#include        <cadef.h>

int ca_test(char *pname, char *pvalue);
LOCAL int cagft(char *pname);
LOCAL void printit(struct  event_handler_args args);
LOCAL int capft(char *pname, char *pvalue);
LOCAL void verify_value(chid chan_id, chtype type);
LOCAL void print_returned(chtype type, const void *pbuffer, unsigned count);

static unsigned long	outstanding;


/*
 *
 *	main
 *
 *	parse command line arguments
 */
#ifndef vxWorks
int main(
int     argc,
char    **argv
)
{

	/*
	 * print error and return if arguments are invalid
	 */
	if(argc < 2  || argc > 3){
		printf("usage: ca_test channel_name <\"put value\">\n");
		printf("the following arguments were received\n");
		while(argc>0) {
			printf("%s\n",argv[0]);
			argv++; argc--;
		}
		return ERROR;
	}


	/*
	 * check for supplied value
	 */
	if(argc == 2){
		return ca_test(argv[1], NULL);
	}
	else if(argc == 3){
		char *pt;

		/* strip leading and trailing quotes*/
		if(argv[2][1]=='"') argv[2]++;
		if( (pt=strchr(argv[2],'"')) ) *pt = 0;
		return ca_test(argv[1], argv[2]);
	}
	else{
		return ERROR;
	}

}
#endif


/*
 *  	ca_test
 *
 *	find channel, write a value if supplied, and 
 *	read back the current value
 *
 */
int ca_test(
char	*pname,
char	*pvalue
)
{

	if(pvalue){
		return capft(pname,pvalue);
	}
	else{
		return cagft(pname);
	}
}



/*
 * 	cagft()
 *
 *	ca get field test
 *
 *	test ca get over the range of CA data types
 */
LOCAL int cagft(char *pname)
{	
	chid		chan_id;
	int		status;
	int		i;
	unsigned long	ntries = 10ul;

	/* 
	 *	convert name to chan id 
	 */
	status = ca_search(pname, &chan_id);
	SEVCHK(status,NULL);
	status = ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		printf("Not Found %s\n", pname);
		return ERROR;
	}

	printf("name:\t%s\n", ca_name(chan_id));
	printf("native type:\t%d\n", ca_field_type(chan_id));
	printf("native count:\t%d\n", ca_element_count(chan_id));


	/* 
 	 * fetch as each type 
	 */
	for(i=0; i<=DBR_CTRL_DOUBLE; i++){
		if(ca_field_type(chan_id)==DBR_STRING) {
			if( (i!=DBR_STRING)
			  && (i!=DBR_STS_STRING)
			  && (i!=DBR_TIME_STRING)
			  && (i!=DBR_GR_STRING)
			  && (i!=DBR_CTRL_STRING)) continue;
		}
		status = ca_array_get_callback(
				i, 
				ca_element_count(chan_id),
				chan_id, 
				printit, 
				NULL);
		SEVCHK(status, NULL);

		outstanding++;
	}

	/*
	 * wait for the operation to complete
	 * before returning 
	 */
	while(ntries){
		unsigned long oldOut;

		oldOut = outstanding;
		ca_pend_event (5.0);

		if(!outstanding){
			printf("\n\n");
			return OK;
		}

		if (outstanding==oldOut) {
			ntries--;
		}
	}

	return	ERROR;
}


/*
 *	PRINTIT()
 */
LOCAL void printit(struct  event_handler_args args)
{
	if (args.status == ECA_NORMAL) {
		print_returned(
			args.type,
			args.dbr,
			args.count);
	}
	else {
		printf ("%s: err resp to get cb was \"%s\"\n",
			__FILE__, ca_message(args.status));
	}

	outstanding--;
}




/*
 *	capft
 *
 *	test ca_put() over a range of data types
 *	
 */
LOCAL int capft(
char		*pname,
char		*pvalue
)
{
	dbr_short_t			shortvalue;
	dbr_long_t			longvalue;
	dbr_float_t			floatvalue;
	dbr_char_t			charvalue;
	dbr_double_t		doublevalue;
	unsigned long		ntries = 10ul;
	int					status;
	chid				chan_id;

	if (((*pname < ' ') || (*pname > 'z'))
	  || ((*pvalue < ' ') || (*pvalue > 'z'))){
		printf("\nusage \"pv name\",\"value\"\n");
		return ERROR;
	}

	/* 
	 *	convert name to chan id 
	 */
	status = ca_search(pname, &chan_id);
	SEVCHK(status,NULL);
	status = ca_pend_io(2.0);
	if(status != ECA_NORMAL){
		printf("Not Found %s\n", pname);
		return ERROR;
	}

	printf("name:\t%s\n", ca_name(chan_id));
	printf("native type:\t%d\n", ca_field_type(chan_id));
	printf("native count:\t%d\n", ca_element_count(chan_id));

	/*
	 *  string value ca_put
	 */
	status = ca_put(
			DBR_STRING, 
			chan_id, 
			pvalue);
	SEVCHK(status, NULL);
	verify_value(chan_id, DBR_STRING);

	if(ca_field_type(chan_id)==0)goto skip_rest;

	if(sscanf(pvalue,"%hd",&shortvalue)==1) {
		/*
		 * short integer ca_put
		 */
		status = ca_put(
				DBR_SHORT, 
				chan_id, 
				&shortvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_SHORT);
		status = ca_put(
				DBR_ENUM, 
				chan_id, 
				&shortvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_ENUM);
		charvalue=(dbr_char_t)shortvalue;
		status = ca_put(
				DBR_CHAR, 
				chan_id, 
				&charvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_CHAR);
	}
	if(sscanf(pvalue,"%d",&longvalue)==1) {
		/*
		 * long integer ca_put
		 */
		status = ca_put(
				DBR_LONG, 
				chan_id, 
				&longvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_LONG);
	}
	if(sscanf(pvalue,"%f",&floatvalue)==1) {
		/*
		 * single precision float ca_put
		 */
		status = ca_put(
				DBR_FLOAT, 
				chan_id, 
				&floatvalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_FLOAT);
	}
	if(sscanf(pvalue,"%lf",&doublevalue)==1) {
		/*
		 * double precision float ca_put
		 */
		status = ca_put(
				DBR_DOUBLE, 
				chan_id, 
				&doublevalue);
		SEVCHK(status, NULL);
		verify_value(chan_id, DBR_DOUBLE);
	}

skip_rest:

	/*
	 * wait for the operation to complete
	 * (outstabnding decrements to zero)
	 */
	while(ntries){
		ca_pend_event(1.0);

		if(!outstanding){
			printf("\n\n");
			return OK;
		}

		ntries--;
	}

	return	ERROR;
}


/*
 * VERIFY_VALUE
 *
 * initiate print out the values in a database access interface structure
 */
LOCAL void verify_value(chid chan_id, chtype type)
{
	int status;

	/*
	 * issue a get which calls back `printit'
	 * upon completion
	 */
	status = ca_array_get_callback(
			type, 
			ca_element_count(chan_id),
			chan_id, 
			printit, 
			NULL);
	SEVCHK(status, NULL);

	outstanding++;
}


/*
 * PRINT_RETURNED
 *
 * print out the values in a database access interface structure
 *
 * switches over the range of CA data types and reports the value
 */
LOCAL void print_returned(chtype type, const void *pbuffer, unsigned count)
{
    unsigned	i;
    char 	tsString[50];

    printf("%s\t",dbr_text[type]);
    switch(type){
	case (DBR_STRING):
	{
		dbr_string_t	*pString = (dbr_string_t *) pbuffer;

		for(i=0; i<count && (*pString)[0]!='\0'; i++) {
			if(count!=1 && (i%5 == 0)) printf("\n");
			printf("%s ", *pString);
			pString++;
		}
		break;
	}
	case (DBR_SHORT):
	{
		dbr_short_t *pvalue = (dbr_short_t *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*(short *)pvalue);
		}
		break;
	}
	case (DBR_ENUM):
	{
		dbr_enum_t *pvalue = (dbr_enum_t *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pvalue);
		}
		break;
	}
	case (DBR_FLOAT):
	{
		dbr_float_t *pvalue = (dbr_float_t *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*(float *)pvalue);
		}
		break;
	}
	case (DBR_CHAR):
	{
		dbr_char_t 	*pvalue = (dbr_char_t *) pbuffer;

		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%u ",*pvalue);
		}
		break;
	}
	case (DBR_LONG):
	{
		dbr_long_t *pvalue = (dbr_long_t *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pvalue);
		}
		break;
	}
	case (DBR_DOUBLE):
	{
		dbr_double_t *pvalue = (dbr_double_t *)pbuffer;
		for (i = 0; i < count; i++,pvalue++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pvalue));
		}
		break;
	}
	case (DBR_STS_STRING):
	case (DBR_GR_STRING):
	case (DBR_CTRL_STRING):
	{
		struct dbr_sts_string *pvalue 
		  = (struct dbr_sts_string *) pbuffer;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tValue: %s",pvalue->value);
		break;
	}
	case (DBR_STS_ENUM):
	{
		struct dbr_sts_enum *pvalue
		  = (struct dbr_sts_enum *)pbuffer;
		dbr_enum_t *pEnum = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pEnum++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%u ",*pEnum);
		}
		break;
	}
	case (DBR_STS_SHORT):
	{
		struct dbr_sts_short *pvalue
		  = (struct dbr_sts_short *)pbuffer;
		dbr_short_t *pshort = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%u ",*pshort);
		}
		break;
	}
	case (DBR_STS_FLOAT):
	{
		struct dbr_sts_float *pvalue
		  = (struct dbr_sts_float *)pbuffer;
		dbr_float_t *pfloat = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_STS_CHAR):
	{
		struct dbr_sts_char *pvalue
		  = (struct dbr_sts_char *)pbuffer;
		dbr_char_t *pchar = &pvalue->value;

		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%u ", *pchar);
		}
		break;
	}
	case (DBR_STS_LONG):
	{
		struct dbr_sts_long *pvalue
		  = (struct dbr_sts_long *)pbuffer;
		dbr_long_t *plong = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
		}
		break;
	}
	case (DBR_STS_DOUBLE):
	{
		struct dbr_sts_double *pvalue
		  = (struct dbr_sts_double *)pbuffer;
		dbr_double_t *pdouble = &pvalue->value;
		printf("%2d %2d",pvalue->status,pvalue->severity);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pdouble));
		}
		break;
	}
	case (DBR_TIME_STRING):
	{
		struct dbr_time_string *pvalue 
		  = (struct dbr_time_string *) pbuffer;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		printf("\tValue: ");
		printf("%s",pvalue->value);
		break;
	}
	case (DBR_TIME_ENUM):
	{
		struct dbr_time_enum *pvalue
		  = (struct dbr_time_enum *)pbuffer;
		dbr_enum_t *pshort = &pvalue->value;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_TIME_SHORT):
	{
		struct dbr_time_short *pvalue
		  = (struct dbr_time_short *)pbuffer;
		dbr_short_t *pshort = &pvalue->value;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",
			pvalue->status,
			pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_TIME_FLOAT):
	{
		struct dbr_time_float *pvalue
		  = (struct dbr_time_float *)pbuffer;
		dbr_float_t *pfloat = &pvalue->value;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_TIME_CHAR):
	{
		struct dbr_time_char *pvalue
		  = (struct dbr_time_char *)pbuffer;
		dbr_char_t *pchar = &pvalue->value;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",(short)(*pchar));
		}
		break;
	}
	case (DBR_TIME_LONG):
	{
		struct dbr_time_long *pvalue
		  = (struct dbr_time_long *)pbuffer;
		dbr_long_t *plong = &pvalue->value;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
		}
		break;
	}
	case (DBR_TIME_DOUBLE):
	{
		struct dbr_time_double *pvalue
		  = (struct dbr_time_double *)pbuffer;
		dbr_double_t *pdouble = &pvalue->value;

		tsStampToText(&pvalue->stamp,TS_TEXT_MMDDYY,tsString);
		printf("%2d %2d",pvalue->status,pvalue->severity);
		printf("\tTimeStamp: %s",tsString);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pdouble));
		}
		break;
	}
	case (DBR_GR_SHORT):
	{
		struct dbr_gr_short *pvalue
		  = (struct dbr_gr_short *)pbuffer;
		dbr_short_t *pshort = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_GR_FLOAT):
	{
		struct dbr_gr_float *pvalue
		  = (struct dbr_gr_float *)pbuffer;
		dbr_float_t *pfloat = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_GR_ENUM):
	case (DBR_CTRL_ENUM):
	{
		struct dbr_gr_enum *pvalue
		  = (struct dbr_gr_enum *)pbuffer;
		printf("%2d %2d",pvalue->status,
			pvalue->severity);
		printf("\tValue: %d",pvalue->value);
		if(pvalue->no_str>0) {
			printf("\n\t%3d",pvalue->no_str);
			for (i = 0; i < (unsigned) pvalue->no_str; i++)
				printf("\n\t%.26s",pvalue->strs[i]);
		}
		break;
	}
	case (DBR_GR_CHAR):
	{
		struct dbr_gr_char *pvalue
		  = (struct dbr_gr_char *)pbuffer;
		dbr_char_t *pchar = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%u ",*pchar);
		}
		break;
	}
	case (DBR_GR_LONG):
	{
		struct dbr_gr_long *pvalue
		  = (struct dbr_gr_long *)pbuffer;
		dbr_long_t *plong = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
		}
		break;
	}
	case (DBR_GR_DOUBLE):
	{
		struct dbr_gr_double *pvalue
		  = (struct dbr_gr_double *)pbuffer;
		dbr_double_t *pdouble = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  (float)(pvalue->upper_disp_limit),
		  (float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),
		  (float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),
		  (float)(pvalue->lower_alarm_limit));
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",(float)(*pdouble));
		}
		break;
	}
	case (DBR_CTRL_SHORT):
	{
		struct dbr_ctrl_short *pvalue
		  = (struct dbr_ctrl_short *)pbuffer;
		dbr_short_t *pshort = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pshort++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*pshort);
		}
		break;
	}
	case (DBR_CTRL_FLOAT):
	{
		struct dbr_ctrl_float *pvalue
		  = (struct dbr_ctrl_float *)pbuffer;
		dbr_float_t *pfloat = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8.3f %8.3f",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pfloat++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.4f ",*pfloat);
		}
		break;
	}
	case (DBR_CTRL_CHAR):
	{
		struct dbr_ctrl_char *pvalue
		  = (struct dbr_ctrl_char *)pbuffer;
		dbr_char_t *pchar = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pchar++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%4d ",(short)(*pchar));
		}
		break;
	}
	case (DBR_CTRL_LONG):
	{
		struct dbr_ctrl_long *pvalue
		  = (struct dbr_ctrl_long *)pbuffer;
		dbr_long_t *plong = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf("\n\t%8d %8d %8d %8d %8d %8d",
		  pvalue->upper_disp_limit,pvalue->lower_disp_limit,
		  pvalue->upper_alarm_limit,pvalue->upper_warning_limit,
		  pvalue->lower_warning_limit,pvalue->lower_alarm_limit);
		printf(" %8d %8d",
		  pvalue->upper_ctrl_limit,pvalue->lower_ctrl_limit);
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,plong++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%d ",*plong);
		}
		break;
	}
	case (DBR_CTRL_DOUBLE):
	{
		struct dbr_ctrl_double *pvalue
		  = (struct dbr_ctrl_double *)pbuffer;
		dbr_double_t *pdouble = &pvalue->value;
		printf("%2d %2d %.8s",pvalue->status,pvalue->severity,
			pvalue->units);
		printf(" %3d\n\t%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f",
		  pvalue->precision,
		  (float)(pvalue->upper_disp_limit),
		  (float)(pvalue->lower_disp_limit),
		  (float)(pvalue->upper_alarm_limit),
		  (float)(pvalue->upper_warning_limit),
		  (float)(pvalue->lower_warning_limit),
		  (float)(pvalue->lower_alarm_limit));
		printf(" %8.3f %8.3f",
		  (float)(pvalue->upper_ctrl_limit),
		  (float)(pvalue->lower_ctrl_limit));
		if(count==1) printf("\tValue: ");
		for (i = 0; i < count; i++,pdouble++){
			if(count!=1 && (i%10 == 0)) printf("\n");
			printf("%6.6f ",(float)(*pdouble));
		}
		break;
	}
    }
    printf("\n");
}

