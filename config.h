/*------------------------------------------------------------------------*/
/*--									--*/
/*--	config.h							--*/
/*--									--*/
/*--	Utility for reading/writing configuration paramters	       	--*/
/*--									--*/
/*------------------------------------------------------------------------*/
/*
Copyright (C) 2001 Dan Boneh

See LICENSE for license
*/

/*
 *  Config file structure:
 *	Lines beginning wiht a ';' and blank lines are ignored.
 *	Every line is of the form:   parameter-name = parameter-value
 *
 *	parameter-name may contain any alpha numeric including '_', '-' .
 *	parameter-value starts at the first non white space following
 *		the '=' and ends at the '\n'.
 *
 *	Several functions are provided to parse the parameter-value.
 *	See prototypes and documentation below.
 */



#ifndef _UTLCONFIG_H_
#define _UTLCONFIG_H_

/* Tweakables for win32 */
#ifndef PUBLIC
#define PUBLIC
#define PRIVATE static
#endif

#define MAX_PARAM   500
#define MAX_NAMLEN	20
#define MAXLINE	2048

// Seperator between parameter name and parameter value.
#define SEP		'='

#ifdef __cplusplus
extern "C" {
#endif

/*---  Configuration context   ---*/

typedef struct config_ctx {

  int NumParam;
  struct assoc_array {
    char *type;
    char *value;
    char *path;
  } ConfigParam[MAX_PARAM];

  char * ConfigFilespec;

} CONF_CTX;


/*---  Return codes  ---*/

#define NOT_FOUND	-1


/*---  Function prototypes  ---*/


/* Constructors for config parameters */
/* ---------------------------------- */

extern PUBLIC CONF_CTX *constructCTX();
extern PUBLIC CONF_CTX *LoadConfig(const char *filename);

/* LoadConfig returns NULL if it is unable to load file */


/*   Destructor: free memory allocated for the config context.		
 *   Config params in given context become inaccessible after this call.
 */
extern PUBLIC int destructCTX(CONF_CTX *ctx);


/*  Save current config context to a config file.  */
extern PUBLIC int WriteConfig(CONF_CTX *ctx, const char *filename);

/*    Read functions    */
/*----------------------*/

/*
 * The interface to all GetConfig functions is uniform:
 *	ctx:    config context where paramter exists.
 *	name:   name of parameter to retrieve.
 *	index:  in case of multiple instances of the same parameter
 *		the index refers to the desired index.  If only one
 *		instance of the parameter is expected this value
 *		should be '0'.
 *	deflt:  Default value to return in case parameter is not found
 *		in config context.
 */


/*
 *  Interpret config parameter as a string.
 *  Returned value must NOT be deallocated by caller.
 */
extern PUBLIC char *GetStringParam(
		const CONF_CTX *ctx, char *name, const int index, 
		char *deflt);

/*
 *  Interpret config parameter as a path name. Relative path names 
 *  are interpreted relative to location of config file.
 *  Returned value must NOT be deallocated by caller.
 */
extern PUBLIC char *GetPathParam(
		CONF_CTX *ctx, char *name, const int index, 
		char *deflt);

/*
 *  Interpret config parameter as a postive integer written in decimal.
 */
extern PUBLIC int GetIntParam(
		const CONF_CTX *ctx, char *name, const int index, 
		const int deflt);


/*
 *  Interpret config parameter as a boolean.
 *  True values:    '\0'   ||  1   ||  true  || yes   (case insensative)
 *  False values:              0   ||  false || no
 */
extern PUBLIC int GetBoolParam(
		const CONF_CTX *ctx, char *name, const int index, 
		const int deflt);

/*
 *  Interpret config parameter as a list of strings seperated by
 *  ' '  ||   ','   ||   ';'
 *  The returned value is a NULL terminated array of string pointers.
 *  Caller must dellocated returned list.
 *  Deallocate by calling free(list[0]) and then free(list)	
 */
extern PUBLIC char **GetListParam(
		const CONF_CTX *ctx, char *name, const int index,
		char **deflt);


/*    Write functions    */
/*-----------------------*/


/*
 * The interface to all SetConfig functions is uniform:
 *	ctx:    config context where paramter exists.
 *	name:   name of parameter to set.  If no param exists
 *		with the given name, a new config param is created.
 *		Otherwise, the old value is discarded.
 *	index:  in case of multiple instances of the same parameter
 *		the index refers to the desired index.  If only one
 *		instance of the parameter is expected this value
 *		should be '0'.
 *	value:  value of config param.
 */

/*
 *  Set string parameter in config context.
 */
extern PUBLIC int SetStringParam(
		CONF_CTX *ctx, char *name, const int index, 
		const char *value);

/*
 *  Set integer parameter in config context.
 */
extern PUBLIC int SetIntParam(
		CONF_CTX *ctx, char *name, const int index, 
		const int value);

/*
 *  Set boolean parameter in config context.  A non-zero value is interpreted
 *  as true.
 */
extern PUBLIC int SetBoolParam(
		CONF_CTX *ctx, char *name, const int index, 
		const int value);

/*
 *  Set list parameter in config context.  
 *  The function created a string seperated by ';' and stores result
 *  as string.
 */
extern PUBLIC int SetListParam(
		CONF_CTX *ctx, char *name, const int index, 
		char **value);

#ifdef __cplusplus
}
#endif

#endif
