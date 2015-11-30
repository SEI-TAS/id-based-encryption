/*------------------------------------------------------------------------*/
/*--									--*/
/*--	config.c							--*/
/*--									--*/
/*--	Utility for reading/writing configuration paramters	       	--*/
/*--	See  config.h  for a detailed description.			--*/
/*--									--*/
/*--	NOTE:  these utilities are NOT designed for high performance	--*/
/*--           and should NOT be called from within inner loops.	--*/
/*--									--*/
/*------------------------------------------------------------------------*/
/* 
 * Dan Boneh
 */

/* Mon Jan 29 20:56:58 PST 2001
 *  Modified to get rid of some warnings.
 *  Ben Lynn
 */

/*
Copyright (C) 2001 Dan Boneh

See LICENSE for license
*/

#include "config.h"
#include <stdio.h>
//#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*------------------------------------------------------------------------*/
/*--	GetFullName							--*/
/*------------------------------------------------------------------------*/

/*
 *   Get full config parameter name, including index.	
 *   Assumes fullname is sufficiently big		
 */

PRIVATE char *GetFullName(char *fullname, char *name, 
			      const int index)
{
  char *look = name;
  char indstr[6];

  /* Add index parameter if required */
  if ((index > 0) & (index < 0xFFFF)) {
    look = fullname;
    strncpy(fullname, name, strlen(name)+1);
    sprintf(indstr, "#%X", index);
    strncat(fullname, indstr, 6);
  }

  return look;
}

/*------------------------------------------------------------------------*/
/*--	GetIndex							--*/
/*------------------------------------------------------------------------*/

PRIVATE int GetIndex(
		const CONF_CTX *ctx, char *name, const int index)
{
  int i;
  char *look;
  char fullname[MAX_NAMLEN+6];

  look = GetFullName(fullname, name, index);

  for(i=0; i<ctx->NumParam; i++)
    if ( strncmp(ctx->ConfigParam[i].type, look, MAX_NAMLEN) == 0)
      return i;

  return NOT_FOUND;
}

/*------------------------------------------------------------------------*/
/*--	GetStringParam						--*/
/*--									--*/
/*--	Returns value of config parameter.				--*/
/*--	Returned value must NOT be deallocated by caller.		--*/
/*------------------------------------------------------------------------*/

extern PUBLIC char *GetStringParam(
		const CONF_CTX *ctx, char *name, const int index, 
		char *deflt) 
{
  int i;

  i = GetIndex(ctx, name, index);

  if (i<0) 
    return deflt;
 
  return ctx->ConfigParam[i].value;
}

/*------------------------------------------------------------------------*/
/*--	SetStringParam						--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int SetStringParam(
		CONF_CTX *ctx, char *name, const int index, 
		const char *value) 
{
  int pos, len;
  char *look;
  char fullname[MAX_NAMLEN+6];

  if ( (!ctx) || (strlen(value) > MAXLINE)  ||
       (strlen(name) > MAX_NAMLEN) ||
       (index < 0) ||  (index > 0xFFFF) )
    return 1;

  pos = GetIndex(ctx, name, index);

  if (pos < 0) {		/*  A new config parameter  */
    pos = ctx->NumParam;
    ctx->NumParam ++ ;

    look = GetFullName(fullname, name, index);

    len = strlen(look)+1;
    ctx->ConfigParam[pos].type = (char *)malloc(len);
    if (ctx->ConfigParam[pos].type == NULL) {
      fprintf(stderr, "SetStringParam: failed to allocate memory.\n");
      return 1;
    }
    strncpy(ctx->ConfigParam[pos].type, look, len);
  }

  else {		/*  A modification of an existing parameter */
    free(ctx->ConfigParam[pos].value);
    if (!ctx->ConfigParam[pos].path)
      free(ctx->ConfigParam[pos].path);
  }

  len = strlen(value)+1;
  ctx->ConfigParam[pos].value = (char *)malloc(len);
  if (ctx->ConfigParam[pos].value == NULL) {
    fprintf(stderr, "SetStringParam: failed to allocate memory.\n");
    return 1;
  }
  strncpy(ctx->ConfigParam[pos].value, value, len);
  ctx->ConfigParam[pos].path = NULL;

  return 0;
}

/*------------------------------------------------------------------------*/
/*--	RelativeToFile						--*/
/*------------------------------------------------------------------------*/

/*
 * Interprets 'filespec' as a file pathname, and interprets 'newfile'
 * relative to it.  Result is written to 'out'.  'out' can be the same
 * as either input.
 */

PRIVATE char *RelativeToFile(char *out, 
				 const char *filespec, const char *newfile)
{
  char * p;
  int len;

  if(newfile == NULL)
    return NULL;

  if ((filespec == NULL) || (*filespec == '\0') || (*newfile == '/') ||
     ((p = (char *)strrchr(filespec, '/')) == NULL)) {
    len = strlen(newfile) + 1;
    if(out == NULL)
      out = (char *)malloc(len);
    if(out)
      strncpy(out, newfile, len);
    return out;
  }

  if(out == NULL)
    out = (char *)malloc((p - filespec) + strlen(newfile) + 2);

  /* Handle duplicate/overlapped args correctly */
  memmove(out + (p - filespec) + 1, newfile, strlen(newfile) + 1);
  if(out != filespec)
    memmove(out, filespec, (p - filespec) + 1);
  return out;
}

/*------------------------------------------------------------------------*/
/*--	GetPathParam						--*/
/*--									--*/
/*--	Returns value of config parameter.				--*/
/*--    The value is interpreted as a pathname.				--*/
/*--    Relative filenames are interpreted relative to location of	--*/
/*--	config file.							--*/
/*--	Returned value must NOT be deallocated by caller.		--*/
/*------------------------------------------------------------------------*/

extern PUBLIC char *GetPathParam(
		CONF_CTX *ctx, char *name, const int index, 
		char *deflt) 
{
  int i;

  i = GetIndex(ctx, name, index);

  if (i<0) 
    return deflt;

  if (ctx->ConfigParam[i].path)
    return ctx->ConfigParam[i].path;
  else
    return (ctx->ConfigParam[i].path =
	        RelativeToFile(NULL, ctx->ConfigFilespec, 
				   ctx->ConfigParam[i].value));
}

/*------------------------------------------------------------------------*/
/*--	GetIntParam							--*/
/*--									--*/
/*--	Returns value of integer config parameter.			--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int GetIntParam(
		const CONF_CTX *ctx, char *name, const int index, 
		const int deflt)
{
  int i;
  char *temp;

  temp = GetStringParam(ctx, name, index, NULL);

  if (!temp) 
    return deflt;

  /* Check if string is an integer */
  for (i=0; (temp[i] != '\0'); ++i)
    if ((!isdigit((int)temp[i])) || (i>10))
      return deflt;
  
  /* Convert to integer */
  return atoi(temp);
}

/*------------------------------------------------------------------------*/
/*--	SetIntParam							--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int SetIntParam(
		CONF_CTX *ctx, char *name, const int index, 
		const int value) 
{
  char str[20];

  sprintf(str, "%d", value);
  
  return SetStringParam(ctx, name, index, str);
}

/*------------------------------------------------------------------------*/
/*--	GetBoolParam						--*/
/*--									--*/
/*--	Returns value of bool config parameter or default if parameter  --*/
/*--    is not defined.							--*/
/*--	Parameter is considered 'true' if it's value is one of		--*/
/*--   	    "1"   ||  "true"  || "yes"   (case insensative)		--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int GetBoolParam(
		const CONF_CTX *ctx, char *name, const int index, 
		const int deflt)
{
  char *temp;

  temp = GetStringParam(ctx, name, index, NULL);

  if (!temp) 
    return deflt;

  if ((*temp == '1')     || 
      (*temp == 't')     ||  (*temp == 'T')    ||      
      (*temp == 'y')     ||  (*temp == 'Y'))

    return 1;	/* true */

  else
    return 0;	/* false */
}

/*------------------------------------------------------------------------*/
/*--	SetBoolParam						--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int SetBoolParam(
		CONF_CTX *ctx, char *name, const int index, 
		const int value) 
{
  char *str;

  if (value)
    str = "True";
  else
    str = "False";
  
  return SetStringParam(ctx, name, index, str);
}

/*------------------------------------------------------------------------*/
/*--	GetListParam						--*/
/*--									--*/
/*--	Returns a list config parameter as a NULL terminated list.	--*/
/*--	Caller must dellocated return value.				--*/
/*--	To deallocate call free(list[0]) and then free(list)		--*/
/*------------------------------------------------------------------------*/

extern PUBLIC char **GetListParam(
		const CONF_CTX *ctx, char *name, const int index,
		char **deflt)
{
  int cnt, len;
  //char *temp, *temp1, *lasts; //got rid of *s
  char *temp, *temp1; //got rid of *s
  char **list;

  temp = GetStringParam(ctx, name, index, NULL);

  if (!temp) 
    return deflt;

  /* Convert string to list */

  len = strlen(temp)+1;
  if ((temp1 = (char *)malloc(len)) == NULL) {
    fprintf(stderr, "GetListParam: Error creating space for %s\n", name);
    return NULL;
  }

  strncpy(temp1, temp, len);

  //  Count number of tokens in string.
  cnt = 0;
  if (strtok(temp1, " ,;") != NULL) 
    for(cnt=1; (strtok(NULL, " ,;") != NULL); ++cnt);

  strncpy(temp1, temp, len);

  if ((list = (char **)malloc( (cnt+1)*(sizeof (char *)) )) == NULL) {
    fprintf(stderr, "GetListParam: Error creating list for %s\n", name);
    return NULL;
  }

  //  Put tokens into list.
  list[0] = (char *)strtok(temp1, " ,;");
  cnt = 0;  
  while (list[cnt]) {
    list[++cnt] = (char *)strtok(NULL, " ,;");
  } 

  return list;
}

/*------------------------------------------------------------------------*/
/*--	SetListParam						--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int SetListParam(
		CONF_CTX *ctx, char *name, const int index, 
		char **value) 
{
  char str[MAXLINE], *pos;
  int cur, len, size=0;

  pos = str;

  /* Construct a string seperate by ';' out of list  */

  for(cur = 0; (value[cur] != NULL); ++cur) {
    len = strlen(value[cur])+1;
    if (size + len > MAXLINE)
      return 1;
    strncpy(pos, value[cur], len);
    pos += len;
    *(pos-1) = ';';
    size += len;
  }

  *(pos-1) = '\0';

  return SetStringParam(ctx, name, index, str);
}

/*------------------------------------------------------------------------*/
/*--	LoadConfig							--*/
/*--									--*/
/*--	Load configuration file.  Must be called before configuration	--*/
/*--    access functions can be used.					--*/
/*------------------------------------------------------------------------*/

extern PUBLIC CONF_CTX *LoadConfig(const char *filename)
{
  FILE *fp;
  char line[MAXLINE], *c, *c1;
  CONF_CTX *ctx;
  int len;

  if ((ctx = constructCTX()) == NULL) 
    return NULL;

  /*--  Open configuration file  --*/
  
  fp = fopen(filename, "r");

  if (!fp) {
    fprintf(stderr, "LoadConfig: Unable to open config file %s\n", 
	            filename);
    return NULL;
  }

  if ((ctx->ConfigFilespec = (char *)malloc(strlen(filename)+1)) == NULL) {
    fprintf(stderr, "LoadConfig: Unable to allocate space for %s\n", 
	    filename);
    return NULL;
  }
  strcpy(ctx->ConfigFilespec, filename);


  /*--  Loop on lines in config file  --*/

  while ( (fgets(line, MAXLINE, fp)) ) {

    if (line[0] == ';')		    // Ignore comments.
      continue;

    // Scan for start of parameter name.
    for(c=line; isspace((int)*c) && *c; ++c);

    // Scan for end of parameter name.
    for(c1=c; !isspace((int)*c1) && (*c1 != SEP) && *c1; ++c1);

    if (*c1 == '\0')	//  Blank or malformed line.
      continue;
      
    *c1 = '\0';

    if (ctx->NumParam >= MAX_PARAM) {
      fprintf(stderr, 
	       "LoadConfig: Too many parameters in configuration file.\n");
      return NULL;
    }

    len = strlen(c);
    if (len > MAX_NAMLEN) {
      fprintf(stderr, 
	      "LoadConfig: Parameter name %s too long in file %s\n", 
	       c, filename);
      return NULL;
    }

    // Copy parameter name into config structure.
    len++;
    ctx->ConfigParam[ctx->NumParam].type = (char *)malloc(len);
    strncpy(ctx->ConfigParam[ctx->NumParam].type, c, len);

    //  Find start of parameter value.
    for(c=c1+1; ((*c == SEP) || isspace((int)*c)) && *c; ++c);
    
    //  Look for new line.
    for(c1=c; *c1 && (*c1 != '\n'); ++c1);

    if (*c1 != '\n') {
      fprintf(stderr, "LoadConfig: Parameter %s too long in file %s\n", 
		ctx->ConfigParam[ctx->NumParam].type, filename);
      return NULL;
    }

    *c1 = '\0';

    // Copy parameter value into config structure.
    len = strlen(c)+1;
    ctx->ConfigParam[ctx->NumParam].value = (char *)malloc(len);
    strncpy(ctx->ConfigParam[ctx->NumParam].value, c, len);
    ctx->ConfigParam[ctx->NumParam].path = NULL;

    ctx->NumParam ++ ;
  }

  fclose(fp);


  /*--  Print config params  --*/
#if 0
  printf("\nConfiguration parameters: \n");
  printf("----------------------------------------\n");
  for(len=0; len<ctx->NumParam; ++len)
    printf("%s: %s \n", ctx->ConfigParam[len].type, 
	                ctx->ConfigParam[len].value);
  printf("----------------------------------------\n\n");
#endif

  return ctx;

}

/*------------------------------------------------------------------------*/
/*--	WriteConfig							--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int WriteConfig(CONF_CTX *ctx, const char *filename)
{
  FILE *fp;
  int i;

  /*--  Open configuration file  --*/
  
  fp = fopen(filename, "w");

  if (!fp) {
    fprintf(stderr, "WriteConfig: Unable to open config file %s\n", 
	    filename);
    return 1;
  }

  fprintf(fp, "; DO NOT EDIT THIS FILE.\n");
  fprintf(fp, "; The file is generated automatically.\n");
  fprintf(fp, ";\n");

  for(i=0; i<ctx->NumParam; i++)
    fprintf(fp, "%s = %s\n", ctx->ConfigParam[i].type, 
	                     ctx->ConfigParam[i].value);

  fclose(fp);

  return 0;

}

/*------------------------------------------------------------------------*/
/*--	constructCTX						--*/
/*------------------------------------------------------------------------*/

extern PUBLIC CONF_CTX *constructCTX()
{
  CONF_CTX *ctx;

  if ( (ctx = (CONF_CTX *)malloc(sizeof(CONF_CTX))) == NULL) {
    fprintf(stderr, 
	      "constructCTX: Unable to allocate space for context\n");
    return NULL;
  }

  ctx->NumParam = 0;

  return ctx;
}

/*------------------------------------------------------------------------*/
/*--	destructCTX							--*/
/*------------------------------------------------------------------------*/

extern PUBLIC int destructCTX(CONF_CTX *ctx)
{
  int i;

  for(i=0; i<ctx->NumParam; i++) {
    free(ctx->ConfigParam[i].type);
    free(ctx->ConfigParam[i].value);
    if(ctx->ConfigParam[i].path)
      free(ctx->ConfigParam[i].path);
  }

  ctx->NumParam = 0;

  return 0;
}

